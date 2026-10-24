import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "scenario_score.py"
SPEC = importlib.util.spec_from_file_location("scenario_score", SCRIPT)
scenario_score = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scenario_score)
HOST_EXE = sys.argv[1] if len(sys.argv) > 1 else None


class ScenarioLoaderTests(unittest.TestCase):
    def test_resolves_exact_name_and_existing_path(self):
        named = scenario_score.resolve_scenario("full_chain")
        pathed = scenario_score.resolve_scenario(str(scenario_score.SCENARIO_DIR / "full_chain.json"))
        self.assertEqual(named["id"], "full_chain")
        self.assertEqual(named, pathed)
        with self.assertRaisesRegex(scenario_score.ScenarioError, "scenario path does not exist"):
            scenario_score.resolve_scenario("../full_chain")

    def test_shipped_scenarios_load_as_version_one(self):
        scenarios = sorted(scenario_score.SCENARIO_DIR.glob("*.json"))
        self.assertGreaterEqual(len(scenarios), 3)
        for path in scenarios:
            with self.subTest(path=path.name):
                scenario = scenario_score.load_scenario(path)
                self.assertEqual(scenario["schemaVersion"], 1)
                self.assertEqual(scenario["id"], path.stem)

    def test_rejects_duplicate_keys_and_unknown_fields(self):
        duplicate = '{"schemaVersion":1,"schemaVersion":1}'
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "duplicate.json"
            path.write_text(duplicate, encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "duplicate JSON"):
                scenario_score.load_scenario(path)

            unknown = {
                "schemaVersion": 1, "id": "bad", "name": "Bad", "description": "bad",
                "roads": [], "buildings": [{"type": "farm", "x": 1, "z": 1}],
                "score": {"metric": "stored_non_raw_units"}, "seed": 12,
            }
            path.write_text(json.dumps(unknown), encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "unknown fields"):
                scenario_score.load_scenario(path)

    def test_rejects_invalid_road_and_boolean_coordinate(self):
        scenario = {
            "schemaVersion": 1, "id": "bad", "name": "Bad", "description": "bad",
            "roads": [{"a": {"x": 1, "z": 1}, "b": {"x": 2, "z": 2}}],
            "buildings": [{"type": "farm", "x": True, "z": 1}],
            "score": {"metric": "stored_non_raw_units"},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "bad.json"
            path.write_text(json.dumps(scenario), encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "straight"):
                scenario_score.load_scenario(path)
            scenario["roads"] = []
            path.write_text(json.dumps(scenario), encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "integer"):
                scenario_score.load_scenario(path)

    def test_rejects_non_adjacent_road_span(self):
        scenario = {
            "schemaVersion": 1, "id": "bad", "name": "Bad", "description": "bad",
            "roads": [{"a": {"x": 1, "z": 1}, "b": {"x": 3, "z": 1}}],
            "buildings": [{"type": "farm", "x": 1, "z": 2}],
            "score": {"metric": "stored_non_raw_units"},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "bad.json"
            path.write_text(json.dumps(scenario), encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "adjacent"):
                scenario_score.load_scenario(path)

    def test_utility_fixtures_declare_valid_catalog_and_index_links(self):
        for name in ("power_deficit", "unconnected_consumer", "overbuilt_generator"):
            scenario = scenario_score.resolve_scenario(name)
            self.assertEqual(scenario["score"]["metric"], "utility_multi_objective")
            self.assertIn("utility power", scenario["catalog"])
        invalid = scenario_score.resolve_scenario("overbuilt_generator")
        invalid["utilityConnections"] = [{"resource": "power", "aBuilding": 0, "bBuilding": 0}]
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "bad.json"
            path.write_text(json.dumps(invalid), encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "endpoints must differ"):
                scenario_score.load_scenario(path)

    def test_utility_score_breakdown_and_hash_are_canonical(self):
        scenario = scenario_score.resolve_scenario("power_deficit")
        world = {"tick": 4, "stateHash": "state", "buildings": [{"inventory": []}] * 3,
                 "roads": [], "orders": [], "vehicles": [], "utilityConnections": [{}, {}],
                 "utilityDiagnostics": {"networks": [{"resource": "power", "requested": "4",
                                                          "consumed": "2"}]}}
        first = scenario_score.score_record(scenario, world, {"buildingTypes": []}, 3)
        second = scenario_score.score_record(scenario, world, {"buildingTypes": []}, 3)
        self.assertEqual(first, second)
        self.assertEqual(first["scoreBreakdown"]["throughputUnits"], 2)
        self.assertEqual(first["scoreBreakdown"]["unmetDemandUnits"], 2)
        self.assertEqual(first["scoreBreakdown"]["costUnits"], 5)
        self.assertNotIn("treasury", first["scoreBreakdown"])
        self.assertEqual(first["scoreHash"], second["scoreHash"])

    def test_combined_fixtures_load_with_declared_objectives(self):
        expected = {
            "growth_town": ["fedResidents", "completedSites", "treasury"],
            "power_and_build": ["completedSites", "throughputUnits", "unmetDemandUnits"],
            "austerity": ["completedSites", "treasury", "storedNonRawUnits"],
        }
        for name, objectives in expected.items():
            scenario = scenario_score.resolve_scenario(name)
            self.assertEqual(scenario["score"]["metric"], "combined_multi_objective")
            self.assertEqual(scenario["score"]["objectives"], objectives)
        malformed = scenario_score.resolve_scenario("growth_town")
        malformed["score"]["objectives"] = [{"unsupported": True}]
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "bad-objectives.json"
            path.write_text(json.dumps(malformed), encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "objectives list"):
                scenario_score.load_scenario(path)

    def test_rail_vs_road_fixture_declares_two_networks_and_1000_unit_receipt(self):
        scenario = scenario_score.resolve_scenario("rail_vs_road")
        self.assertEqual(len(scenario["roads"]), 20)
        self.assertEqual(len(scenario["rails"]), 20)
        self.assertEqual(scenario["score"]["metric"], "rail_vs_road")
        self.assertEqual(scenario["score"]["amount"], 1000)
        self.assertIn("station", scenario["catalog"])
        self.assertIn("vehicle train capacity 1000 speed 4", scenario["catalog"])

    def test_rail_fixture_rejects_non_adjacent_rail_edge(self):
        scenario = scenario_score.resolve_scenario("rail_vs_road")
        scenario["rails"][0]["b"] = {"x": 3, "z": 7}
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "bad_rail.json"
            path.write_text(json.dumps(scenario), encoding="utf-8")
            with self.assertRaisesRegex(scenario_score.ScenarioError, "adjacent rail edge"):
                scenario_score.load_scenario(path)

    def test_combined_score_receipt_is_multi_objective_and_deterministic(self):
        scenario = scenario_score.resolve_scenario("growth_town")
        world = {"tick": 10, "stateHash": "stable-state", "treasury": "7", "buildings": [
            {"id": 1, "type": "market", "cell": {"x": 11, "z": 2}, "constructionSite": False,
             "constructionState": "done", "constructionProgress": 1.0, "inventory": []}],
            "roads": [], "orders": [], "vehicles": []}
        summary = {"errorCode": "none", "population": "4"}
        first = scenario_score.score_record(scenario, world, {"buildingTypes": []}, 64, summary)
        second = scenario_score.score_record(scenario, world, {"buildingTypes": []}, 64, summary)
        self.assertEqual(first["scoreHash"], second["scoreHash"])
        self.assertEqual(first["scoreBreakdown"]["fedResidents"], 4)
        self.assertEqual(first["scoreBreakdown"]["completedSites"], 1)
        self.assertEqual(first["scoreBreakdown"]["treasury"], 7)
        self.assertNotIn("scoreUnits", first)

    def test_treasury_is_reported_only_for_trade_declaring_scenarios(self):
        scenario = scenario_score.resolve_scenario("grain_export_for_power")
        world = {"tick": 4, "stateHash": "state", "treasury": "17", "buildings": [],
                 "roads": [], "orders": [], "vehicles": [], "utilityConnections": [],
                 "utilityDiagnostics": {"networks": []}}
        record = scenario_score.score_record(scenario, world,
            {"resources": [{"name": "coal", "tradeDeclared": True}], "buildingTypes": []}, 3)
        self.assertEqual(record["scoreBreakdown"]["treasury"], 17)
        self.assertIn("scoreUnits", record)

    def test_construction_score_counts_completed_sites_and_hashes_final_states(self):
        scenario = scenario_score.resolve_scenario("build_two_mills_with_supply")
        world = {"tick": "20", "stateHash": "stable-state", "buildings": [
            {"id": 1, "type": "farm", "cell": {"x": 1, "z": 2}, "inventory": []},
            {"id": 2, "type": "sawmill", "cell": {"x": 3, "z": 2}, "inventory": []},
            {"id": 3, "type": "mill", "cell": {"x": 5, "z": 2}, "constructionSite": False, "constructionState": "done", "constructionProgress": 1.0, "inventory": []},
            {"id": 4, "type": "mill", "cell": {"x": 7, "z": 2}, "constructionSite": True, "constructionState": "building", "constructionProgress": 0.5, "inventory": []}],
            "roads": [], "orders": [], "vehicles": []}
        first = scenario_score.score_record(scenario, world, {"buildingTypes": []}, 16)
        second = scenario_score.score_record(scenario, world, {"buildingTypes": []}, 16)
        self.assertEqual(first, second)
        self.assertEqual(first["scoreUnits"], 1)
        self.assertEqual(first["scoreBreakdown"]["completedSites"], 1)
        self.assertEqual(first["scoreBreakdown"]["totalSites"], 2)
        self.assertEqual(first["scoreBreakdown"]["siteStates"][1]["state"], "building")
        self.assertEqual(first["scoreHash"], second["scoreHash"])


class ScenarioHostIntegrationTests(unittest.TestCase):
    @unittest.skipUnless(HOST_EXE, "host executable argument is required")
    def test_coal_to_heat_script_uses_two_owned_trucks_and_scores_heat(self):
        scenario = scenario_score.resolve_scenario("coal_to_heat")
        first = scenario_score.run_scenario(HOST_EXE, scenario, 120)
        repeated = scenario_score.run_scenario(HOST_EXE, scenario, 120)
        self.assertEqual(first["scoreBreakdown"]["heatUnits"], 4)
        self.assertEqual(len(first["scoreBreakdown"]["fleetStates"]), 2)
        self.assertTrue(all(vehicle["parked"] for vehicle in first["scoreBreakdown"]["fleetStates"]))
        self.assertEqual(first["scoreHash"], repeated["scoreHash"])

    @unittest.skipUnless(HOST_EXE, "host executable argument is required")
    def test_live_jsonl_host_runs_every_fixture_and_repeats_hash(self):
        scenarios = [scenario_score.load_scenario(path)
                     for path in sorted(scenario_score.SCENARIO_DIR.glob("*.json"))]
        first_runs = []
        for scenario in scenarios:
            with self.subTest(scenario=scenario["id"]):
                result = scenario_score.run_scenario(HOST_EXE, scenario, 16)
                self.assertEqual(result["scenarioId"], scenario["id"])
                self.assertEqual(len(result["scenarioHash"]), 64)
                self.assertEqual(len(result["scoreHash"]), 64)
                self.assertGreaterEqual(result["buildings"], len(scenario["buildings"]))
                first_runs.append(result)

        full_chain = next(s for s in scenarios if s["id"] == "full_chain")
        repeated = scenario_score.run_scenario(HOST_EXE, full_chain, 16)
        first = next(result for result in first_runs if result["scenarioId"] == "full_chain")
        self.assertEqual(first["scoreHash"], repeated["scoreHash"])
        self.assertEqual(first["stateHash"], repeated["stateHash"])
        self.assertEqual(first["tick"], 17)

    @unittest.skipUnless(HOST_EXE, "host executable argument is required")
    def test_supply_scenarios_complete_sites_and_repeat_deterministic_hashes(self):
        for name, expected in (("build_a_mill_with_supply", 1), ("build_two_mills_with_supply", 2)):
            with self.subTest(scenario=name):
                scenario = scenario_score.resolve_scenario(name)
                first = scenario_score.run_scenario(HOST_EXE, scenario, 64, exact_ticks=True)
                second = scenario_score.run_scenario(HOST_EXE, scenario, 64, exact_ticks=True)
                self.assertEqual(first["scoreUnits"], expected)
                self.assertEqual(first["scoreBreakdown"]["completedSites"], expected)
                self.assertTrue(all(site["state"] == "done" for site in first["scoreBreakdown"]["siteStates"]))
                self.assertGreater(first["scoreBreakdown"]["completionTick"], 1)
                self.assertLessEqual(first["scoreBreakdown"]["completionTick"], first["tick"])
                self.assertLessEqual(first["scoreBreakdown"]["completionTick"], 65)
                self.assertEqual(first["scoreHash"], second["scoreHash"])

    @unittest.skipUnless(HOST_EXE, "host executable argument is required")
    def test_combined_scenarios_repeat_score_hash_and_attempt_their_objectives(self):
        for name in ("growth_town", "power_and_build", "austerity"):
            with self.subTest(scenario=name):
                scenario = scenario_score.resolve_scenario(name)
                first = scenario_score.run_scenario(HOST_EXE, scenario, 64, exact_ticks=True)
                second = scenario_score.run_scenario(HOST_EXE, scenario, 64, exact_ticks=True)
                self.assertEqual(first["scoreHash"], second["scoreHash"])
                self.assertEqual(first["stateHash"], second["stateHash"])
                self.assertEqual(first["scoreBreakdown"], second["scoreBreakdown"])
                self.assertGreaterEqual(first["autopilot"]["creates_queued"], 1)
                if name in ("growth_town", "austerity"):
                    self.assertGreaterEqual(first["autopilot"].get("trade_queued", 0), 1)

    @unittest.skipUnless(HOST_EXE, "host executable argument is required")
    def test_power_objectives_report_measured_deficit_isolation_and_surplus(self):
        # The live host reports last-tick utility amounts in catalog units (power:2 -> 2), not micro-scaled.
        expected = {
            "power_deficit": (2, 2),
            "unconnected_consumer": (0, 2),
            "overbuilt_generator": (2, 0),
        }
        for name, (throughput, unmet) in expected.items():
            with self.subTest(scenario=name):
                result = scenario_score.run_scenario(HOST_EXE, scenario_score.resolve_scenario(name), 16)
                self.assertEqual(result["scoreBreakdown"]["throughputUnits"], throughput)
                self.assertEqual(result["scoreBreakdown"]["unmetDemandUnits"], unmet)
                self.assertEqual(len(result["scoreHash"]), 64)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
