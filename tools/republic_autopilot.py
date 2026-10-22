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


def raw_resources(catalog, count_generated=False):
    """Resources made from nothing. With count_generated, ones that buildings also consume (for example
    electricity) are scored too; pure extraction outputs stay excluded."""
    consumed = {a["resource"] for b in catalog["buildingTypes"] for r in b["recipes"] for a in r["inputs"]}
    raw = set()
    for building in catalog["buildingTypes"]:
        for recipe in building["recipes"]:
            if not recipe["inputs"]:
                raw.update(o["resource"] for o in recipe["outputs"] if not (count_generated and o["resource"] in consumed and building["name"].startswith("powerplant")))
    return raw


def processed_units(world, raw):
    """Stored amount of every non-raw resource, in whole units (amounts are micro-units)."""
    total = 0
    for building in world["buildings"]:
        for item in building["inventory"]:
            if item["resource"] not in raw:
                total += int(item["amount"])
    return total / 1_000_000


def place_near(host, catalog, type_name, cell):
    """Places the building so its access cell sits on an existing road, nearest road cell first."""
    width = next(b["width"] for b in catalog["buildingTypes"] if b["name"] == type_name)
    cells = set()
    for road in host.call("world")["world"]["roads"]:
        if road["enabled"]:
            for end in ("a", "b"):
                cells.add((road[end]["x"], road[end]["z"]))
    candidates = sorted(cells, key=lambda c: abs(c[0] - cell["x"]) + abs(c[1] - cell["z"]))
    for rx, rz in candidates[:400]:
        revision = host.call("world")["world"]["revision"]
        sub = host.call("submit", revision=revision, command={
            "kind": "place_building", "type": type_name, "x": rx - (width - 1) // 2, "z": rz + 1})
        host.call("step", ticks=1)
        if host.call("poll", handle=sub["handle"])["state"] == "accepted":
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--ticks", type=int, default=500)
    parser.add_argument("--count-electricity", action="store_true",
                        help="also score stored output of power plants (comparable with REPUBLIC-1's definition)")
    parser.add_argument("--json", action="store_true", help="print one JSON result line (controller vs idle baseline)")
    args, host_args = parser.parse_known_args()
    host_args = [a for a in host_args if a != "--"]

    host = Host(args.host, host_args)
    ordered = set()
    catalog = host.call("catalog")
    placed = set()
    raw = raw_resources(catalog, args.count_electricity)
    try:
        for round_number in range(1, args.rounds + 1):
            plan = host.call("advise")
            submitted = 0
            world_now = host.call("world")["world"]
            batch = []
            for need in plan["unservedInputs"]:
                if not need["producers"]:
                    maker = raw_producer_type(catalog, need["resource"])
                    if maker and need["resource"] not in placed:
                        consumer = next(b for b in world_now["buildings"] if b["id"] == need["building"])
                        if place_near(host, catalog, maker, consumer["cell"]):
                            print(f"  placed a {maker} near building {consumer['id']} for {need['resource']}")
                            placed.add(need["resource"])
                            submitted += 1
                    continue
                command = need["producers"][0]["command"]
                key = (command["source"], command["destination"], command["resource"])
                if key in ordered:
                    continue
                batch.append({k: command[k] for k in ("source", "destination", "resource", "vehicleType")})
                ordered.add(key)
                submitted += 1
            for start in range(0, len(batch), 64):
                revision = host.call("world")["world"]["revision"]
                host.call("create_haul_orders", revision=revision, orders=batch[start:start + 64])
            stepped = host.call("step", ticks=args.ticks)
            world = host.call("world")["world"]
            running = sum(1 for b in world["buildings"] if b["stallReason"] == "running")
            print(f"round {round_number}: submitted {submitted} orders, tick {world['tick']}, "
                  f"{running}/{len(world['buildings'])} buildings running, {len(world['vehicles'])} trucks, "
                  f"score (stored non-raw units) {processed_units(world, raw):.1f}")
            blocked = [n for n in plan["unservedInputs"] if not n["producers"]]
            if blocked and round_number == 1:
                resources = sorted({n["resource"] for n in blocked})
                print("  cannot be served by any building in this world:", ", ".join(resources))
        for b in world["buildings"]:
            held = ", ".join(f"{i['resource']}={int(i['amount']) / 1_000_000:.1f}" for i in b["inventory"] if int(i["amount"]))
            print(f"  building {b['id']} {b['type']}: {b['stallReason']} [{held}]")
        for o in host.call("world")["world"]["orders"]:
            print(f"  order {o['id']} {o['source']}->{o['destination']} {o['resource']}: {o['state']}")
        final_score = processed_units(world, raw)
        final_tick = int(world["tick"])
        orders = len(world["orders"])
    finally:
        host.close()
    if args.json:
        # Idle baseline: same world and tick count with no commands, so the score is attributable.
        idle = Host(args.host, host_args)
        try:
            while int(idle.call("world")["world"]["tick"]) < final_tick:
                idle.call("step", ticks=min(500, final_tick - int(idle.call("world")["world"]["tick"])))
            idle_score = processed_units(idle.call("world")["world"], raw)
        finally:
            idle.close()
        print(json.dumps({"benchmark": "stored_non_raw_units", "tick": final_tick, "controller": final_score,
                          "idle": idle_score, "gain": final_score - idle_score, "orders": orders,
                          "generatorsPlaced": sorted(placed)}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
