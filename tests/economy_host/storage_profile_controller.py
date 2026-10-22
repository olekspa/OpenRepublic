#!/usr/bin/env python3
"""Synthetic definition/profile -> region -> haul -> checkpoint process regression."""
import base64
from pathlib import Path
import struct
import sys
import tempfile

from checkpoint_controller import Client, accepted, require, step, submit, world


def placement(name, x):
    record = bytearray(0x548)
    encoded = name.encode("ascii")
    record[:len(encoded)] = encoded
    matrix = [1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., x, 0., 0., 1.]
    struct.pack_into("<16f", record, 0x31c, *matrix)
    return record


def main():
    exe = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="openrepublic-profile-") as folder:
        root = Path(folder)
        definitions = root / "buildings_types"
        definitions.mkdir()
        (definitions / "source.ini").write_text(
            "$PRODUCTION grain 24\n$STORAGE_EXPORT_SPECIAL BOX 2 grain\n")
        (definitions / "depot.ini").write_text("$STORAGE_IMPORT_SPECIAL BOX 3 grain\n")
        (root / "buildings_game.bin").write_bytes(
            struct.pack("<I", 2) + placement("source", 0.) + placement("depot", 32.))
        profile = root / "authored.profile"
        profile.write_text("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR BOX grain 1000000\n")
        args = ["--region", str(root), "--content-root", str(root), "--center", "0", "--radius", "100"]
        baseline = Client(exe, args)
        try:
            require(len(world(baseline)["buildings"]) == 1, "unmapped storage-only depot stays excluded")
        finally:
            baseline.close()
        client = Client(exe, [*args, "--storage-profile", str(profile)])
        restored = None
        try:
            catalog = client.call("catalog")
            types = {b["name"]: b for b in catalog["buildingTypes"]}
            require(types["source"]["storage"][0]["capacity"] == "2000000", "explicit producer capacity")
            require(types["depot"]["storage"][0]["capacity"] == "3000000" and
                    not types["depot"]["recipes"], "storage-only depot has capacity and no invented production")
            buildings = world(client)["buildings"]
            require(len(buildings) == 2, "both imported placements accepted")
            source, depot = buildings
            ax = source["cell"]["x"] + (types["source"]["width"] - 1) // 2
            bx = depot["cell"]["x"] + (types["depot"]["width"] - 1) // 2
            z = source["cell"]["z"] - 1
            handles = [submit(client, {"kind": "build_road", "ax": x, "az": z, "bx": x + 1, "bz": z})
                       for x in range(ax, bx)]
            step(client)
            accepted(client, handles)
            handle = submit(client, {"kind": "create_haul_order", "source": int(source["id"]),
                                     "destination": int(depot["id"]), "resource": "grain", "vehicleType": "truck"})
            step(client, 64)
            accepted(client, [handle])
            snapshot = world(client)
            require(snapshot["buildings"][1]["inventory"][0]["amount"] == "3000000",
                    "depot fills to explicit capacity: " + str(snapshot))
            export = client.call("begin_checkpoint", revision=snapshot["revision"])
            data = bytearray()
            while len(data) < export["totalBytes"]:
                chunk = client.call("read_checkpoint", checkpointId=export["checkpointId"],
                                    offset=len(data), limit=export["maxChunkBytes"])
                data.extend(base64.b64decode(chunk["dataBase64"]))
            checkpoint = root / "world.checkpoint"
            checkpoint.write_bytes(data)
            restored = Client(exe, ["--checkpoint", str(checkpoint)])
            require(world(restored)["stateHash"] == snapshot["stateHash"], "profile-derived world restores exactly")
            for _ in range(8):
                step(client)
                step(restored)
                require(world(client)["stateHash"] == world(restored)["stateHash"], "profile capacities survive replay")
        finally:
            if restored:
                restored.close()
            client.close()
    print("PASS: authored profile imports a storage-only depot, delivers to capacity and resumes identically.")


if __name__ == "__main__":
    main()
