#!/usr/bin/env python3
"""Run the ordered synthetic campaign scenarios through the baseline autopilot.

Each mission's par score is interpreted as its authored controller-tick budget.
The report distinguishes objectives the scenario scorer can measure from
objective metrics that are absent from its final score receipt.
"""
import argparse
import contextlib
import importlib.util
import io
import json
import pathlib
import re
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
CAMPAIGN_DIR = ROOT / "data" / "campaign" / "republic_rising"
SCENARIO_SCORE = ROOT / "tools" / "scenario_score.py"
SAFE_ID = re.compile(r"[a-z0-9]+(?:_[a-z0-9]+)*\Z")
_SCORE_SPEC = importlib.util.spec_from_file_location("campaign_scenario_score", SCENARIO_SCORE)
scenario_score = importlib.util.module_from_spec(_SCORE_SPEC)
_SCORE_SPEC.loader.exec_module(scenario_score)


class CampaignSmokeError(ValueError):
    pass


def _read_campaign(path):
    """Read the bounded line-oriented campaign manifest used by the runtime."""
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "OPENREPUBLIC_CAMPAIGN 1":
        raise CampaignSmokeError(f"{path}: expected OPENREPUBLIC_CAMPAIGN 1 header")
    fields = {}
    missions = []
    for number, line in enumerate(lines[1:], start=2):
        if not line or line.startswith("#"):
            continue
        if line.startswith("mission="):
            parts = line[len("mission="):].split("|")
            if len(parts) not in (6, 7):
                raise CampaignSmokeError(f"{path}:{number}: mission requires six or seven pipe-separated fields")
            mission_id, title, briefing, scenario, objectives, par_score = parts[:6]
            starting_treasury = parts[6] if len(parts) == 7 else "0"
            if not SAFE_ID.fullmatch(mission_id):
                raise CampaignSmokeError(f"{path}:{number}: invalid mission ID {mission_id!r}")
            if any(not item or pathlib.Path(item).name != item or item in (".", "..")
                   for item in (briefing, scenario, objectives)):
                raise CampaignSmokeError(f"{path}:{number}: referenced files must be basenames")
            try:
                par_score = int(par_score)
            except ValueError as error:
                raise CampaignSmokeError(f"{path}:{number}: par score must be an integer") from error
            if not 1 <= par_score <= 100_000:
                raise CampaignSmokeError(f"{path}:{number}: par score must be 1..100000 ticks")
            try:
                starting_treasury = int(starting_treasury)
            except ValueError as error:
                raise CampaignSmokeError(f"{path}:{number}: starting treasury must be an integer") from error
            if not 0 <= starting_treasury <= 18_446_744_073_709_551_615:
                raise CampaignSmokeError(f"{path}:{number}: starting treasury must be an unsigned 64-bit integer")
            missions.append({"id": mission_id, "title": title, "briefingFile": briefing,
                             "scenarioFile": scenario, "objectivesFile": objectives,
                             "parScoreTicks": par_score, "startingTreasury": starting_treasury})
        elif "=" in line:
            key, value = line.split("=", 1)
            if key not in ("id", "title") or not value.strip() or key in fields:
                raise CampaignSmokeError(f"{path}:{number}: unsupported or duplicate field {key!r}")
            fields[key] = value.strip()
        else:
            raise CampaignSmokeError(f"{path}:{number}: invalid manifest line")
    if not SAFE_ID.fullmatch(fields.get("id", "")) or not fields.get("title"):
        raise CampaignSmokeError(f"{path}: id and title are required")
    if not missions or len({mission["id"] for mission in missions}) != len(missions):
        raise CampaignSmokeError(f"{path}: missions must be non-empty and IDs unique")
    for mission in missions:
        for key in ("briefingFile", "scenarioFile", "objectivesFile"):
            mission[key] = path.parent / mission[key]
            if not mission[key].is_file():
                raise CampaignSmokeError(f"{path}: missing {key} {mission[key].name}")
    return {"id": fields["id"], "title": fields["title"], "missions": missions}


def _objective_value(objective, world, score):
    """Evaluate the existing native objective metrics over the final host snapshot."""
    metric = objective.get("metric")
    buildings = world.get("buildings", [])
    kind = objective.get("building_type", "")
    matching = [building for building in buildings
                if not kind or building.get("type") == kind]
    if metric == "inventory_at_least":
        resource = objective.get("resource")
        amount = sum(int(item.get("amount", 0)) for building in matching
                     for item in building.get("inventory", [])
                     if item.get("resource") == resource)
        return amount, "finalSnapshotInventory"
    if metric == "delivered_at_least":
        # Match the native evaluator's documented current-stock proxy; the
        # snapshot has no durable delivery history.
        resource = objective.get("resource")
        amount = sum(int(item.get("amount", 0)) for building in matching
                     for item in building.get("inventory", [])
                     if item.get("resource") == resource)
        return amount, "finalDestinationStockProxy"
    if metric == "construction_sites_count":
        return sum(1 for building in matching if building.get("constructionSite")), "finalSnapshotConstructionSites"
    if metric == "construction_sites_complete":
        # Completed sites are promoted to ordinary buildings in the snapshot;
        # the scenario receipt retains the completed-site outcome explicitly.
        states = score.get("scoreBreakdown", {}).get("siteStates", [])
        return sum(1 for item in states if item.get("state") == "done" and
                   (not kind or item.get("type") == kind)), "scenarioScoreCompletedSites"
    if metric == "population_at_least":
        return int(world.get("population", 0)), "finalSnapshotPopulation"
    if metric == "building_count_at_least":
        return len(matching), "finalSnapshotBuildingCount"
    return None, "unsupported objective metric"


def _score_metric_value(score):
    metric = score.get("metric")
    breakdown = score.get("scoreBreakdown", {})
    fields = {
        "heat_units": "heatUnits",
        "completed_construction_sites": "completedSites",
        "happy_residents": "happyResidents",
        "utility_multi_objective": "throughputUnits",
    }
    field = fields.get(metric)
    if field is None:
        return None, None
    return breakdown.get(field), field


def _run_scenario(host_exe, scenario_path, ticks):
    """Run the shared baseline autopilot and retain its final world snapshot."""
    scenario = scenario_score.load_scenario(scenario_path)
    if scenario.get("controllerScript"):
        raise CampaignSmokeError(f"{scenario_path.name}: controllerScript is not a baseline-autopilot mission")
    if scenario["score"]["metric"] == "rail_vs_road":
        raise CampaignSmokeError(f"{scenario_path.name}: rail_vs_road needs the dedicated comparison scorer")
    temporary = tempfile.TemporaryDirectory(prefix="openrepublic-campaign-") if "catalog" in scenario else None
    args = []
    if temporary:
        catalog_path = pathlib.Path(temporary.name) / "scenario.catalog"
        catalog_path.write_text(scenario["catalog"], encoding="utf-8")
        args.extend(("--catalog", str(catalog_path)))
    host = None
    try:
        host = scenario_score.autopilot.Host(str(host_exe), args)
        catalog = host.call("catalog")
        scenario_score.autopilot._check_error(catalog, "catalog")
        scenario_score._seed(host, scenario)
        start_tick = int(scenario_score.autopilot._world(host)["tick"])
        rounds = scenario_score._default_rounds(scenario, ticks)
        per_round = ticks // rounds
        placed = set()
        totals = {"placements_accepted": 0, "retargets_queued": 0, "accepted_retargets": 0,
                  "rejected_retargets": 0, "creates_queued": 0, "accepted_creates": 0,
                  "rejected_creates": 0, "depot_dispatches_queued": 0,
                  "accepted_depot_dispatches": 0, "rejected_depot_dispatches": 0}
        if scenario["score"]["metric"] == "combined_multi_objective":
            totals["trade_queued"] = 0
        with contextlib.redirect_stdout(io.StringIO()):
            for round_number in range(1, rounds + 1):
                result = scenario_score.autopilot.run_round(
                    host, catalog, per_round, placed, round_number=round_number)
                for field in totals:
                    totals[field] += result[field]
        world = result["world"] if rounds else scenario_score.autopilot._world(host)
        summary = scenario_score.score_context(host, scenario, world)
        score = scenario_score.score_record(scenario, world, catalog, ticks, summary)
        if score.get("scoreBreakdown", {}).get("population") is not None:
            world["population"] = score["scoreBreakdown"]["population"]
        score["autopilot"] = totals
        return score, world
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


def run(host, campaign, ticks_override=None):
    missions = []
    for mission in campaign["missions"]:
        ticks = ticks_override or mission["parScoreTicks"]
        score, world = _run_scenario(host, mission["scenarioFile"], ticks)
        objectives_doc = json.loads(mission["objectivesFile"].read_text(encoding="utf-8"))
        objectives = []
        all_measured_pass = True
        for objective in objectives_doc.get("objectives", []):
            value, source = _objective_value(objective, world, score)
            target = objective.get("target")
            state = "not_measured" if value is None else ("reachable" if value >= target else "not_reached")
            if state != "reachable":
                all_measured_pass = False
            objectives.append({"id": objective.get("id"), "metric": objective.get("metric"),
                               "value": value, "target": target, "status": state, "evidence": source})
        value, field = _score_metric_value(score)
        missions.append({"id": mission["id"], "title": mission["title"],
                         "briefingFile": mission["briefingFile"].name,
                         "scenarioId": score.get("scenarioId"), "ticks": ticks,
                         "parScoreTicks": mission["parScoreTicks"],
                         "startingTreasury": mission["startingTreasury"],
                         "scenarioMetric": score.get("metric"),
                         "scenarioMetricValue": value, "scenarioMetricField": field,
                         "scoreBreakdown": score.get("scoreBreakdown", {}),
                         "objectiveReachabilityMeasured": all_measured_pass and all(
                             item["status"] != "not_measured" for item in objectives),
                         "objectives": objectives,
                         "stateHash": score.get("stateHash"), "scoreHash": score.get("scoreHash")})
    return {"schemaVersion": 1, "campaignId": campaign["id"], "campaignTitle": campaign["title"],
            "evidence": "OBSERVED OpenRepublic synthetic baseline autopilot run; WRSR parity UNKNOWN",
            "missions": missions,
            "allMeasuredObjectivesReachable": all(
                item["status"] == "reachable" for mission in missions for item in mission["objectives"]
                if item["status"] != "not_measured"),
            "hasUnmeasuredObjectives": any(item["status"] == "not_measured"
                                             for mission in missions for item in mission["objectives"])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="OpenRepublicEconomyHost executable")
    parser.add_argument("--campaign", default=str(CAMPAIGN_DIR / "campaign.txt"),
                        help="campaign manifest (default: republic_rising)")
    parser.add_argument("--ticks", type=int, help="override every mission's authored par tick budget")
    args = parser.parse_args()
    if args.ticks is not None and not 1 <= args.ticks <= 100_000:
        parser.error("--ticks must be from 1 through 100000")
    report = run(pathlib.Path(args.host).resolve(), _read_campaign(pathlib.Path(args.campaign)), args.ticks)
    print(json.dumps(report, sort_keys=True, indent=2))
    return 0 if report["allMeasuredObjectivesReachable"] and not report["hasUnmeasuredObjectives"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (CampaignSmokeError, OSError, ValueError, KeyError, RuntimeError) as error:
        print(f"campaign smoke failed: {error}", file=sys.stderr)
        raise SystemExit(2)
