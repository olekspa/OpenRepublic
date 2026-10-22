#!/usr/bin/env python3
"""Exercise the economy JSONL host as an independent local controller."""

import json
import subprocess
import sys


class Client:
    def __init__(self, executable):
        self.process = subprocess.Popen(
            [executable, "--role", "controller"], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            encoding="utf-8", bufsize=1,
        )
        self.next_id = 0

    def call(self, method, **fields):
        self.next_id += 1
        request_id = str(self.next_id)
        request = {"id": request_id, "protocolVersion": 0, "method": method, **fields}
        self.process.stdin.write(json.dumps(request, separators=(",", ":")) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError("economy host closed stdout: " + self.process.stderr.read())
        response = json.loads(line)
        if response.get("id") != request_id:
            raise RuntimeError("response ID mismatch")
        if not response.get("ok"):
            raise RuntimeError("protocol error: " + repr(response.get("error")))
        return response["result"]

    def close(self):
        if self.process.poll() is None:
            self.call("shutdown")
        self.process.stdin.close()
        status = self.process.wait(timeout=5)
        stderr = self.process.stderr.read()
        if status != 0 or stderr:
            raise RuntimeError(f"host close failed ({status}): {stderr}")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify_oversized_line(executable):
    completed = subprocess.run(
        [executable], input=b"x" * (64 * 1024 + 1), capture_output=True, timeout=5
    )
    lines = completed.stdout.splitlines()
    require(completed.returncode == 5, "oversized input exits with status 5")
    require(len(lines) == 1, "oversized input produces one error response")
    response = json.loads(lines[0])
    require(response.get("error", {}).get("code") == "request_too_large",
            "oversized input reports request_too_large")
    require(not completed.stderr, "oversized request writes no diagnostics")


def main():
    if len(sys.argv) != 2:
        print("Usage: economy_controller.py <OpenRepublicEconomyHost>", file=sys.stderr)
        return 2
    client = Client(sys.argv[1])
    try:
        verify_oversized_line(sys.argv[1])
        hello = client.call("hello")
        require(hello["role"] == "controller", "controller handshake")
        catalog = client.call("catalog")
        building_types = {item["name"]: item for item in catalog["buildingTypes"]}
        for name in ("farm", "mill", "bakery", "market"):
            require(name in building_types, "synthetic catalog missing " + name)
        vehicle = catalog["vehicleTypes"][0]["name"]
        resources = {item["name"] for item in catalog["resources"]}
        require({"grain", "flour", "bread"}.issubset(resources), "synthetic resources missing")

        def world():
            return client.call("world")["world"]

        state = world()
        origins = [(2, 8), (10, 8), (18, 8), (26, 8)]
        handles = []
        for name, (x, z) in zip(("farm", "mill", "bakery", "market"), origins):
            result = client.call("submit", revision=state["revision"],
                                 command={"kind": "place_building", "type": name, "x": x, "z": z})
            require(result["errorCode"] == "none", "place " + name)
            handles.append(result["handle"])
        require(client.call("step", ticks=1)["errorCode"] == "none", "apply placements")
        for handle in handles:
            require(client.call("poll", handle=handle)["state"] == "accepted", "building accepted")
        state = world()
        buildings = {building["type"]: building for building in state["buildings"]}
        require(set(buildings) == {"farm", "mill", "bakery", "market"}, "all four buildings placed")

        # Synthetic building access cells sit on the north edge; connect the
        # four access cells with a deterministic, one-cell-wide road chain.
        accesses = []
        for name, building in buildings.items():
            definition = building_types[name]
            cell = building["cell"]
            accesses.append((cell["x"] + (definition["width"] - 1) // 2, cell["z"] - 1))
        accesses.sort(key=lambda cell: cell[0])
        require(len({cell[1] for cell in accesses}) == 1,
                "synthetic access cells share a road row")
        road_row = accesses[0][1]
        road_cells = [(x, road_row) for x in range(accesses[0][0], accesses[-1][0] + 1)]
        handles = []
        state = world()
        for a, b in zip(road_cells, road_cells[1:]):
            if a[1] != b[1] or abs(a[0] - b[0]) != 1:
                continue
            result = client.call("submit", revision=state["revision"], command={
                "kind": "build_road", "ax": a[0], "az": a[1], "bx": b[0], "bz": b[1]})
            require(result["errorCode"] == "none", "queue road")
            handles.append(result["handle"])
        require(handles, "road chain generated")
        require(client.call("step", ticks=1)["errorCode"] == "none", "apply roads")
        for handle in handles:
            require(client.call("poll", handle=handle)["state"] == "accepted", "road accepted")

        state = world()
        chain = [("farm", "mill", "grain"), ("mill", "bakery", "flour"),
                 ("bakery", "market", "bread")]
        for source, destination, resource in chain:
            result = client.call("submit", revision=state["revision"], command={
                "kind": "create_haul_order", "source": buildings[source]["id"],
                "destination": buildings[destination]["id"], "resource": resource,
                "vehicleType": vehicle})
            require(result["errorCode"] == "none", "queue " + resource + " haul")
            handles.append(result["handle"])
        require(client.call("step", ticks=1)["errorCode"] == "none", "apply haul orders")
        for handle in handles[-3:]:
            require(client.call("poll", handle=handle)["state"] == "accepted", "haul accepted")

        delivered = False
        for _ in range(400):
            result = client.call("step", ticks=32)
            require(result["errorCode"] == "none", "bounded production step")
            state = world()
            market = next(building for building in state["buildings"] if building["type"] == "market")
            if any(item["resource"] == "bread" and int(item["amount"]) > 0
                   for item in market["inventory"]):
                delivered = True
                break
        require(delivered, "bread delivered to market")

        state = world()
        bakery = next(building for building in state["buildings"] if building["type"] == "bakery")
        result = client.call("submit", revision=state["revision"], command={
            "kind": "set_building_enabled", "buildingId": bakery["id"], "enabled": False})
        require(result["errorCode"] == "none", "disable bakery")
        require(client.call("step", ticks=1)["errorCode"] == "none", "apply disable")
        explanation = client.call("explain", buildingId=bakery["id"])
        require(explanation["reason"] == "disabled" and explanation["explanation"],
                "disabled bakery has a stall explanation")
        client.close()
        print("PASS: catalog-driven controller delivered bread and observed a disabled bakery stall.")
        return 0
    finally:
        if client.process.poll() is None:
            client.process.kill()
            client.process.wait(timeout=5)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("FAIL: " + str(error), file=sys.stderr)
        raise SystemExit(1)
