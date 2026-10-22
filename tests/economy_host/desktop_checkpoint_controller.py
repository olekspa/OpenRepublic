#!/usr/bin/env python3
"""Manual GPU acceptance helper: make an authored fixture, then verify the live desktop pipe.

This is deliberately outside automatic CTest: starting the desktop requires a foreground/GPU run.
"""
import argparse
import json
from pathlib import Path

from checkpoint_controller import Client, accepted, require, step, submit, world, load_exporter


def make_fixture(host, output):
    client = Client(host, [])
    try:
        handles = []
        for name, x, z in (("farm", 4, 8), ("farm", 40, 44), ("mill", 46, 48),
                           ("bakery", 52, 48), ("farm", 58, 44)):
            handles.append(submit(client, {"kind": "place_building", "type": name, "x": x, "z": z}))
        for x in range(40, 58):
            handles.append(submit(client, {"kind": "build_road", "ax": x, "az": 43, "bx": x + 1, "bz": 43}))
        for z in range(43, 47):
            handles.append(submit(client, {"kind": "build_road", "ax": 46, "az": z, "bx": 46, "bz": z + 1}))
        for x in range(46, 52):
            handles.append(submit(client, {"kind": "build_road", "ax": x, "az": 47, "bx": x + 1, "bz": 47}))
        step(client, 12)
        accepted(client, handles)
        handles = [submit(client, {"kind": "remove_building", "buildingId": 1})]
        for building in (2, 3, 4, 5):
            handles.append(submit(client, {"kind": "set_building_enabled", "buildingId": building, "enabled": False}))
        step(client)
        accepted(client, handles)
        handle = submit(client, {"kind": "create_haul_order", "source": 2, "destination": 5,
                                 "resource": "grain", "vehicleType": "truck"})
        step(client)
        accepted(client, [handle])
        require(world(client)["orders"][0]["cargo"] == "4", "authored fixture has loaded cargo")
        # Startup displays IDs 2,3,4,5. The first step must reconcile IDs 2,3,5,6 and new roads.
        submit(client, {"kind": "retarget_haul_order", "orderId": 1, "destination": 3})
        submit(client, {"kind": "remove_building", "buildingId": 4})
        submit(client, {"kind": "place_building", "type": "market", "x": 60, "z": 48})
        for x in range(52, 60):
            submit(client, {"kind": "build_road", "ax": x, "az": 47, "bx": x + 1, "bz": 47})
        road = next(r for r in world(client)["roads"] if r["a"] == {"x": 49, "z": 47})
        submit(client, {"kind": "set_road_enabled", "roadId": road["id"], "enabled": False})
        receipt = load_exporter().export_checkpoint(client, output)
        print(json.dumps({"fixture": receipt, "world": world(client)}, sort_keys=True))
    finally:
        client.close()


def verify(host, checkpoint, pipe_name, output):
    exporter = load_exporter()
    transport = exporter.PipeTransport(pipe_name)
    reference = None
    try:
        desktop = exporter.RpcClient(transport)
        reference = Client(host, ["--checkpoint", checkpoint])
        initial = world(desktop)
        require(initial["stateHash"] == world(reference)["stateHash"], "desktop starts at exact checkpoint hash")
        require([b["id"] for b in initial["buildings"]] == [2, 3, 4, 5], "gapped source IDs preserved")
        require(initial["orders"][0]["cargo"] == "4", "loaded cargo retained before first step")
        for _ in range(4):
            step(desktop)
            step(reference)
            current = world(desktop)
            require(current["stateHash"] == world(reference)["stateHash"], "desktop and host continuation match")
            total = sum(int(i["amount"]) for b in current["buildings"] for i in b["inventory"])
            total += sum(int(v["cargo"]) for v in current["vehicles"])
            require(total == 24, "all 24 stored/onboard units conserved after empty-building removal")
        require([b["id"] for b in current["buildings"]] == [2, 3, 5, 6], "queued remove/add applied with exact IDs")
        require(current["orders"][0]["destination"] == 3, "pending retarget applied")
        require(any(not r["enabled"] for r in current["roads"]), "pending road closure applied")
        receipt = exporter.export_checkpoint(desktop, output)
        saved = Client(host, ["--checkpoint", output])
        try:
            require(world(saved)["stateHash"] == current["stateHash"], "live pipe export resumes exact app state")
        finally:
            saved.close()
        print(json.dumps({"verified": True, "tick": current["tick"], "stateHash": current["stateHash"],
                          "buildings": [b["id"] for b in current["buildings"]],
                          "vehicles": current["vehicles"], "export": receipt}, sort_keys=True))
    finally:
        transport.close()
        if reference:
            reference.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("make", "verify"))
    parser.add_argument("--host", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--checkpoint")
    parser.add_argument("--pipe")
    args = parser.parse_args()
    if args.mode == "make":
        make_fixture(args.host, args.output)
    else:
        if not args.checkpoint or not args.pipe:
            parser.error("verify requires --checkpoint and --pipe")
        verify(args.host, args.checkpoint, args.pipe, args.output)


if __name__ == "__main__":
    main()
