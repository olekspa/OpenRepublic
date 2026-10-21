#!/usr/bin/env python3
"""Drive the OpenRepublic synthetic logistics demo through its stdio JSON API."""

import json
import queue
import subprocess
import sys
import threading

PROTOCOL_VERSION = 0
ACTIVE_HOSTS = set()


class Host:
    def __init__(self, executable, role):
        args = [executable]
        if role is not None:
            args.extend(["--role", role])
        self.process = subprocess.Popen(
            args,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )
        self.responses = queue.Queue()
        self.stdout_reader = threading.Thread(target=self._read_stdout, daemon=True)
        self.stdout_reader.start()
        self.sequence = 0
        ACTIVE_HOSTS.add(self)

    def _read_stdout(self):
        for line in self.process.stdout:
            self.responses.put(line)
        self.responses.put(None)

    def read_response_line(self):
        try:
            return self.responses.get(timeout=10)
        except queue.Empty as error:
            raise RuntimeError("Agent host response timed out") from error

    def call(self, method, **fields):
        self.sequence += 1
        request_id = str(self.sequence)
        request = {"id": request_id, "protocolVersion": PROTOCOL_VERSION, "method": method, **fields}
        self.process.stdin.write(json.dumps(request, separators=(",", ":")) + "\n")
        self.process.stdin.flush()
        line = self.read_response_line()
        if not line:
            detail = self.process.stderr.read()
            raise RuntimeError(f"Agent host exited without a response: {detail}")
        response = json.loads(line)
        if response.get("id") != request_id:
            raise RuntimeError(f"response id mismatch: expected {request_id}, got {response.get('id')!r}")
        return response

    def close(self):
        try:
            if self.process.stdin and not self.process.stdin.closed:
                self.process.stdin.close()
            code = self.process.wait(timeout=10)
            self.stdout_reader.join(timeout=2)
            if self.stdout_reader.is_alive():
                raise RuntimeError("Agent host stdout reader did not finish after process exit")
            if code != 0:
                detail = self.process.stderr.read()
                raise RuntimeError(f"Agent host exited with {code}: {detail}")
            try:
                extra = self.responses.get_nowait()
            except queue.Empty:
                extra = None
            if extra:
                raise RuntimeError("Agent host emitted protocol output after EOF")
            diagnostics = self.process.stderr.read()
            if diagnostics:
                raise RuntimeError(f"unexpected host diagnostics on stderr: {diagnostics}")
        finally:
            self._stop_child()
            ACTIVE_HOSTS.discard(self)

    def abort(self):
        try:
            if self.process.stdin and not self.process.stdin.closed:
                self.process.stdin.close()
        except OSError:
            pass
        self._stop_child()
        self.stdout_reader.join(timeout=2)
        for stream in (self.process.stdout, self.process.stderr):
            if stream and not stream.closed:
                stream.close()
        ACTIVE_HOSTS.discard(self)

    def _stop_child(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception, traceback):
        if exception_type is None:
            self.close()
        else:
            if self.process.stdin and not self.process.stdin.closed:
                self.process.stdin.close()
            self._stop_child()
        return False


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def decimal_u64(value, name):
    check(isinstance(value, str) and value.isascii() and value.isdigit(),
        f"{name} must be an unsigned decimal string")
    parsed = int(value)
    check(parsed <= 0xFFFFFFFFFFFFFFFF, f"{name} exceeds uint64")
    return parsed


def run_observer_checks(executable):
    bad_args = subprocess.run([executable, "--unknown-option"], capture_output=True, text=True, timeout=10)
    check(bad_args.returncode == 2 and not bad_args.stdout and bad_args.stderr,
        "command-line diagnostics are sent to stderr, not protocol stdout")
    host = Host(executable, None)
    hello = host.call("hello")
    check(hello.get("ok") and hello["result"]["role"] == "observer", "host default role handshake")
    before = host.call("world")["result"]["world"]
    step = host.call("step", ticks=1)
    check(step.get("ok") and step["result"]["errorCode"] == "permission_denied", "observer step denial")
    ack = host.call("acknowledge", commandId="1")
    check(ack.get("ok") and ack["result"]["errorCode"] == "permission_denied", "observer acknowledgement denial")
    unknown = host.call("command_result", commandId="1")
    check(unknown.get("ok") and unknown["result"]["errorCode"] == "unknown_command",
        "observer acknowledgement denial does not create or alter command results")
    invalid_lines = [
        "not-json\n",
        json.dumps({"id": "wrong-version", "protocolVersion": 99, "method": "step", "ticks": 1}) + "\n",
        '{"id":"duplicate","id":"other","protocolVersion":0,"method":"step","ticks":1}\n',
        json.dumps({"id": "unknown-field", "protocolVersion": 0, "method": "step", "ticks": 1, "extra": True}) + "\n",
        "not-json" + " " * (64 * 1024 - len("not-json")) + "\n",
    ]
    for raw in invalid_lines:
        host.process.stdin.write(raw)
        host.process.stdin.flush()
        rejected = json.loads(host.read_response_line())
        check(not rejected.get("ok", True), "malformed/version/duplicate/unknown-field request rejection")
    after = host.call("world")["result"]["world"]
    check(after["tick"] == before["tick"] and after["revision"] == before["revision"],
        "rejected requests and denied observer mutation leave state unchanged")
    host.close()


def run_unterminated_oversize_check(executable):
    process = subprocess.Popen(
        [executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    response_line = []
    reader = threading.Thread(target=lambda: response_line.append(process.stdout.readline()), daemon=True)
    try:
        # Keep stdin open after sending byte MaxRequestLineBytes + 1. The host
        # must reject immediately rather than waiting for a newline or EOF.
        process.stdin.write(b"x" * (64 * 1024 + 1))
        process.stdin.flush()
        reader.start()
        reader.join(timeout=5)
        check(not reader.is_alive(), "unterminated oversized request is rejected before EOF")
        check(response_line and response_line[0], "oversized request produces a protocol error response")
        response = json.loads(response_line[0])
        check(not response.get("ok", True) and response.get("error", {}).get("code") == "request_too_large",
            "unterminated oversized request returns request_too_large")
        check(process.wait(timeout=5) == 5, "oversized request exits promptly with documented status 5")
        trailing_stdout = process.stdout.read()
        stderr = process.stderr.read()
        check(not trailing_stdout, "oversized subprocess emits no stdout beyond its one protocol error")
        check(not stderr, f"oversized subprocess emits no diagnostics: {stderr!r}")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if process.stdin and not process.stdin.closed:
            process.stdin.close()
        process.stdout.close()
        process.stderr.close()


def run_controller(executable):
    host = Host(executable, "controller")
    hello = host.call("hello")
    check(hello.get("ok") and hello["result"]["role"] == "controller", "controller role handshake")
    actions = set(host.call("capabilities")["result"]["actions"])
    for action in ("query_world", "step_simulation", "construct_road", "construct_storage", "assign_transport_plan"):
        check(action in actions, f"controller capability {action}")

    def world():
        response = host.call("world")
        check(response.get("ok") and response["result"]["errorCode"] == "none", "world query")
        return response["result"]["world"]

    initial = world()
    initial_production = initial["production"]
    check(initial_production["rules"] == {
        "farmGrainPerHarvest": 2,
        "farmHarvestIntervalTicks": 2,
        "millGrainPerTick": 1,
    }, "external controller can inspect the authoritative synthetic production rules")
    initial_delivered = decimal_u64(initial_production["deliveredFlourTotal"], "initial deliveredFlourTotal")

    command_ids = []

    def submit(method, **args):
        current = world()
        response = host.call(method, revision=current["revision"], **args)
        check(response.get("ok"), f"{method} protocol request accepted")
        result = response["result"]
        check(result["errorCode"] == "none" and result["state"] == "queued", f"{method} queued: {result}")
        command_ids.append(result["commandId"])

    # Extend the built-in road to a new player storage, then assign all actions
    # before the first tick so the queue applies at one deterministic boundary.
    submit("add_road_segment", ax=2, az=0, bx=3, bz=0)
    submit("add_road_segment", ax=3, az=0, bx=4, bz=0)
    submit("add_road_segment", ax=4, az=0, bx=5, bz=0)
    submit("place_storage", x=5, z=0)
    current = world()
    plan = host.call("assign_transport_plan", revision=current["revision"], farmId=1001, millId=1002, storageId=3001)
    check(plan.get("ok") and plan["result"]["errorCode"] == "none" and plan["result"]["state"] == "queued",
        f"transport plan queued: {plan}")
    command_ids.append(plan["result"]["commandId"])

    stepped = host.call("step", ticks=1)
    check(stepped.get("ok") and stepped["result"]["errorCode"] == "none", "construction batch applied")
    for command_id in command_ids:
        outcome = host.call("command_result", commandId=command_id)
        check(outcome.get("ok") and outcome["result"]["errorCode"] == "none" and
            outcome["result"]["state"] == "applied", f"command {command_id} applied: {outcome}")
        acknowledged = host.call("acknowledge", commandId=command_id)
        check(acknowledged.get("ok") and acknowledged["result"]["errorCode"] == "none",
            f"command {command_id} acknowledged")

    delivered = None
    for _ in range(128):
        stepped = host.call("step", ticks=16)
        check(stepped.get("ok") and stepped["result"]["errorCode"] == "none", "bounded simulation step")
        state = world()
        storage = next((entity for entity in state["entities"] if entity["id"] == 3001), None)
        check(storage is not None, "player storage remains queryable")
        if storage["inventory"]["flour"] > 0:
            delivered = storage
            break
    check(delivered is not None, "external controller completes flour delivery within bounded run")
    final = world()
    check(final["route"]["storageId"] == 3001, "authoritative route targets player storage")
    check(delivered["inventory"]["flour"] > 0, "player storage contains delivered flour")
    final_delivered = decimal_u64(final["production"]["deliveredFlourTotal"], "final deliveredFlourTotal")
    stored_flour = sum(entity["inventory"]["flour"] for entity in final["entities"]
        if entity["kind"] == "storage")
    check(final_delivered > initial_delivered,
        "cumulative delivery telemetry increases after the external controller's route runs")
    check(final_delivered >= stored_flour,
        "cumulative delivery telemetry reconciles with flour remaining in storage when consumption is absent")
    events = host.call("events", afterSequence="0", limit=32)
    check(events.get("ok") and events["result"]["errorCode"] == "none" and events["result"]["events"],
        "controller can observe structured lifecycle events")
    host.close()
    return final["tick"], delivered["inventory"]["flour"]


def main():
    if len(sys.argv) != 2:
        print("Usage: drive_logistics.py <OpenRepublicAgentHost executable>", file=sys.stderr)
        return 2
    executable = sys.argv[1]
    try:
        run_observer_checks(executable)
        run_unterminated_oversize_check(executable)
        tick, flour = run_controller(executable)
        print(f"PASS: external stdio controller delivered {flour} flour to player storage by tick {tick}.")
        return 0
    finally:
        for host in tuple(ACTIVE_HOSTS):
            host.abort()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
