#!/usr/bin/env python3
"""Write a tiny OpenRepublic-authored GLB fixture for the Wicked Engine probe."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


def make_glb() -> bytes:
    # A two-triangle orange quad in the XY plane; it uses no external assets.
    positions = (
        (-1.0, -1.0, 0.0),
        (1.0, -1.0, 0.0),
        (1.0, 1.0, 0.0),
        (-1.0, 1.0, 0.0),
    )
    indices = (0, 1, 2, 0, 2, 3)
    binary = b"".join(struct.pack("<3f", *position) for position in positions)
    index_offset = len(binary)
    binary += struct.pack("<6H", *indices)
    binary += b"\0" * ((-len(binary)) % 4)

    document = {
        "asset": {"version": "2.0", "generator": "OpenRepublic synthetic fixture"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "OpenRepublicProbeQuad", "mesh": 0}],
        "meshes": [{"name": "OpenRepublicProbeMesh", "primitives": [{
            "attributes": {"POSITION": 0},
            "indices": 1,
            "material": 0,
            "mode": 4,
        }]}],
        "materials": [{"name": "OpenRepublicProbeOrange", "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 0.18, 0.025, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 0.65,
        }}],
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48, "target": 34962},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": 12, "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
             "min": [-1.0, -1.0, 0.0], "max": [1.0, 1.0, 0.0]},
            {"bufferView": 1, "componentType": 5123, "count": 6, "type": "SCALAR",
             "min": [0], "max": [3]},
        ],
    }
    json_chunk = json.dumps(document, separators=(",", ":")).encode("utf-8")
    json_chunk += b" " * ((-len(json_chunk)) % 4)
    total_length = 12 + 8 + len(json_chunk) + 8 + len(binary)
    return (
        struct.pack("<4sII", b"glTF", 2, total_length)
        + struct.pack("<I4s", len(json_chunk), b"JSON") + json_chunk
        + struct.pack("<I4s", len(binary), b"BIN\0") + binary
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path("build/openrepublic-probe/openrepublic_scene_probe.glb"))
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    payload = make_glb()
    args.output.write_bytes(payload)
    print(f"Wrote {args.output} ({len(payload)} bytes)")


if __name__ == "__main__":
    main()
