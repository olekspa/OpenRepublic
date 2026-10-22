#!/usr/bin/env python3
"""Recover an oversized command history, export it, and verify process continuation."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from checkpoint_controller import CATALOG, Client, load_exporter, require, setup, step, submit, world


def total(snapshot):
    return sum(int(item["amount"]) for b in snapshot["buildings"] for item in b["inventory"]) + sum(
        int(v["cargo"]) for v in snapshot["vehicles"])


def main():
    require(len(sys.argv) == 2, "Usage: command_history_controller.py <OpenRepublicEconomyHost>")
    host = sys.argv[1]
    exporter = load_exporter()
    with tempfile.TemporaryDirectory(prefix="openrepublic-history-") as directory:
        directory = Path(directory)
        catalog = directory / "authored.catalog"
        catalog.write_text(CATALOG, encoding="utf-8")
        original = Client(host, ["--catalog", str(catalog)])
        restored = None
        try:
            pending = setup(original)
            hello = original.call("hello")
            require(hello["commandHistoryVersion"] == 1 and hello["commandHistoryPolicy"] == "explicit",
                    "host advertises explicit versioned history maintenance")
            revision = world(original)["revision"]
            invalid_order = {"source": 1, "destination": 999999, "resource": "grain", "vehicleType": "truck"}
            first_handle = last_handle = None
            count = 75_008
            for _ in range(count // 64):
                batch = original.call("create_haul_orders", revision=revision, orders=[invalid_order] * 64)
                require(batch["errorCode"] == "none" and len(batch["results"]) == 64, "batch admitted")
                require(all(r["errorCode"] == "none" and r["state"] == "queued" for r in batch["results"]),
                        "each stress command queued")
                first_handle = first_handle or batch["results"][0]["handle"]
                last_handle = batch["results"][-1]["handle"]
            step(original)
            before = world(original)
            require(total(before) == 12, "stress commands do not change stored/onboard grain")
            require(original.call("poll", handle=pending)["state"] == "accepted", "pending retarget applied")
            require(original.call("poll", handle=first_handle)["state"] == "rejected", "stress result terminal")
            failed = original.call("begin_checkpoint", revision=before["revision"])
            require(failed["errorCode"] == "persistence_failed", "oversized retained history refuses export")
            require(world(original)["stateHash"] == before["stateHash"], "failed export is immutable")

            queued = submit(original, {"kind": "retarget_haul_order", "orderId": 1, "destination": 2})
            before = world(original)
            compact = original.call("compact_history", revision=before["revision"], keepRecent=32)
            require(compact["errorCode"] == "none", "explicit compaction accepted")
            after = world(original)
            require(after["tick"] == before["tick"] and int(after["revision"]) == int(before["revision"]) + 1,
                    "compaction advances revision once, without a tick")
            for collection in ("buildings", "roads", "vehicles", "orders"):
                require(after[collection] == before[collection], f"compaction preserves {collection}")
            history = after["commandHistory"]
            require(history["retainedTerminalResults"] == "32" and history["pendingCommands"] == "1",
                    "recent terminal results and every pending command retained")
            require(int(compact["removedResults"]) > count - 32 and
                    compact["removedResults"] == compact["expiredHandles"], "core and API storage retired")
            expired = original.call("poll", handle=first_handle)
            require(expired["errorCode"] == "command_expired" and expired["state"] == "expired" and
                    expired["failure"] == "none", "discarded terminal handle explicitly expired")
            require(original.call("poll", handle=last_handle)["state"] == "rejected", "recent result retained")
            require(original.call("poll", handle=queued)["state"] == "queued", "pending result never expires")
            noop = original.call("compact_history", revision=after["revision"], keepRecent=32)
            require(noop["errorCode"] == "none" and noop["removedResults"] == "0" and
                    world(original)["stateHash"] == after["stateHash"], "repeated compaction is immutable")

            path = directory / "compacted.checkpoint"
            receipt = exporter.export_checkpoint(original, path)
            require(path.stat().st_size < 16_384 and b"OPENREPUBLIC_ECONOMY_WORLD 3\n" in path.read_bytes(),
                    "small checkpoint embeds compacted v3 world")
            restored = Client(host, ["--checkpoint", str(path)])
            resumed = world(restored)
            require(original.session != restored.session and resumed["stateHash"] == after["stateHash"] and
                    resumed["commandHistory"] == history, "fresh process resumes exact retained floor and state")
            require(restored.call("poll", handle=queued)["errorCode"] == "unknown_command",
                    "old process handles do not become current-session expired handles")
            for _ in range(24):
                step(original)
                step(restored)
                left, right = world(original), world(restored)
                require(left["stateHash"] == right["stateHash"], "v3 process continuation hashes match")
                require(total(left) == total(right) == 12, "continued cargo accounting conserved")
            require(original.call("poll", handle=queued)["state"] == "accepted", "pending retarget applied after export")
            autopilot = Path(__file__).resolve().parents[2] / "tools" / "republic_autopilot.py"
            retired = []
            for keep in ("0", "all"):
                run = subprocess.run([sys.executable, "-B", str(autopilot), "--host", host,
                                      "--rounds", "2", "--ticks", "1", "--retain-command-results", keep,
                                      "--json", "--", "--checkpoint", str(path)],
                                     capture_output=True, text=True, encoding="utf-8", timeout=30)
                require(run.returncode == 0 and not run.stderr, f"actual autopilot CLI succeeds: {run.stderr}")
                result = json.loads(run.stdout.splitlines()[-1])
                retired.append(result["commandResultsRetired"])
                require(result["commandResultsRetention"] == (0 if keep == "0" else None),
                        "CLI reports selected retention policy")
            require(retired == [33, 0], "actual CLI compacts after polls and explicit all opt-out preserves history")
            print(json.dumps({"verified": True, "stressCommands": count, "removedResults": compact["removedResults"],
                              "expiredHandles": compact["expiredHandles"], "checkpointBytes": receipt["totalBytes"],
                              "matchedTicks": 24, "grainUnits": 12, "finalTick": left["tick"],
                              "finalStateHash": left["stateHash"], "autopilotRetired": retired}, sort_keys=True))
        finally:
            if restored:
                restored.close()
            original.close()


if __name__ == "__main__":
    main()
