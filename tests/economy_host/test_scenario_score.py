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


class ScenarioHostIntegrationTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
