#!/usr/bin/env python3
"""Compare cargo-preserving baseline replanning with an identical idle fixture."""

import importlib.util
import pathlib
import sys
import tempfile


def require(value, message):
    if not value:
        raise RuntimeError(message)


def load_autopilot():
    path = pathlib.Path(__file__).resolve().parents[2] / "tools" / "republic_autopilot.py"
    spec = importlib.util.spec_from_file_location("republic_autopilot", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


CATALOG = """openrepublic-economy 1
resource grain
building source 1 1
  store grain 100
  recipe harvest every 1 out grain:1
end
building depot 1 1
  store grain 4
end
vehicle truck capacity 4 speed 1
"""


def total(world):
    return sum(int(item["amount"]) for building in world["buildings"]
               for item in building["inventory"]) + sum(int(v["cargo"]) for v in world["vehicles"])


def stored(world, building_id):
    return sum(int(item["amount"]) for b in world["buildings"] if b["id"] == building_id
               for item in b["inventory"])


def setup(host):
    revision = host.call("world")["world"]["revision"]
    handles = []
    commands = [{"kind": "place_building", "type": type_name, "x": x, "z": 2}
                for type_name, x in (("source", 1), ("source", 3), ("depot", 8), ("depot", 12))]
    commands += [{"kind": "build_road", "ax": x, "az": 1, "bx": x + 1, "bz": 1}
                 for x in range(1, 12)]
    for command in commands:
        reply = host.call("submit", revision=revision, command=command)
        require(reply["errorCode"] == "none", "fixture command admitted")
        handles.append(reply["handle"])
    require(host.call("step", ticks=12)["errorCode"] == "none", "fixture production step")
    for handle in handles:
        require(host.call("poll", handle=handle)["state"] == "accepted", "fixture command applied")
    revision = host.call("world")["world"]["revision"]
    handles = []
    for source in (1, 2):
        for command in ({"kind": "set_building_enabled", "buildingId": source, "enabled": False},
                        {"kind": "create_haul_order", "source": source, "destination": 3,
                         "resource": "grain", "vehicleType": "truck"}):
            reply = host.call("submit", revision=revision, command=command)
            require(reply["errorCode"] == "none", "queue fixture loaded truck")
            handles.append(reply["handle"])
    require(host.call("step", ticks=1)["errorCode"] == "none", "load both trucks before depot fills")
    for handle in handles:
        require(host.call("poll", handle=handle)["state"] == "accepted", "loaded fixture applied")
    for _ in range(12):
        world = host.call("world")["world"]
        order = next(o for o in world["orders"] if o["id"] == 1)
        if order["failure"] == "destination_full" and int(order["cargo"]) == 4:
            require(total(world) == 24 and stored(world, 3) == 4 and stored(world, 4) == 0,
                    "race fixture conserves all 24 units, one loaded truck waiting")
            return world
        host.call("step", ticks=1)
    raise RuntimeError("fixture did not reach a loaded destination_full stall")


def main():
    require(len(sys.argv) == 2, "Usage: replanning_controller.py <OpenRepublicEconomyHost>")
    autopilot = load_autopilot()
    with tempfile.TemporaryDirectory(prefix="openrepublic-replanning-") as directory:
        catalog_path = pathlib.Path(directory) / "synthetic.economy"
        catalog_path.write_text(CATALOG, encoding="utf-8")
        controller = autopilot.Host(sys.argv[1], ["--catalog", str(catalog_path)])
        idle = autopilot.Host(sys.argv[1], ["--catalog", str(catalog_path)])
        try:
            before = setup(controller)
            baseline = setup(idle)
            require(before["stateHash"] == baseline["stateHash"], "matched starting world hashes")
            plan = controller.call("advise")
            require(len(plan["retargets"]) == 1 and plan["retargets"][0]["destination"] == 4,
                    "adviser chooses empty reachable alternative")
            result = autopilot.run_round(controller, controller.call("catalog"), 1, set())
            require(result["accepted_retargets"] == 1 and result["rejected_retargets"] == 0,
                    "baseline controller polls accepted cargo-preserving retarget")
            require(result["accepted_creates"] == 0, "existing truck used without a new order")
            after = result["world"]
            current = next(o for o in after["orders"] if o["id"] == 1)
            original = next(o for o in before["orders"] if o["id"] == 1)
            require(current["vehicleId"] == original["vehicleId"] and current["destination"] == 4,
                    "retarget retains truck and order identity")
            require(len(after["vehicles"]) == 2 and int(current["cargo"]) == 4,
                    "retarget preserves loaded cargo before arrival")
            for _ in range(8):
                require(total(after) == 24, "all stored and onboard grain conserved")
                if stored(after, 4) == 4:
                    break
                controller.call("step", ticks=1)
                after = controller.call("world")["world"]
            require(stored(after, 4) == 4 and total(after) == 24, "redirected cargo delivered intact")
            idle.call("step", ticks=int(after["tick"]) - int(baseline["tick"]))
            idle_world = idle.call("world")["world"]
            require(total(idle_world) == 24 and stored(idle_world, 4) == 0,
                    "idle control conserves cargo but does not use alternative storage")
            require(stored(after, 3) == stored(idle_world, 3) == 4,
                    "original destination stock unchanged between controls")
            require(controller.call("advise")["retargets"] == [], "delivered order needs no replan")
            print(f"PASS: matched tick {after['tick']}; replanning delivered 4 vs idle 0; "
                  "24 total units conserved, original truck/order retained.")
        finally:
            try:
                controller.close()
            finally:
                idle.close()
            require(controller.proc.returncode == idle.proc.returncode == 0,
                    "both synthetic host processes shut down with exit zero")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
