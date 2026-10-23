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
                "--scenario", "grain_shortage", "--rounds", "1", "--ticks", "1",
                "--transcript-dir", str(root / "transcripts"), "--output-json", str(output)],
                text=True, capture_output=True, timeout=60, check=False)
            self.assertEqual(completed.returncode, 0, completed.stderr + completed.stdout)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(result["scenarioId"], "grain_shortage")
            self.assertEqual(result["scenarioMetric"], "stored_non_raw_units")
            rows = result["results"]
            self.assertEqual({row["controller"] for row in rows}, {"scripted", "autopilot-baseline"})
            for row in rows:
                self.assertIn("scoreMicroUnits", row)
                self.assertIn("scoreHash", row)
            self.assertIn("stored non-raw units", completed.stdout)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
