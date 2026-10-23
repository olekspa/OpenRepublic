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
import tempfile


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

    _object(scenario, "scenario", ("schemaVersion", "id", "name", "description", "roads", "buildings", "score"),
            ("catalog", "utilityConnections"))
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

    building_types = {"farm", "mill", "bakery", "market"}
    if "catalog" in scenario:
        if not isinstance(scenario["catalog"], str) or not scenario["catalog"].strip() or len(scenario["catalog"]) > 16_384:
            raise ScenarioError("catalog must be non-empty text of at most 16384 characters")
        building_types = set(re.findall(r"(?m)^building\s+([a-z][a-z0-9_]*)\s+", scenario["catalog"]))
        if not building_types or not re.search(r"(?m)^utility\s+power\s*$", scenario["catalog"]):
            raise ScenarioError("catalog must declare utility power and at least one building")
    for index, building in enumerate(scenario["buildings"]):
        _object(building, f"buildings[{index}]", ("type", "x", "z"))
        if building["type"] not in building_types:
            raise ScenarioError(f"buildings[{index}].type is not in the scenario catalog")
        _integer(building["x"], f"buildings[{index}].x", 0, 100_000)
        _integer(building["z"], f"buildings[{index}].z", 1, 100_000)

    connections = scenario.get("utilityConnections", [])
    if not isinstance(connections, list) or len(connections) > MAX_SETUP_COMMANDS:
        raise ScenarioError("utilityConnections must be an array of at most 512 links")
    seen_connections = set()
    for index, link in enumerate(connections):
        _object(link, f"utilityConnections[{index}]", ("resource", "aBuilding", "bBuilding"))
        if link["resource"] != "power":
            raise ScenarioError(f"utilityConnections[{index}].resource must be power")
        a = _integer(link["aBuilding"], f"utilityConnections[{index}].aBuilding", 0, len(scenario["buildings"]) - 1)
        b = _integer(link["bBuilding"], f"utilityConnections[{index}].bBuilding", 0, len(scenario["buildings"]) - 1)
        if a == b:
            raise ScenarioError(f"utilityConnections[{index}] endpoints must differ")
        edge = (link["resource"], min(a, b), max(a, b))
        if edge in seen_connections:
            raise ScenarioError(f"utilityConnections[{index}] duplicates an earlier link")
        seen_connections.add(edge)
    if len(scenario["roads"]) + len(scenario["buildings"]) + len(connections) > MAX_SETUP_COMMANDS:
        raise ScenarioError(f"setup may contain at most {MAX_SETUP_COMMANDS} commands")
    _object(scenario["score"], "score", ("metric",))
    metric = scenario["score"]["metric"]
    if metric not in ("stored_non_raw_units", "utility_multi_objective"):
        raise ScenarioError("score.metric must be stored_non_raw_units or utility_multi_objective")
    if metric == "utility_multi_objective" and "catalog" not in scenario:
        raise ScenarioError("utility_multi_objective scenarios must include a synthetic catalog")
    return scenario


def resolve_scenario(name_or_path):
    """Resolve an explicit JSON path or an exact shipped scenario ID/stem."""
    candidate = pathlib.Path(name_or_path)
    if candidate.is_file():
        return load_scenario(candidate)
    if candidate.parent != pathlib.Path(".") or candidate.suffix:
        raise ScenarioError(f"scenario path does not exist: {name_or_path}")
    matches = [path for path in SCENARIO_DIR.glob("*.json")
               if path.stem == name_or_path]
    if not matches:
        raise ScenarioError(f"unknown scenario name or path: {name_or_path}")
    return load_scenario(matches[0])


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
    links = scenario.get("utilityConnections", [])
    if links:
        world = autopilot._world(host)
        ids = [int(building["id"]) for building in world["buildings"][-len(scenario["buildings"]):]]
        revision = world["revision"]
        link_handles = []
        for link in links:
            result = host.call("submit", revision=revision, command={
                "kind": "connect_utility", "resource": link["resource"],
                "a": ids[link["aBuilding"]], "b": ids[link["bBuilding"]]})
            if result.get("errorCode") != "none" or result.get("state") != "queued" or not result.get("handle"):
                raise RuntimeError(f"scenario utility link was not queued: {result}")
            link_handles.append(result["handle"])
        autopilot._step(host, 1)
        for handle in link_handles:
            if autopilot._poll_terminal(host, handle) != "accepted":
                raise RuntimeError(f"scenario utility link {handle} was rejected")


def seed_scenario(host, scenario):
    """Apply the common fixture setup to an already-running fresh host."""
    _seed(host, scenario)


def score_record(scenario, world, catalog, controller_ticks):
    """Build the shared deterministic scenario metric receipt for a final world."""
    raw = autopilot.raw_resources(catalog)
    score_micro_units = sum(int(item["amount"])
                            for building in world["buildings"]
                            for item in building["inventory"] if item["resource"] not in raw)
    breakdown = None
    if scenario["score"]["metric"] == "utility_multi_objective":
        diagnostics = world.get("utilityDiagnostics")
        if diagnostics is None:
            raise RuntimeError("utility diagnostics are required for utility_multi_objective scoring")
        networks = diagnostics.get("networks", [])
        requested = sum(int(network["requested"]) for network in networks if network["resource"] == "power")
        consumed = sum(int(network["consumed"]) for network in networks if network["resource"] == "power")
        breakdown = {
            "throughputMicroUnits": consumed,
            "unmetDemandMicroUnits": max(0, requested - consumed),
            "costUnits": len(world["buildings"]) + len(world["roads"]) + len(world.get("utilityConnections", [])),
            "costDefinition": "final building + road + utility-link count",
        }
    scenario_hash = hashlib.sha256(json.dumps(
        scenario, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")).hexdigest()
    stable = {
        "schemaVersion": 1, "scenarioId": scenario["id"], "scenarioHash": scenario_hash,
        "metric": scenario["score"]["metric"], "controllerTicks": controller_ticks,
        "setupTicks": (1 if scenario["roads"] or scenario["buildings"] else 0) +
                      (1 if scenario.get("utilityConnections") else 0),
        "tick": int(world["tick"]), "stateHash": str(world["stateHash"]),
        "scoreMicroUnits": score_micro_units, "scoreUnits": score_micro_units / 1_000_000,
        "buildings": len(world["buildings"]), "roads": len(world["roads"]),
        "orders": len(world["orders"]), "vehicles": len(world["vehicles"]),
    }
    if breakdown is not None:
        stable["scoreBreakdown"] = breakdown
        stable.pop("scoreMicroUnits")
        stable.pop("scoreUnits")
    digest = hashlib.sha256(json.dumps(stable, sort_keys=True, separators=(",", ":"),
                                       ensure_ascii=False).encode("utf-8")).hexdigest()
    return {**stable, "scoreHash": digest}


def run_scenario(host_exe, scenario, ticks, host_args=()):
    """Run setup and one autopilot round; return a canonical, hashable score record."""
    if isinstance(ticks, bool) or not isinstance(ticks, int) or not 1 <= ticks <= MAX_TICKS:
        raise ValueError(f"ticks must be from 1 through {MAX_TICKS}")
    temporary = tempfile.TemporaryDirectory(prefix="openrepublic-scenario-") if "catalog" in scenario else None
    args = list(host_args)
    if temporary:
        catalog_path = pathlib.Path(temporary.name) / "scenario.catalog"
        catalog_path.write_text(scenario["catalog"], encoding="utf-8")
        args.extend(("--catalog", str(catalog_path)))
    host = None
    try:
        host = autopilot.Host(host_exe, args)
        catalog = host.call("catalog")
        autopilot._check_error(catalog, "catalog")
        _seed(host, scenario)
        with contextlib.redirect_stdout(io.StringIO()):
            result = autopilot.run_round(host, catalog, ticks, set(), round_number=1)
        world = result["world"]
        if scenario["score"]["metric"] == "utility_multi_objective":
            diagnostics = host.call("utility_diagnostics", revision=world["revision"], offset=0, limit=128)
            autopilot._check_error(diagnostics, "utility_diagnostics")
            if not diagnostics.get("available"):
                raise RuntimeError("utility diagnostics are unavailable for the final scenario tick")
            networks = list(diagnostics.get("networks", []))
            while len(networks) < int(diagnostics.get("total", len(networks))):
                page = host.call("utility_diagnostics", revision=world["revision"],
                                 offset=len(networks), limit=128)
                autopilot._check_error(page, "utility_diagnostics")
                networks.extend(page.get("networks", []))
            world["utilityDiagnostics"] = {"available": True, "networks": networks}
        stable = score_record(scenario, world, catalog, ticks)
        stable = {key: value for key, value in stable.items() if key != "scoreHash"}
        stable.update({
            "autopilot": {key: result[key] for key in (
                "placements_accepted", "retargets_queued", "accepted_retargets", "rejected_retargets",
                "creates_queued", "accepted_creates", "rejected_creates")},
        })
        digest = hashlib.sha256(json.dumps(stable, sort_keys=True, separators=(",", ":"),
                                           ensure_ascii=False).encode("utf-8")).hexdigest()
        return {**stable, "scoreHash": digest}
    finally:
        if host is not None:
            try:
                host.close()
            finally:
                for stream in (host.proc.stdin, host.proc.stdout):
                    if stream is not None and not stream.closed:
                        stream.close()
        if temporary:
            temporary.cleanup()


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
