#!/usr/bin/env python3
"""Exercise scenario tournament wiring with only the scripted backend."""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOURNAMENT = ROOT / "tools" / "republic_tournament.py"
HOST = sys.argv[1]


class ScenarioTournamentTests(unittest.TestCase):
    def test_scripted_controller_and_autopilot_share_scenario_metric(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            manifest = root / "manifest.json"
            responses = root / "responses.json"
            output = root / "results.json"
            manifest.write_text(json.dumps({"controllers": [
                {"name": "scripted", "kind": "llm", "backend": "scripted",
                 "scripted_responses": str(responses)}]}), encoding="utf-8")
            responses.write_text(json.dumps([{"actions": [], "notes": "bounded test"}]), encoding="utf-8")
            completed = subprocess.run([
                sys.executable, str(TOURNAMENT), "--host", HOST, "--manifest", str(manifest),
                "--scenario", "grain_shortage", "--rounds", "2", "--ticks", "1",
                "--repeats", "2", "--final-tick", "7",
                "--transcript-dir", str(root / "transcripts"), "--output-json", str(output)],
                text=True, capture_output=True, timeout=60, check=False)
            self.assertEqual(completed.returncode, 0, completed.stderr + completed.stdout)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(result["scenarioId"], "grain_shortage")
            self.assertEqual(result["scenarioMetric"], "stored_non_raw_units")
            self.assertEqual(result["repeats"], 2)
            self.assertTrue(result["conclusiveSample"])
            rows = result["results"]
            self.assertEqual(len(rows), 4)
            self.assertEqual({row["controller"] for row in rows}, {"scripted", "autopilot-baseline"})
            for row in rows:
                self.assertIn("scoreMicroUnits", row)
                self.assertIn("scoreHash", row)
                self.assertEqual(row["tick"], 7)
                self.assertIn("rejectedActions", row)
            for controller in ("scripted", "autopilot-baseline"):
                hashes = {row["scoreHash"] for row in rows if row["controller"] == controller}
                self.assertEqual(len(hashes), 1, f"{controller} repeat receipts should be deterministic")
            self.assertEqual({entry["runs"] for entry in result["summary"]}, {2})
            self.assertTrue(all(entry["metrics"]["scoreUnits"]["sampleStddev"] is not None
                                for entry in result["summary"]))
            self.assertIn("stored non-raw units", completed.stdout)
            self.assertIn("CONCLUSIVE SAMPLE", completed.stdout)

    def test_scripted_tournaments_run_combined_scenario_catalogs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            manifest = root / "manifest.json"
            responses = root / "responses.json"
            manifest.write_text(json.dumps({"controllers": [
                {"name": "scripted", "kind": "llm", "backend": "scripted",
                 "scripted_responses": str(responses)}]}), encoding="utf-8")
            responses.write_text(json.dumps([{"actions": [], "notes": "combined fixture smoke"}]), encoding="utf-8")
            for scenario in ("growth_town", "power_and_build", "austerity"):
                with self.subTest(scenario=scenario):
                    output = root / f"{scenario}.json"
                    completed = subprocess.run([
                        sys.executable, str(TOURNAMENT), "--host", HOST, "--manifest", str(manifest),
                        "--scenario", scenario, "--rounds", "4", "--ticks", "4", "--repeats", "1",
                        "--transcript-dir", str(root / "transcripts"), "--output-json", str(output)],
                        text=True, capture_output=True, timeout=60, check=False)
                    self.assertEqual(completed.returncode, 0, completed.stderr + completed.stdout)
                    result = json.loads(output.read_text(encoding="utf-8"))
                    self.assertEqual(result["scenarioId"], scenario)
                    self.assertEqual(result["scenarioMetric"], "combined_multi_objective")
                    self.assertEqual({row["controller"] for row in result["results"]},
                                     {"scripted", "autopilot-baseline"})
                    for row in result["results"]:
                        self.assertEqual(len(row["scoreHash"]), 64)
                        self.assertIn("scoreBreakdown", row)

    def test_single_run_is_marked_non_conclusive(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            manifest = root / "manifest.json"
            responses = root / "responses.json"
            output = root / "results.json"
            manifest.write_text(json.dumps({"controllers": [
                {"name": "scripted", "kind": "llm", "backend": "scripted",
                 "scripted_responses": str(responses)}]}), encoding="utf-8")
            responses.write_text(json.dumps([{"actions": [], "notes": "bounded test"}]), encoding="utf-8")
            completed = subprocess.run([
                sys.executable, str(TOURNAMENT), "--host", HOST, "--manifest", str(manifest),
                "--scenario", "grain_shortage", "--rounds", "1", "--ticks", "1",
                "--transcript-dir", str(root / "transcripts"), "--output-json", str(output)],
                text=True, capture_output=True, timeout=60, check=False)
            self.assertEqual(completed.returncode, 0, completed.stderr + completed.stdout)
            self.assertFalse(json.loads(output.read_text(encoding="utf-8"))["conclusiveSample"])
            self.assertIn("NON-CONCLUSIVE", completed.stdout)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
