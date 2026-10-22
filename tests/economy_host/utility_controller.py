#!/usr/bin/env python3
"""Exercise utility connections through a separate economy host process."""
import json
import argparse
import base64
from checkpoint_controller import Client as HostClient, accepted, step, submit, world as query_world
from pathlib import Path
import subprocess
import sys
import tempfile


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


class Client:
    def __init__(self, exe, catalog):
        self.proc = subprocess.Popen([exe, "--role", "controller", "--catalog", str(catalog)],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1)
        self.sequence = 0
        self.session = None
        hello = self.call("hello")
        self.session = hello["sessionId"]

    def request(self, method, **fields):
        self.sequence += 1
        request = {"protocolVersion": 0, "id": str(self.sequence), "method": method,
                   **fields}
        if self.session:
            request["sessionId"] = self.session
        self.proc.stdin.write(json.dumps(request) + "\n")
        self.proc.stdin.flush()
        line = self.proc.stdout.readline()
        require(bool(line), "host closed stdout before a response")
        response = json.loads(line)
        require(response.get("id") == str(self.sequence), "response id matches request")
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
            require(code == 0 and not errors, f"host exits cleanly: code={code}, stderr={errors}")
        finally:
            if self.proc.poll() is None:
                self.proc.kill()
                self.proc.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host")
    parser.add_argument("--write-checkpoint", type=Path)
    args = parser.parse_args()
    host = args.host
    catalog_text = """openrepublic-economy 1
utility water
resource grain
building plant 1 1
  recipe pump every 1 out water:5
end
building house 1 1
  store grain 20
  recipe consume every 1 in water:1 out grain:1
end
"""
    with tempfile.TemporaryDirectory(prefix="openrepublic-utility-") as directory:
        catalog = Path(directory) / "utility.catalog"
        catalog.write_text(catalog_text, encoding="utf-8")
        client = Client(host, catalog)
        try:
            hello = client.call("hello")
            require("connect_utility" in hello["actions"] and "disconnect_utility" in hello["actions"],
                    "capabilities advertise utility commands")
            resources = client.call("catalog")["resources"]
            require(resources[0] == {"id": 0, "name": "water", "utility": True},
                    "catalog exposes the utility resource kind")

            for kind, type_name, x in (("place_building", "plant", 1),
                                       ("place_building", "house", 4)):
                world = client.call("world")["world"]
                command = {"kind": kind, "type": type_name, "x": x, "z": 1}
                result = client.call("submit", revision=world["revision"], command=command)
                require(result["errorCode"] == "none", "fixture building queued")
            client.call("step", ticks=1)
            world = client.call("world")["world"]
            require(len(world["buildings"]) == 2, "fixture buildings placed")
            a, b = (entry["id"] for entry in world["buildings"])

            result = client.call("submit", revision=world["revision"], command={
                "kind": "connect_utility", "resource": "water", "a": b, "b": a})
            require(result["errorCode"] == "none" and result["state"] == "queued",
                    "utility connection queued")
            require(client.call("world")["world"]["utilityConnections"] == [],
                    "connection remains pending until a tick")
            client.call("step", ticks=1)
            connected = client.call("world")["world"]
            require(connected["utilityConnections"] == [{"resource": "water", "a": a, "b": b}],
                    "world query shows canonical connection after tick")
            house = next(building for building in connected["buildings"] if building["type"] == "house")
            grain = next(item for item in house["inventory"] if item["resource"] == "grain")
            require(grain["amount"] == "1",
                    "utility producer supplies connected consumer through the authoritative tick")
            page = client.call("world", revision=connected["revision"], kind="utility_connections",
                               offset=0, limit=1)
            require(page["total"] == 1 and page["entities"] == connected["utilityConnections"],
                    "utility connections are returned through the paged protocol")

            result = client.call("submit", revision=connected["revision"], command={
                "kind": "disconnect_utility", "resource": "water", "a": b, "b": a})
            require(result["errorCode"] == "none", "utility disconnection queued")
            client.call("step", ticks=1)
            require(client.call("world")["world"]["utilityConnections"] == [],
                    "disconnection removes the link")
        finally:
            client.close()
    run_power_chain(host, args.write_checkpoint)


def export(client):
    header = client.call("begin_checkpoint", revision=query_world(client)["revision"])
    data = bytearray()
    while len(data) < header["totalBytes"]:
        chunk = client.call("read_checkpoint", checkpointId=header["checkpointId"],
                            offset=len(data), limit=header["maxChunkBytes"])
        data.extend(base64.b64decode(chunk["dataBase64"]))
    return bytes(data)


def run_power_chain(host, output):
    """Authored fuel -> power -> ore -> iron, with no utility inventory."""
    catalog_text = """openrepublic-economy 1
resource coal
resource ore
resource iron
utility power
building fuel_source 1 1
  store coal 16
  recipe quarry every 1 out coal:1
end
building generator 1 1
  store coal 12
  recipe generate every 1 in coal:1 out power:3
end
building powered_mine 1 1
  store ore 16
  recipe extract every 1 in power:2 out ore:2
end
building powered_processor 1 1
  store ore 12
  store iron 12
  recipe process every 1 in ore:2 power:1 out iron:1
end
building depot 1 1
  store iron 40
end
vehicle truck capacity 8 speed 4
"""
    with tempfile.TemporaryDirectory(prefix="openrepublic-power-chain-") as directory:
        catalog = Path(directory) / "powered.catalog"
        catalog.write_text(catalog_text)
        client = HostClient(host, ["--catalog", str(catalog)])
        resumed = None
        try:
            names = ["fuel_source", "generator", "powered_mine", "powered_processor", "depot"]
            handles = [submit(client, {"kind": "place_building", "type": name, "x": 4 + i * 8, "z": 8})
                       for i, name in enumerate(names)]
            handles.extend(submit(client, {"kind": "build_road", "ax": x, "az": 7, "bx": x + 1, "bz": 7})
                           for x in range(4, 36))
            step(client)
            accepted(client, handles)
            require("power" in client.call("explain", buildingId=3)["explanation"], "mine reports power shortage")
            handles = [submit(client, {"kind": "create_haul_order", "source": source, "destination": destination,
                                       "resource": resource, "vehicleType": "truck"})
                       for source, destination, resource in ((1, 2, "coal"), (3, 4, "ore"), (4, 5, "iron"))]
            handles.extend(submit(client, {"kind": "connect_utility", "resource": "power", "a": 2, "b": b})
                           for b in (3, 4))
            step(client, 64)
            accepted(client, handles)
            powered = query_world(client)
            require(len(powered["utilityConnections"]) == 2, "two exact utility links")
            amount = int(powered["buildings"][4]["inventory"][0]["amount"])
            require(amount > 0, "fuel-powered chain delivers iron to depot")
            require(all(item["resource"] != "power" for b in powered["buildings"] for item in b["inventory"]),
                    "utility has no warehouse stock")
            handle = submit(client, {"kind": "disconnect_utility", "resource": "power", "a": 2, "b": 3})
            step(client)
            accepted(client, [handle])
            require("power" in client.call("explain", buildingId=3)["explanation"], "disconnected mine reports shortage")
            handle = submit(client, {"kind": "connect_utility", "resource": "power", "a": 2, "b": 3})
            step(client)
            accepted(client, [handle])
            checkpoint = Path(directory) / "powered.checkpoint"
            checkpoint.write_bytes(export(client))
            resumed = HostClient(host, ["--checkpoint", str(checkpoint)])
            require(query_world(resumed)["stateHash"] == query_world(client)["stateHash"], "network checkpoint hash matches")
            for _ in range(16):
                step(client)
                step(resumed)
                require(query_world(client)["stateHash"] == query_world(resumed)["stateHash"], "utility replay matches")
            if output:
                with output.open("xb") as destination:
                    destination.write(export(client))
            print(f"PASS: fuel-powered chain delivered {amount} iron; isolation and 16 replay ticks match.")
        finally:
            if resumed:
                resumed.close()
            client.close()


if __name__ == "__main__":
    main()
