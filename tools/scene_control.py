#!/usr/bin/env python3
"""Tiny client for the OpenRepublic scene-control pipe (docs/SCENE_CONTROL.md).

  python tools/scene_control.py [--pipe or-scene] look.get
  python tools/scene_control.py look.preset '{"name":"dusk"}'
  python tools/scene_control.py camera.set '{"yawDegrees":-110,"elevationDegrees":14}'
  python tools/scene_control.py --day-cycle 10     # sweeps the sun through a day over ~10 s
"""
import argparse
import json
import math
import sys
import time


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pipe", default="or-scene")
    ap.add_argument("--day-cycle", type=float, default=0.0, help="seconds for one dawn-to-dusk sweep")
    ap.add_argument("method", nargs="?")
    ap.add_argument("params", nargs="?", default="{}")
    args = ap.parse_args()
    bs = chr(92)
    pipe = open(bs + bs + "." + bs + "pipe" + bs + args.pipe, "r+b", buffering=0)

    def call(method, params):
        pipe.write((json.dumps({"protocolVersion": 0, "id": "c", "method": method, "params": params}) + "\n").encode())
        return json.loads(pipe.readline().decode())

    if args.day_cycle > 0:
        steps = max(8, int(args.day_cycle * 4))
        for i in range(steps + 1):
            f = i / steps  # 0 dawn .. 1 dusk
            elevation = 2 + 68 * math.sin(math.pi * f)
            reply = call("look.set", {"sunElevationDegrees": round(elevation, 2), "sunYawOffsetDegrees": round(-90 + 180 * f, 1)})
            if not reply.get("ok"):
                print(reply)
                return 1
            time.sleep(args.day_cycle / steps)
        return 0
    if not args.method:
        ap.error("method or --day-cycle required")
    print(json.dumps(call(args.method, json.loads(args.params)), indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
