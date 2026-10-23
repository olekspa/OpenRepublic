#!/usr/bin/env python3
"""Run controller entries on isolated fresh OpenRepublic host instances and tabulate scores.

Manifest format: {"worlds":[{"name":"fixture","host_args":[]}],
"controllers":[{"name":"autopilot","kind":"autopilot"},
{"name":"codex","kind":"llm","backend":"codex","prompt":"tools/prompts/llm_republic.md"}]}
Each world/controller pair receives a separate host process and an unchanged copy/source world.
The current host protocol has no republic owner IDs, so isolation is process/world based.
"""
import argparse
import concurrent.futures
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from llm_republic import Host
from republic_autopilot import processed_units, raw_resources
import scenario_score


def score_idle(host_exe, host_args, target_tick):
    host=Host(host_exe,host_args)
    try:
        catalog=host.call("catalog"); catalog=catalog.get("catalog",catalog)
        world=host.call("world")["world"]
        while int(world["tick"])<target_tick:
            count=min(100_000,target_tick-int(world["tick"]))
            result=host.call("step",ticks=count)
            if result.get("errorCode")!="none": raise RuntimeError(f"idle step failed: {result}")
            world=host.call("world")["world"]
        return processed_units(world,raw_resources(catalog))
    finally: host.close()


def execute_one(host_exe, world, controller, rounds, ticks, root):
    name=f"{world['name']} / {controller['name']}"
    host_args=world.get("host_args",[])
    transcript=str(root/(safe_name(world['name'])+"-"+safe_name(controller['name'])+".jsonl"))
    if controller["kind"]=="autopilot":
        command=[sys.executable,str(Path(__file__).with_name("republic_autopilot.py")),"--host",host_exe,
                 "--rounds",str(rounds),"--ticks",str(ticks),"--json","--",*host_args]
        result=subprocess.run(command,text=True,capture_output=True,timeout=7200,check=False)
        if result.returncode: raise RuntimeError(f"{name}: autopilot exit {result.returncode}: {result.stderr[-1000:]}")
        row=next((json.loads(line) for line in reversed(result.stdout.splitlines()) if line.startswith('{"benchmark"')),None)
        if row is None: raise RuntimeError(f"{name}: autopilot JSON result missing")
        target=int(row["tick"]); final=float(row["controller"]); idle=float(row["idle"])
    elif controller["kind"]=="llm":
        command=[sys.executable,str(Path(__file__).with_name("llm_republic.py")),"--host",host_exe,
                 "--backend",controller.get("backend","codex"),"--rounds",str(rounds),"--ticks",str(ticks),
                 "--transcript",transcript,"--",*host_args]
        if controller.get("prompt"): command[command.index("--transcript"):command.index("--transcript")]=["--prompt",str(Path(controller["prompt"]).resolve())]
        if controller.get("scripted_responses"):
            command[command.index("--transcript"):command.index("--transcript")]=["--scripted-responses",str(Path(controller["scripted_responses"]).resolve())]
        result=subprocess.run(command,text=True,capture_output=True,timeout=7200,check=False)
        if result.returncode: raise RuntimeError(f"{name}: LLM exit {result.returncode}: {result.stderr[-1000:]}")
        rows=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{"round"')]
        if not rows: raise RuntimeError(f"{name}: no round output")
        final=float(rows[-1]["score"]); target=int(rows[-1]["tick"])
        idle=score_idle(host_exe,host_args,target)
        errors=sum(1 for x in rows if x.get("backendError"))
        no_action=sum(1 for x in rows if not x.get("actions"))
        return {"world":world["name"],"controller":controller["name"],"kind":"llm","tick":target,
                "score":final,"idle":idle,"gain":final-idle,"backendErrors":errors,"emptyActionRounds":no_action,
                "transcript":transcript}
    else: raise ValueError(f"unknown controller kind: {controller['kind']}")
    return {"world":world["name"],"controller":controller["name"],"kind":"autopilot","tick":target,
            "score":final,"idle":idle,"gain":final-idle,"backendErrors":0,"emptyActionRounds":None}


def execute_scenario(host_exe, scenario_arg, scenario, controller, rounds, ticks, root):
    """Run a controller on the shared scenario fixture and scorer metric."""
    name=controller["name"]
    if controller.get("kind")=="autopilot":
        receipt=scenario_score.run_scenario(host_exe,scenario,rounds*ticks)
        return {"world":scenario["id"],"controller":name,"kind":"autopilot-baseline",
                "tick":receipt["tick"],"scoreMicroUnits":receipt["scoreMicroUnits"],
                "scoreUnits":receipt["scoreUnits"],"scoreHash":receipt["scoreHash"]}
    transcript=str(root/(safe_name(scenario["id"])+"-"+safe_name(name)+".jsonl"))
    command=[sys.executable,str(Path(__file__).with_name("llm_republic.py")),"--host",host_exe,
             "--scenario",scenario_arg,"--backend",controller.get("backend","codex"),
             "--rounds",str(rounds),"--ticks",str(ticks),"--transcript",transcript]
    if controller.get("prompt"):
        command.extend(["--prompt",str(Path(controller["prompt"]).resolve())])
    if controller.get("scripted_responses"):
        command.extend(["--scripted-responses",str(Path(controller["scripted_responses"]).resolve())])
    result=subprocess.run(command,text=True,capture_output=True,timeout=7200,check=False)
    if result.returncode:
        raise RuntimeError(f"{name}: LLM exit {result.returncode}: {result.stderr[-1000:]}")
    receipts=[json.loads(line)["scenarioScore"] for line in result.stdout.splitlines()
              if line.startswith('{') and '"scenarioScore"' in line]
    if not receipts: raise RuntimeError(f"{name}: scenario score JSON missing")
    receipt=receipts[-1]
    rows=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{') and '"round"' in line]
    return {"world":scenario["id"],"controller":name,"kind":"llm","tick":receipt["tick"],
            "scoreMicroUnits":receipt["scoreMicroUnits"],"scoreUnits":receipt["scoreUnits"],
            "scoreHash":receipt["scoreHash"],"backendErrors":sum(bool(x.get("backendError")) for x in rows),
            "emptyActionRounds":sum(not x.get("actions") for x in rows),"transcript":transcript}


def safe_name(value):
    return "".join(c if c.isalnum() or c in "-_" else "_" for c in value)[:80]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host",required=True); p.add_argument("--manifest",required=True)
    p.add_argument("--rounds",type=int,default=8); p.add_argument("--ticks",type=int,default=500)
    p.add_argument("--parallel",type=int,default=1); p.add_argument("--output-json"); p.add_argument("--scenario",help="synthetic scenario ID or JSON path")
    p.add_argument("--transcript-dir",default=str(Path(tempfile.gettempdir())/"openrepublic-tournament"))
    a=p.parse_args()
    if not 1<=a.rounds<=1000 or not 1<=a.ticks<=100000 or not 1<=a.parallel<=16: p.error("rounds/ticks/parallel outside bounds")
    with open(a.manifest,encoding="utf-8-sig") as f: manifest=json.load(f)
    worlds=manifest.get("worlds",[]); controllers=manifest.get("controllers",[])
    if not controllers: p.error("manifest needs at least one controller")
    scenario=None
    if a.scenario:
        try: scenario=scenario_score.resolve_scenario(a.scenario)
        except scenario_score.ScenarioError as e: p.error(str(e))
        if any(world.get("host_args",[]) for world in worlds): p.error("--scenario cannot be combined with world host_args")
        worlds=[{"name":scenario["id"],"host_args":[]}]
    elif not worlds: p.error("manifest needs at least one world")
    for world in worlds:
        if not isinstance(world.get("name"),str) or not isinstance(world.get("host_args",[]),list): p.error("invalid world entry")
    for controller in controllers:
        if controller.get("kind") not in ("llm","autopilot") or not isinstance(controller.get("name"),str): p.error("invalid controller entry")
        if controller.get("kind")=="llm" and controller.get("backend","codex") not in ("codex","openai-compatible","claude","scripted"): p.error("invalid LLM backend")
    root=Path(a.transcript_dir).resolve(); root.mkdir(parents=True,exist_ok=True)
    jobs=[(w,c) for w in worlds for c in controllers]; results=[]
    if scenario is not None and not any(c.get("kind") == "autopilot" for c in controllers):
        jobs.append((worlds[0],{"name":"autopilot-baseline","kind":"autopilot"}))
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.parallel) as pool:
        futures={pool.submit(execute_scenario,a.host,a.scenario,scenario,c,a.rounds,a.ticks,root)
                 if scenario is not None else pool.submit(execute_one,a.host,w,c,a.rounds,a.ticks,root):(w,c)
                 for w,c in jobs}
        for future in concurrent.futures.as_completed(futures):
            w,c=futures[future]
            try: results.append(future.result())
            except Exception as e: results.append({"world":w["name"],"controller":c["name"],"error":str(e)})
    results.sort(key=lambda r:(r["world"],r["controller"]))
    if scenario is not None:
        print("| Scenario | Controller | Tick | Score (stored non-raw units) | Empty rounds | Errors |")
        print("|---|---|---:|---:|---:|---:|")
        for r in results:
            if "error" in r: print(f"| {r['world']} | {r['controller']} | — | — | — | {r['error']} |")
            else: print(f"| {r['world']} | {r['controller']} | {r['tick']} | {r['scoreUnits']:.6f} | {r.get('emptyActionRounds','—')} | {r.get('backendErrors',0)} |")
    else:
        print("| World | Controller | Tick | Score | Idle | Gain | Empty rounds | Errors |")
        print("|---|---|---:|---:|---:|---:|---:|---:|")
    if scenario is None:
        for r in results:
            if "error" in r: print(f"| {r['world']} | {r['controller']} | — | — | — | — | — | {r['error']} |")
            else: print(f"| {r['world']} | {r['controller']} | {r['tick']} | {r['score']:.3f} | {r['idle']:.3f} | {r['gain']:.3f} | {r['emptyActionRounds'] if r['emptyActionRounds'] is not None else '—'} | {r['backendErrors']} |")
    if a.output_json:
        with open(a.output_json,"w",encoding="utf-8") as f: json.dump({"rounds":a.rounds,"ticksPerRound":a.ticks,"scenarioId":scenario["id"] if scenario else None,"scenarioMetric":scenario["score"]["metric"] if scenario else None,"results":results},f,indent=2)
    return 1 if any("error" in r for r in results) else 0

if __name__=="__main__": sys.exit(main())
