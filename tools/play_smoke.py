#!/usr/bin/env python3
"""Run the baseline autopilot against the synthetic fixtures backing play modes.

This headless harness checks scenario score milestones, not the desktop renderer
or WRSR parity. Run after building OpenRepublicEconomyHost.
"""
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PRESETS = {
    "coal": ("coal_to_heat_logistics.json", 1500),
    "sites": ("build_two_mills_with_supply.json", 1500),
    "town": ("thriving_town.json", 500),
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="OpenRepublicEconomyHost executable")
    parser.add_argument("--ticks", type=int, help="override each preset's default tick horizon")
    args = parser.parse_args()
    if args.ticks is not None and not 1 <= args.ticks <= 100000:
        parser.error("--ticks must be from 1 through 100000")
    for preset, (scenario_name, default_ticks) in PRESETS.items():
        scenario = ROOT / "data" / "scenarios" / scenario_name
        objective_path = ROOT / "data" / "objectives" / {
            "coal": "coal_demo.json", "sites": "construction_sites_demo.json", "town": "growth_town.json"
        }[preset]
        objectives = json.loads(objective_path.read_text(encoding="utf-8"))["objectives"]
        command = [sys.executable, str(ROOT / "tools" / "scenario_score.py"), "--host", args.host,
                   "--scenario", str(scenario), "--ticks", str(args.ticks or default_ticks)]
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        score = json.loads(result.stdout)
        breakdown = score.get("scoreBreakdown", {})
        print(f"[{preset}] scenario={scenario_name} ticks={score.get('controllerTicks', args.ticks or default_ticks)} score={score.get('scoreUnits', score.get('scoreMicroUnits'))}")
        for objective in objectives:
            oid = objective["id"]
            if preset == "coal":
                if oid != "produce_heat":
                    print(f"  {oid}: not measured by scenario_score.py objective telemetry")
                    continue
                field, target = "heatUnits", 100
                value = breakdown.get(field, 0)
            elif preset == "sites":
                field, target = ("totalSites", 2) if oid == "place_sites" else ("completedSites", 2)
                value = breakdown.get(field, 0)
            else:
                if oid == "housing":
                    print(f"  {oid}: not measured by scenario_score.py objective telemetry")
                    continue
                field, target = "population", 100
                value = breakdown.get(field, 0)
            state = "complete" if value >= target else "incomplete"
            print(f"  {oid}: {state} ({value}/{target}; proxy={field})")
        print(f"  source=OBSERVED OpenRepublic synthetic host score; WRSR parity UNKNOWN")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.CalledProcessError, ValueError, KeyError) as error:
        print(f"play smoke failed: {error}", file=sys.stderr)
        raise SystemExit(1)
