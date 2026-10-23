#!/usr/bin/env python3
"""Run one authored synthetic scenario against the OpenRepublic baseline autopilot.

Example:
  python tools/scenario_score.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe \
      --scenario data/scenarios/grain_shortage.json --ticks 500

Scenario setup is submitted to the same controller-enabled JSONL host used by
the autopilot. Setup consumes one tick; ``ticks`` is the subsequent autopilot
run length. Scores describe this synthetic OpenRepublic model only.
"""
import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import pathlib
import re
import sys


MAX_TICKS = 100_000
MAX_SETUP_COMMANDS = 512
SCENARIO_DIR = pathlib.Path(__file__).resolve().parents[1] / "data" / "scenarios"
AUTOPILOT_PATH = pathlib.Path(__file__).with_name("republic_autopilot.py")
_SPEC = importlib.util.spec_from_file_location("openrepublic_republic_autopilot", AUTOPILOT_PATH)
autopilot = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(autopilot)


class ScenarioError(ValueError):
    """A scenario file is malformed or exceeds the supported fixture bounds."""


def _object(value, name, required, optional=()):
    if not isinstance(value, dict):
        raise ScenarioError(f"{name} must be an object")
    missing = set(required) - value.keys()
    extra = value.keys() - set(required) - set(optional)
    if missing:
        raise ScenarioError(f"{name} is missing fields: {', '.join(sorted(missing))}")
    if extra:
        raise ScenarioError(f"{name} has unknown fields: {', '.join(sorted(extra))}")
    return value


def _integer(value, name, minimum, maximum):
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise ScenarioError(f"{name} must be an integer from {minimum} through {maximum}")
    return value


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ScenarioError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def load_scenario(path):
    """Load and validate a bounded version-1 JSON fixture transactionally."""
    path = pathlib.Path(path)
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as error:
        raise ScenarioError(f"cannot read scenario: {error}") from error
    try:
        scenario = json.loads(text, object_pairs_hook=_unique_object)
    except (json.JSONDecodeError, UnicodeError) as error:
        raise ScenarioError(f"invalid scenario JSON: {error}") from error

    _object(scenario, "scenario", ("schemaVersion", "id", "name", "description", "roads", "buildings", "score"))
    _integer(scenario["schemaVersion"], "schemaVersion", 1, 1)
    if not isinstance(scenario["id"], str) or not re.fullmatch(r"[a-z0-9]+(?:_[a-z0-9]+)*", scenario["id"]):
        raise ScenarioError("id must be a lowercase underscore-separated identifier")
    for field in ("name", "description"):
        if not isinstance(scenario[field], str) or not scenario[field].strip() or len(scenario[field]) > 512:
            raise ScenarioError(f"{field} must be non-empty text of at most 512 characters")
    if not isinstance(scenario["roads"], list) or not isinstance(scenario["buildings"], list):
        raise ScenarioError("roads and buildings must be arrays")
    if not scenario["buildings"]:
        raise ScenarioError("buildings must contain at least one initial building")
    if len(scenario["roads"]) + len(scenario["buildings"]) > MAX_SETUP_COMMANDS:
        raise ScenarioError(f"setup may contain at most {MAX_SETUP_COMMANDS} commands")

    for index, road in enumerate(scenario["roads"]):
        road = _object(road, f"roads[{index}]", ("a", "b"))
        for endpoint in ("a", "b"):
            cell = _object(road[endpoint], f"roads[{index}].{endpoint}", ("x", "z"))
            _integer(cell["x"], f"roads[{index}].{endpoint}.x", -100_000, 100_000)
            _integer(cell["z"], f"roads[{index}].{endpoint}.z", -100_000, 100_000)
        a, b = road["a"], road["b"]
        if a == b or (a["x"] != b["x"] and a["z"] != b["z"]):
            raise ScenarioError(f"roads[{index}] must be a non-zero straight horizontal or vertical segment")
        if abs(a["x"] - b["x"]) + abs(a["z"] - b["z"]) != 1:
            raise ScenarioError(f"roads[{index}] must be one orthogonally adjacent road edge")

    for index, building in enumerate(scenario["buildings"]):
        _object(building, f"buildings[{index}]", ("type", "x", "z"))
        if building["type"] not in ("farm", "mill", "bakery", "market"):
            raise ScenarioError(f"buildings[{index}].type is not in the built-in synthetic catalog")
        _integer(building["x"], f"buildings[{index}].x", 0, 100_000)
        _integer(building["z"], f"buildings[{index}].z", 1, 100_000)

    _object(scenario["score"], "score", ("metric",))
    if scenario["score"]["metric"] != "stored_non_raw_units":
        raise ScenarioError("score.metric must be stored_non_raw_units")
    return scenario


def _seed(host, scenario):
    """Queue fixture roads/buildings and apply them together at the next tick."""
    commands = [
        {"kind": "build_road", "ax": road["a"]["x"], "az": road["a"]["z"],
         "bx": road["b"]["x"], "bz": road["b"]["z"]}
        for road in scenario["roads"]
    ]
    commands.extend({"kind": "place_building", "type": building["type"],
                     "x": building["x"], "z": building["z"]}
                    for building in scenario["buildings"])
    revision = autopilot._world(host)["revision"]
    handles = []
    for command in commands:
        result = host.call("submit", revision=revision, command=command)
        if result.get("errorCode") != "none" or result.get("state") != "queued" or not result.get("handle"):
            raise RuntimeError(f"scenario setup command was not queued: {result}")
        handles.append(result["handle"])
    if handles:
        autopilot._step(host, 1)
    for handle in handles:
        state = autopilot._poll_terminal(host, handle)
        if state != "accepted":
            raise RuntimeError(f"scenario setup command {handle} was rejected")


def run_scenario(host_exe, scenario, ticks, host_args=()):
    """Run setup and one autopilot round; return a canonical, hashable score record."""
    if isinstance(ticks, bool) or not isinstance(ticks, int) or not 1 <= ticks <= MAX_TICKS:
        raise ValueError(f"ticks must be from 1 through {MAX_TICKS}")
    host = autopilot.Host(host_exe, list(host_args))
    try:
        catalog = host.call("catalog")
        autopilot._check_error(catalog, "catalog")
        _seed(host, scenario)
        with contextlib.redirect_stdout(io.StringIO()):
            result = autopilot.run_round(host, catalog, ticks, set(), round_number=1)
        world = result["world"]
        raw = autopilot.raw_resources(catalog)
        score_micro_units = sum(int(item["amount"])
                                for building in world["buildings"]
                                for item in building["inventory"] if item["resource"] not in raw)
        scenario_hash = hashlib.sha256(json.dumps(
            scenario, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")).hexdigest()
        stable = {
            "schemaVersion": 1,
            "scenarioId": scenario["id"],
            "scenarioHash": scenario_hash,
            "metric": scenario["score"]["metric"],
            "controllerTicks": ticks,
            "setupTicks": 1 if scenario["roads"] or scenario["buildings"] else 0,
            "tick": int(world["tick"]),
            "stateHash": str(world["stateHash"]),
            "scoreMicroUnits": score_micro_units,
            "scoreUnits": score_micro_units / 1_000_000,
            "buildings": len(world["buildings"]),
            "roads": len(world["roads"]),
            "orders": len(world["orders"]),
            "vehicles": len(world["vehicles"]),
            "autopilot": {key: result[key] for key in (
                "placements_accepted", "retargets_queued", "accepted_retargets", "rejected_retargets",
                "creates_queued", "accepted_creates", "rejected_creates")},
        }
        digest = hashlib.sha256(json.dumps(stable, sort_keys=True, separators=(",", ":"),
                                           ensure_ascii=False).encode("utf-8")).hexdigest()
        return {**stable, "scoreHash": digest}
    finally:
        try:
            host.close()
        finally:
            for stream in (host.proc.stdin, host.proc.stdout):
                if stream is not None and not stream.closed:
                    stream.close()


def tick_count(value):
    try:
        ticks = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("ticks must be an integer from 1 through 100000") from error
    if not 1 <= ticks <= MAX_TICKS:
        raise argparse.ArgumentTypeError("ticks must be from 1 through 100000")
    return ticks


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="OpenRepublicEconomyHost executable")
    parser.add_argument("--scenario", required=True, help="synthetic scenario JSON file")
    parser.add_argument("--ticks", required=True, type=tick_count,
                        help="autopilot simulation ticks after one setup tick")
    args = parser.parse_args(argv)
    try:
        scenario = load_scenario(args.scenario)
        score = run_scenario(args.host, scenario, args.ticks)
    except (ScenarioError, RuntimeError, OSError, ValueError) as error:
        print(f"scenario score failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(score, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
