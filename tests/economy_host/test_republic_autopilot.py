import contextlib
import copy
import importlib.util
import io
import pathlib
import json
import unittest


SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "tools" / "republic_autopilot.py"
SPEC = importlib.util.spec_from_file_location("republic_autopilot", SCRIPT)
autopilot = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(autopilot)


def world(revision=7):
    return {"revision": str(revision), "tick": "10", "buildings": [], "roads": [],
            "vehicles": [], "orders": []}


class FakeHost:
    def __init__(self, snapshot=None, plans=None, command_states=None, submit_errors=None):
        self.snapshot = snapshot or world()
        self.plans = list(plans or [])
        self.command_states = list(command_states or [])
        self.submit_errors = list(submit_errors or [])
        self.calls = []
        self.pending = {}
        self.next_handle = 0

    def call(self, method, **params):
        self.calls.append((method, copy.deepcopy(params)))
        if method == "world":
            return {"errorCode": "none", "world": copy.deepcopy(self.snapshot)}
        if method == "advise":
            if self.plans:
                plan = self.plans.pop(0)
            else:
                plan = {"revision": self.snapshot["revision"], "unservedInputs": []}
            return {"errorCode": "none", **copy.deepcopy(plan)}
        if method == "submit":
            if self.submit_errors:
                error = self.submit_errors.pop(0)
                return {"errorCode": error, "state": "rejected"}
            return self._queued()
        if method == "create_haul_orders":
            results = [self._queued() for _ in params["orders"]]
            return {"errorCode": "none", "results": results}
        if method == "step":
            self.snapshot["tick"] = str(int(self.snapshot["tick"]) + params["ticks"])
            self.snapshot["revision"] = str(int(self.snapshot["revision"]) + 1)
            for handle, command in list(self.pending.items()):
                if command.get("kind") == "retarget_haul_order":
                    order = next((o for o in self.snapshot["orders"]
                                  if o["id"] == command["orderId"]), None)
                    if order:
                        order["destination"] = command["destination"]
                if command.get("kind") == "place_building":
                    self.snapshot["buildings"].append({"id": 99, "type": command["type"],
                                                       "cell": {"x": command["x"], "z": command["z"]},
                                                       "stallReason": "running", "inventory": []})
            return {"errorCode": "none"}
        if method == "poll":
            state = self.command_states.pop(0) if self.command_states else "accepted"
            return {"errorCode": "none", "state": state}
        raise AssertionError(f"unexpected method {method}")

    def _queued(self):
        self.next_handle += 1
        handle = str(self.next_handle)
        # The caller records submit commands separately; queued handle ownership is
        # sufficient for lifecycle tests that do not need to simulate world effects.
        self.pending[handle] = self.calls[-1][1].get("command", {})
        return {"errorCode": "none", "state": "queued", "handle": handle}


def suggestion(order_id=5, destination=3):
    return {"orderId": order_id, "vehicleId": 50, "resource": "grain", "cargo": "4",
            "destination": destination, "distanceCells": 2,
            "command": {"kind": "retarget_haul_order", "orderId": order_id,
                        "destination": destination, "ignoredAdviserField": True}}


def order_snapshot(revision=7):
    snapshot = world(revision)
    snapshot["orders"] = [{"id": 5, "source": 10, "destination": 1,
                           "resource": "grain", "cargo": "4"}]
    return snapshot


class FakeStdin:
    def __init__(self):
        self.lines = []

    def write(self, line):
        self.lines.append(line)

    def flush(self):
        pass


class FakeStdout:
    def __init__(self, lines):
        self.lines = list(lines)

    def readline(self):
        return self.lines.pop(0)


class FakeProcess:
    def __init__(self, lines):
        self.stdin = FakeStdin()
        self.stdout = FakeStdout(lines)


class AutopilotTests(unittest.TestCase):
    def run_round(self, host, catalog=None, ticks=1, placed=None):
        with contextlib.redirect_stdout(io.StringIO()):
            return autopilot.run_round(host, catalog or {"buildingTypes": []}, ticks,
                                       placed if placed is not None else set())

    def test_success_queues_exact_retarget_first_and_counts_after_poll(self):
        plan = {"revision": "7", "retargets": [suggestion()],
                "unservedInputs": [{"producers": [{"command": {
                    "source": 10, "destination": 3, "resource": "grain", "vehicleType": "truck"}}]}]}
        host = FakeHost(order_snapshot(), [plan])
        result = self.run_round(host)
        submitted = [params for method, params in host.calls if method == "submit"]
        self.assertEqual(submitted[0]["command"],
                         {"kind": "retarget_haul_order", "orderId": 5, "destination": 3})
        self.assertEqual(submitted[0]["revision"], "7")
        self.assertEqual(result["retargets_queued"], 1)
        self.assertEqual(result["accepted_retargets"], 1)
        self.assertEqual(result["rejected_retargets"], 0)
        self.assertEqual(result["creates_queued"], 0)
        self.assertEqual(result["world"]["orders"][0]["destination"], 3)
        methods = [method for method, _ in host.calls]
        self.assertLess(methods.index("submit"), methods.index("create_haul_orders") if "create_haul_orders" in methods else methods.index("step"))
        self.assertEqual(len([1 for method, params in host.calls if method == "step" and params["ticks"] == 1]), 1)

    def test_trade_plan_exports_above_reserve_and_imports_only_with_funds(self):
        catalog = {
            "resources": [
                {"name": "grain", "tradeDeclared": True, "buyPrice": "5", "sellPrice": "3"},
                {"name": "coal", "tradeDeclared": True, "buyPrice": "5", "sellPrice": "1"},
            ],
            "buildingTypes": [{"name": "trade_post", "tradePost": True,
                "storage": [{"resource": "grain", "capacity": "100"},
                            {"resource": "coal", "capacity": "20"}],
                "recipes": [{"inputs": [{"resource": "coal"}], "outputs": [{"resource": "power"}]}]}],
        }
        snapshot = world()
        snapshot["treasury"] = "20"
        snapshot["buildings"] = [{"id": 4, "type": "trade_post",
            "inventory": [{"resource": "grain", "amount": "25"},
                          {"resource": "coal", "amount": "0"}]}]
        actions = autopilot._trade_actions(snapshot, catalog)
        self.assertEqual(actions, [
            ("trade_export", {"buildingId": 4, "resource": "grain", "amount": 15, "rateCap": 15}),
            ("trade_import", {"buildingId": 4, "resource": "coal", "amount": 4, "rateCap": 4}),
        ])

    def test_host_call_uses_monotonic_ids_session_binding_and_protocol_errors(self):
        client = autopilot.Host.__new__(autopilot.Host)
        client.proc = FakeProcess([
            json.dumps({"ok": True, "result": {}, "sessionId": "session-a"}) + "\n",
            json.dumps({"ok": True, "result": {"tick": "0"}, "sessionId": "session-a"}) + "\n",
            json.dumps({"ok": False, "error": {"code": "stale_session"}}) + "\n",
        ])
        client.next_id = 0
        client.session_id = None
        self.assertEqual(client.call("hello"), {})
        self.assertEqual(client.call("world"), {"tick": "0"})
        requests = [json.loads(line) for line in client.proc.stdin.lines]
        self.assertEqual([request["id"] for request in requests], ["1", "2"])
        self.assertNotIn("sessionId", requests[0])
        self.assertEqual(requests[1]["sessionId"], "session-a")
        with self.assertRaisesRegex(RuntimeError, "stale_session"):
            client.call("advise")
        self.assertEqual(json.loads(client.proc.stdin.lines[-1])["id"], "3")

    def test_terminal_rejection_is_not_counted_as_acceptance(self):
        host = FakeHost(order_snapshot(), [{"revision": "7", "retargets": [suggestion()],
                                           "unservedInputs": []}], ["rejected"])
        result = self.run_round(host)
        self.assertEqual(result["retargets_queued"], 1)
        self.assertEqual(result["accepted_retargets"], 0)
        self.assertEqual(result["rejected_retargets"], 1)

    def test_unqueued_retarget_does_not_suppress_matching_create(self):
        plan = {"revision": "7", "retargets": [suggestion()],
                "unservedInputs": [{"producers": [{"command": {
                    "source": 10, "destination": 3, "resource": "grain", "vehicleType": "truck"}}]}]}
        host = FakeHost(order_snapshot(), [plan], submit_errors=["invalid_argument"])
        result = self.run_round(host)
        self.assertEqual(result["retargets_queued"], 0)
        self.assertEqual(result["rejected_retargets"], 1)
        self.assertEqual(result["creates_queued"], 1)
        self.assertEqual(result["accepted_creates"], 1)

    def test_stale_advice_revision_rejected_then_fresh_revision_works(self):
        host = FakeHost(order_snapshot(8), [
            {"revision": "7", "retargets": [suggestion()], "unservedInputs": [{"producers": [{"command": {
                "source": 10, "destination": 3, "resource": "grain", "vehicleType": "truck"}}]}]},
            {"revision": "8", "retargets": [suggestion()], "unservedInputs": []},
        ], submit_errors=["stale_revision"])
        first = self.run_round(host)
        self.assertEqual(first["accepted_retargets"], 1)
        self.assertEqual(first["rejected_retargets"], 1)
        self.assertEqual(first["creates_queued"], 0)
        submitted = [params for method, params in host.calls if method == "submit"]
        self.assertEqual([params["revision"] for params in submitted], ["7", "8"])

    def test_old_adviser_without_retargets_remains_supported(self):
        host = FakeHost(world(), [{"revision": "7", "unservedInputs": [{"producers": [{"command": {
            "source": 1, "destination": 2, "resource": "grain", "vehicleType": "truck"}}]}]}])
        result = self.run_round(host)
        self.assertEqual(result["accepted_retargets"], 0)
        self.assertEqual(result["creates_queued"], 1)
        self.assertEqual(result["accepted_creates"], 1)
        self.assertEqual(result["rejected_creates"], 0)

    def test_refresh_does_not_duplicate_an_already_queued_retarget(self):
        class ConcurrentHost(FakeHost):
            def call(self, method, **params):
                result = super().call(method, **params)
                if method == "submit":
                    # A rule reload can change revision without applying the pending command.
                    self.snapshot["revision"] = "8"
                return result

        need = {"producers": [{"command": {
            "source": 10, "destination": 3, "resource": "grain", "vehicleType": "truck"}}]}
        host = ConcurrentHost(order_snapshot(), [
            {"revision": "7", "retargets": [suggestion()], "unservedInputs": []},
            {"revision": "8", "retargets": [suggestion()], "unservedInputs": [need]},
        ])
        result = self.run_round(host)
        self.assertEqual(result["retargets_queued"], 1)
        self.assertEqual(result["accepted_retargets"], 1)
        self.assertEqual(result["creates_queued"], 0)
        self.assertEqual(len([1 for method, _ in host.calls if method == "submit"]), 1)

    def test_run_round_respects_generated_power_score_option(self):
        snapshot = world()
        snapshot["buildings"] = [{"id": 1, "type": "powerplant solar", "stallReason": "running",
                                  "inventory": [{"resource": "electricity", "amount": "5000000"}]}]
        host = FakeHost(snapshot, [{"revision": "7", "unservedInputs": []}])
        catalog = {"buildingTypes": [
            {"name": "powerplant solar", "recipes": [{"inputs": [], "outputs": [
                {"resource": "electricity"}]}]},
            {"name": "factory", "recipes": [{"inputs": [{"resource": "electricity"}],
                                                "outputs": [{"resource": "goods"}]}]},
        ]}
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            result = autopilot.run_round(host, catalog, 1, set(), count_generated=True)
        self.assertEqual(result["world"]["tick"], "11")
        self.assertIn("score (stored non-raw units) 5.0", output.getvalue())

    def test_placement_advancement_refreshes_advice_before_retarget(self):
        snapshot = order_snapshot()
        snapshot["buildings"].append({"id": 20, "type": "consumer", "cell": {"x": 0, "z": 0},
                                       "stallReason": "running", "inventory": []})
        snapshot["roads"] = [{"enabled": True, "a": {"x": 0, "z": 0}, "b": {"x": 1, "z": 0}}]
        first_plan = {"revision": "7", "retargets": [], "unservedInputs": [{
            "building": 20, "resource": "grain", "producers": []}]}
        refreshed = {"revision": "8", "retargets": [suggestion()], "unservedInputs": []}
        catalog = {"buildingTypes": [{"name": "farm", "width": 1, "recipes": [{
            "inputs": [], "outputs": [{"resource": "grain"}]}]}]}
        host = FakeHost(snapshot, [first_plan, refreshed])
        result = self.run_round(host, catalog, placed=set())
        advises = [call for call in host.calls if call[0] == "advise"]
        submits = [params for method, params in host.calls if method == "submit"]
        self.assertEqual(len(advises), 2)
        self.assertEqual(submits[0]["command"]["kind"], "place_building")
        self.assertEqual(submits[1]["command"],
                         {"kind": "retarget_haul_order", "orderId": 5, "destination": 3})
        self.assertEqual(submits[1]["revision"], "8")
        self.assertEqual(result["accepted_retargets"], 1)
        self.assertEqual(result["placements_accepted"], 1)

    def test_rejected_placement_attempts_still_refresh_advice(self):
        snapshot = order_snapshot()
        snapshot["buildings"].append({"id": 20, "type": "consumer", "cell": {"x": 0, "z": 0},
                                       "stallReason": "running", "inventory": []})
        snapshot["roads"] = [{"enabled": True, "a": {"x": 0, "z": 0}, "b": {"x": 1, "z": 0}}]
        first_plan = {"revision": "7", "retargets": [], "unservedInputs": [{
            "building": 20, "resource": "grain", "producers": []}]}
        refreshed = {"revision": "9", "retargets": [suggestion()], "unservedInputs": []}
        catalog = {"buildingTypes": [{"name": "farm", "width": 1, "recipes": [{
            "inputs": [], "outputs": [{"resource": "grain"}]}]}]}
        host = FakeHost(snapshot, [first_plan, refreshed], command_states=["rejected", "rejected", "accepted"])
        result = self.run_round(host, catalog, placed=set())
        advises = [call for call in host.calls if call[0] == "advise"]
        submits = [params for method, params in host.calls if method == "submit"]
        self.assertEqual(len(advises), 2)
        self.assertEqual(submits[-1]["command"]["kind"], "retarget_haul_order")
        self.assertEqual(submits[-1]["revision"], "9")
        self.assertEqual(result["accepted_retargets"], 1)
        self.assertEqual(result["placements_accepted"], 0)

    def test_history_compaction_runs_after_every_outcome_poll(self):
        class HistoryHost(FakeHost):
            methods = {"compact_history"}

            def call(self, method, **params):
                if method != "compact_history":
                    return super().call(method, **params)
                self.calls.append((method, copy.deepcopy(params)))
                retained = int(self.snapshot["commandHistory"]["retainedTerminalResults"])
                removed = retained - params["keepRecent"]
                self.snapshot["commandHistory"]["retainedTerminalResults"] = str(params["keepRecent"])
                self.snapshot["revision"] = str(int(self.snapshot["revision"]) + 1)
                return {"errorCode": "none", "removedResults": str(removed)}

        snapshot = order_snapshot()
        snapshot["commandHistory"] = {"version": 1, "retainedTerminalResults": "1100"}
        host = HistoryHost(snapshot, [{"revision": "7", "retargets": [suggestion()],
                                       "unservedInputs": []}])
        result = self.run_round(host)
        methods = [method for method, _ in host.calls]
        self.assertLess(methods.index("poll"), methods.index("compact_history"))
        self.assertEqual(result["accepted_retargets"], 1)
        self.assertEqual(result["command_results_removed"], 76)
        self.assertEqual(result["world"]["revision"], "9")
        self.assertEqual(result["world"]["commandHistory"]["retainedTerminalResults"], "1024")
        compact = next(params for method, params in host.calls if method == "compact_history")
        self.assertEqual(compact, {"revision": "8", "keepRecent": 1024})

    def test_history_unsupported_opt_out_and_below_limit_are_read_only(self):
        snapshot = world()
        snapshot["commandHistory"] = {"version": 1, "retainedTerminalResults": "2048"}
        old_host = FakeHost(copy.deepcopy(snapshot))
        self.assertEqual(autopilot._compact_history(old_host, snapshot, 1024), (snapshot, 0))
        current_host = FakeHost(copy.deepcopy(snapshot))
        current_host.methods = {"compact_history"}
        self.assertEqual(autopilot._compact_history(current_host, snapshot, None), (snapshot, 0))
        snapshot["commandHistory"]["retainedTerminalResults"] = "1024"
        self.assertEqual(autopilot._compact_history(current_host, snapshot, 1024), (snapshot, 0))
        self.assertEqual(old_host.calls, [])
        self.assertEqual(current_host.calls, [])

    def test_history_stale_revision_refresh_can_obviate_pruning(self):
        class StaleHost(FakeHost):
            methods = {"compact_history"}

            def call(self, method, **params):
                if method == "compact_history":
                    self.calls.append((method, params))
                    self.snapshot["revision"] = "8"
                    self.snapshot["commandHistory"]["retainedTerminalResults"] = "0"
                    return {"errorCode": "stale_revision"}
                return super().call(method, **params)

        snapshot = world()
        snapshot["commandHistory"] = {"version": 1, "retainedTerminalResults": "1025"}
        host = StaleHost(copy.deepcopy(snapshot))
        after, removed = autopilot._compact_history(host, snapshot, 1024)
        self.assertEqual(after["revision"], "8")
        self.assertEqual(removed, 0)
        self.assertEqual([method for method, _ in host.calls], ["compact_history", "world"])

    def test_history_stale_retry_is_bounded_and_errors_are_reported(self):
        class StaleHost(FakeHost):
            methods = {"compact_history"}

            def call(self, method, **params):
                if method == "compact_history":
                    self.calls.append((method, params))
                    return {"errorCode": "stale_revision"}
                return super().call(method, **params)

        snapshot = world()
        snapshot["commandHistory"] = {"version": 1, "retainedTerminalResults": "1025"}
        host = StaleHost(copy.deepcopy(snapshot))
        with self.assertRaisesRegex(RuntimeError, "stale_revision"):
            autopilot._compact_history(host, snapshot, 1024)
        self.assertEqual([method for method, _ in host.calls], ["compact_history", "world", "compact_history"])

    def test_history_invalid_metadata_is_not_silently_ignored(self):
        host = FakeHost()
        host.methods = {"compact_history"}
        for metadata in (None, [], {}, {"version": 2}, {"version": 1, "retainedTerminalResults": "-1"},
                         {"version": 1, "retainedTerminalResults": "bad"}):
            snapshot = {**world(), "commandHistory": metadata}
            with self.assertRaisesRegex(RuntimeError, "invalid command history"):
                autopilot._compact_history(host, snapshot, 1024)
        self.assertEqual(host.calls, [])

    def test_history_retention_argument_bounds(self):
        self.assertEqual(autopilot.retained_results("0"), 0)
        self.assertEqual(autopilot.retained_results("4096"), 4096)
        self.assertIsNone(autopilot.retained_results("all"))
        for value in ("-1", "4097", "1.5", "no"):
            with self.assertRaises(autopilot.argparse.ArgumentTypeError):
                autopilot.retained_results(value)


if __name__ == "__main__":
    unittest.main()
