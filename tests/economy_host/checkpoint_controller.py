#!/usr/bin/env python3
"""Verify checkpoint export, process restart and continuation without game inputs."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from http.server import ThreadingHTTPServer


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def load_exporter():
    path = Path(__file__).resolve().parents[2] / "tools" / "economy_checkpoint.py"
    spec = importlib.util.spec_from_file_location("economy_checkpoint", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Client:
    def __init__(self, exe, extra):
        self.proc = subprocess.Popen([exe, "--role", "controller", *extra], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                     encoding="utf-8", bufsize=1)
        self.id = 0
        self.session = None
        self.session = self.call("hello")["sessionId"]

    def request(self, method, **fields):
        self.id += 1
        request = {"protocolVersion": 0, "id": str(self.id), "method": method, **fields}
        if self.session:
            request.setdefault("sessionId", self.session)
        self.proc.stdin.write(json.dumps(request) + "\n")
        self.proc.stdin.flush()
        line = self.proc.stdout.readline()
        require(bool(line), "host closed protocol output")
        response = json.loads(line)
        require(response["id"] == str(self.id), "matching response identity")
        return response

    def call(self, method, **fields):
        response = self.request(method, **fields)
        require(response.get("ok"), f"{method}: {response.get('error')}")
        return response["result"]

    def close(self):
        try:
            if self.proc.poll() is None:
                self.call("shutdown")
            self.proc.stdin.close()
            code = self.proc.wait(timeout=5)
            errors = self.proc.stderr.read()
            require(code == 0 and not errors, f"clean host exit: {code}, {errors}")
        finally:
            if self.proc.poll() is None:
                self.proc.kill()
                self.proc.wait(timeout=5)


CATALOG = """openrepublic-economy 1
resource grain
building source 1 1
  store grain 100
  recipe harvest every 1 out grain:1
end
building depot 1 1
  store grain 20
end
vehicle truck capacity 4 speed 1
"""


def world(client):
    reply = client.call("world")
    require(reply["errorCode"] == "none", "query world")
    return reply["world"]


def submit(client, command):
    reply = client.call("submit", revision=world(client)["revision"], command=command)
    require(reply["errorCode"] == "none", "command queued")
    return reply["handle"]


def step(client, ticks=1):
    require(client.call("step", ticks=ticks)["errorCode"] == "none", "step accepted")


def accepted(client, handles):
    for handle in handles:
        require(client.call("poll", handle=handle)["state"] == "accepted", "fixture command accepted")


def setup(client):
    handles = []
    for name, x in (("source", 1), ("depot", 9), ("depot", 15)):
        handles.append(submit(client, {"kind": "place_building", "type": name, "x": x, "z": 2}))
    for x in range(1, 15):
        handles.append(submit(client, {"kind": "build_road", "ax": x, "az": 1, "bx": x + 1, "bz": 1}))
    step(client, 12)
    accepted(client, handles)
    handles = [submit(client, {"kind": "set_building_enabled", "buildingId": 1, "enabled": False}),
               submit(client, {"kind": "create_haul_order", "source": 1, "destination": 2,
                               "resource": "grain", "vehicleType": "truck"})]
    step(client)
    accepted(client, handles)
    require(world(client)["orders"][0]["cargo"] == "4", "fixture has a naturally loaded truck")
    return submit(client, {"kind": "retarget_haul_order", "orderId": 1, "destination": 3})


def startup_failures(exe, directory, valid_path, exporter):
    for option, value in (("--catalog", ""), ("--rules", "missing.lua"),
                          ("--region", "missing"), ("--content-root", "missing"),
                          ("--center", "-1"), ("--radius", "400")):
        result = subprocess.run([exe, "--checkpoint", str(valid_path), option, value],
                                input=b"", capture_output=True, timeout=5)
        require(result.returncode == 2 and not result.stdout and b"cannot be combined" in result.stderr,
                "explicit incompatible option rejects before opening files: " + option)
    for name, payload in (("empty", b""), ("truncated", valid_path.read_bytes()[:-1]),
                          ("trailing", valid_path.read_bytes() + b"extra"),
                          ("oversized", b"x" * (exporter.MAX_CHECKPOINT_BYTES + 1))):
        path = directory / (name + ".checkpoint")
        path.write_bytes(payload)
        result = subprocess.run([exe, "--checkpoint", str(path)], input=b"", capture_output=True, timeout=5)
        require(result.returncode == 2 and not result.stdout and result.stderr,
                "invalid checkpoint fails before protocol startup: " + name)
    missing = subprocess.run([exe, "--checkpoint", str(directory / "absent")],
                             input=b"", capture_output=True, timeout=5)
    require(missing.returncode == 2 and not missing.stdout and b"open" in missing.stderr,
            "missing checkpoint path reports open failure")


def verify_http_cli(exe, directory, checkpoint):
    tools = Path(__file__).resolve().parents[2] / "tools"
    spec = importlib.util.spec_from_file_location("economy_http_bridge", tools / "economy_http_bridge.py")
    bridge = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bridge)
    host = bridge.HostProcess([exe, "--role", "observer", "--checkpoint", str(checkpoint)])
    server = ThreadingHTTPServer(("127.0.0.1", 0), bridge.make_handler(host))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    output = directory / "http.checkpoint"
    command = [sys.executable, "-B", str(tools / "economy_checkpoint.py"), "--url",
               f"http://127.0.0.1:{server.server_address[1]}", "--output", str(output)]
    try:
        first = subprocess.run(command, capture_output=True, text=True, timeout=10)
        require(first.returncode == 0 and not first.stderr, "live HTTP CLI exports through query-only observer")
        receipt = json.loads(first.stdout)
        require(output.read_bytes() == checkpoint.read_bytes() and receipt["totalBytes"] == output.stat().st_size,
                "HTTP CLI preserves native checkpoint bytes")
        again = subprocess.run(command, capture_output=True, text=True, timeout=10)
        require(again.returncode == 1 and output.read_bytes() == checkpoint.read_bytes(),
                "HTTP CLI default preserves existing output")
        forced = subprocess.run([*command, "--force"], capture_output=True, text=True, timeout=10)
        require(forced.returncode == 0 and output.read_bytes() == checkpoint.read_bytes(),
                "HTTP CLI explicit force replaces completed checkpoint")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        host.close()
        require(host._proc.returncode == 0 and not host._proc.stderr.read(), "HTTP test host exits cleanly")


def main():
    require(len(sys.argv) == 2, "Usage: checkpoint_controller.py <OpenRepublicEconomyHost>")
    exe = sys.argv[1]
    exporter = load_exporter()
    with tempfile.TemporaryDirectory(prefix="openrepublic-checkpoint-test-") as temporary:
        directory = Path(temporary)
        catalog = directory / "synthetic.economy"
        catalog.write_text(CATALOG, encoding="utf-8")
        original = Client(exe, ["--catalog", str(catalog)])
        restored = None
        try:
            old_handle = setup(original)
            # Preserve the active tuned catalog, rather than depending on startup files or Lua history.
            tuned = original.call("reload_rules", text='return {schema_version=1, vehicles={truck={capacity_percent=200}}}')
            supports_lua = tuned["errorCode"] != "unsupported"
            require(not supports_lua or tuned["errorCode"] == "none", "active rules tuning accepted")
            before = world(original)
            path = directory / "republic.checkpoint"
            exporter.export_checkpoint(original, path)
            require(path.read_bytes().startswith(b"OPENREPUBLIC_ECONOMY_CHECKPOINT 1\n"), "native checkpoint written")
            require(world(original)["stateHash"] == before["stateHash"], "export does not advance or mutate world")
            require(original.call("poll", handle=old_handle)["state"] == "queued", "pending command stays pending")
            restored = Client(exe, ["--checkpoint", str(path)])
            require(restored.session != original.session, "restored host has a new session")
            stale = restored.request("world", sessionId=original.session)
            require(not stale["ok"] and stale["error"]["code"] == "stale_session", "old process session rejects")
            resumed = world(restored)
            require(resumed["stateHash"] == before["stateHash"] and resumed["tick"] == before["tick"] and
                    resumed["revision"] == before["revision"], "restart preserves exact state counters and pending hash")
            require(resumed["orders"][0]["cargo"] == "4" and resumed["orders"][0]["destination"] == 2,
                    "loaded truck resumes before pending retarget applies")
            require(restored.call("poll", handle=old_handle)["errorCode"] == "unknown_command",
                    "API handles do not migrate across sessions")
            require(restored.call("events", afterSequence="0", limit=32)["newestAvailableSequence"] == "0",
                    "API event journal starts fresh")
            require(restored.call("rules_history")["history"] == [], "old Lua source history not invented")
            require(restored.call("catalog") == original.call("catalog"), "active catalog exactly retained")
            for _ in range(32):
                step(original)
                step(restored)
                a, b = world(original), world(restored)
                require(a["stateHash"] == b["stateHash"], "continued process replay hashes agree every tick")
                require(sum(int(i["amount"]) for building in b["buildings"] for i in building["inventory"]) +
                        sum(int(v["cargo"]) for v in b["vehicles"]) == 12, "cargo conserved after restart")
            require(world(restored)["orders"][0]["destination"] == 3, "pending retarget applied after restore")
            if supports_lua:
                reload_result = restored.call("reload_rules", text='return {schema_version=1, vehicles={truck={capacity_percent=100}}}')
                require(reload_result["errorCode"] == "none", "restored host accepts rule reload")
                require(restored.call("catalog")["vehicleTypes"][0]["capacity"] == "8",
                        "future rules are relative to the embedded active catalog")
            startup_failures(exe, directory, path, exporter)
            verify_http_cli(exe, directory, path)
            print("PASS: native checkpoint restarts loaded cargo and pending retarget; 32 matched ticks, "
                  "12 units conserved, new session/handles/events, bounded startup and catalog binding verified.")
        finally:
            try:
                original.close()
            finally:
                if restored:
                    restored.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
