#!/usr/bin/env python3
"""Rule-based baseline republic: the smallest autonomous controller for OpenRepublicEconomyHost.

It spawns the host as a controller, then repeats: ask `advise`, submit one haul order for every
unserved input that has a producer (nearest first), step, and report. It is a baseline for
LLM or learned controllers, not a claim about WRSR behaviour.

  python tools/republic_autopilot.py --host <OpenRepublicEconomyHost.exe> [--rounds 5] [--ticks 500]
         [host args after --]  e.g. -- --region <map dir> --content-root <media_soviet> --radius 450
"""
import argparse
import json
import subprocess
import sys


class Host:
    def __init__(self, exe, extra):
        self.proc = subprocess.Popen([exe, "--role", "controller", *extra], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1)
        self.next_id = 0

    def call(self, method, **params):
        self.next_id += 1
        request = {"protocolVersion": 0, "id": str(self.next_id), "method": method, **params}
        self.proc.stdin.write(json.dumps(request) + "\n")
        self.proc.stdin.flush()
        reply = json.loads(self.proc.stdout.readline())
        if not reply.get("ok"):
            raise RuntimeError(f"{method} failed: {reply.get('error')}")
        return reply["result"]

    def close(self):
        try:
            self.call("shutdown")
        except Exception:
            pass
        self.proc.wait(timeout=10)


def raw_producer_type(catalog, resource):
    """A building type that makes `resource` from nothing (extractor or generator), if any."""
    for building in catalog["buildingTypes"]:
        for recipe in building["recipes"]:
            if not recipe["inputs"] and any(o["resource"] == resource for o in recipe["outputs"]):
                return building["name"]
    return None


def place_near(host, type_name, cell):
    """Tries free-looking cells around `cell` (nearest first) until the host accepts a placement."""
    for ring in range(2, 30, 2):
        for dx, dz in ((ring, 0), (-ring, 0), (0, ring), (0, -ring), (ring, ring), (-ring, -ring), (ring, -ring), (-ring, ring)):
            revision = host.call("world")["world"]["revision"]
            sub = host.call("submit", revision=revision,
                            command={"kind": "place_building", "type": type_name, "x": cell["x"] + dx, "z": cell["z"] + dz})
            host.call("step", ticks=1)
            if host.call("poll", handle=sub["handle"])["state"] == "accepted":
                return True
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--ticks", type=int, default=500)
    args, host_args = parser.parse_known_args()
    host_args = [a for a in host_args if a != "--"]

    host = Host(args.host, host_args)
    ordered = set()
    catalog = host.call("catalog")
    placed = set()
    try:
        for round_number in range(1, args.rounds + 1):
            plan = host.call("advise")
            submitted = 0
            world_now = host.call("world")["world"]
            for need in plan["unservedInputs"]:
                if not need["producers"]:
                    maker = raw_producer_type(catalog, need["resource"])
                    if maker and need["resource"] not in placed:
                        consumer = next(b for b in world_now["buildings"] if b["id"] == need["building"])
                        if place_near(host, maker, consumer["cell"]):
                            print(f"  placed a {maker} near building {consumer['id']} for {need['resource']}")
                            placed.add(need["resource"])
                            submitted += 1
                    continue
                command = need["producers"][0]["command"]
                key = (command["source"], command["destination"], command["resource"])
                if key in ordered:
                    continue
                world = host.call("world")
                host.call("submit", revision=world["world"]["revision"], command=command)
                ordered.add(key)
                submitted += 1
            stepped = host.call("step", ticks=args.ticks)
            world = host.call("world")["world"]
            running = sum(1 for b in world["buildings"] if b["stallReason"] == "running")
            print(f"round {round_number}: submitted {submitted} orders, tick {world['tick']}, "
                  f"{running}/{len(world['buildings'])} buildings running, {len(world['vehicles'])} trucks")
            blocked = [n for n in plan["unservedInputs"] if not n["producers"]]
            if blocked and round_number == 1:
                resources = sorted({n["resource"] for n in blocked})
                print("  cannot be served by any building in this world:", ", ".join(resources))
    finally:
        host.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
