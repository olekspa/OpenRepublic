#!/usr/bin/env python3
"""Run the combined synthetic economy scale matrix in isolated processes."""
import argparse
import os
import pathlib
import platform
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bench", required=True, type=pathlib.Path)
    parser.add_argument("--ticks", type=int, default=10)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    rows = []
    for buildings in (1000, 5000, 20000):
        hashes = set()
        for threads in (1, 2, 4, 8):
            run = subprocess.run(
                [str(args.bench.resolve()), str(buildings), str(args.ticks), str(threads)],
                check=True, capture_output=True, text=True,
            )
            fields = run.stdout.strip().split(",")
            if len(fields) != 8:
                raise RuntimeError(f"unexpected benchmark output: {run.stdout!r}")
            size, configured_threads, ticks, actual, seconds, tps, peak_mib, state_hash = fields
            rows.append((size, configured_threads, ticks, actual, float(seconds), float(tps), peak_mib, state_hash))
            hashes.add(state_hash)
            print(f"completed {buildings:,} buildings at {threads} thread(s): {float(tps):.2f} ticks/s, {peak_mib} MiB")
        if len(hashes) != 1:
            raise RuntimeError(f"thread-count state hashes differ for {buildings}: {sorted(hashes)}")

    machine = f"{platform.platform()}; {platform.processor() or 'processor string unavailable'}; {os.cpu_count()} logical CPUs"
    lines = [
        "Machine: " + machine,
        "OpenRepublic synthetic combined scale workload; WRSR parity UNKNOWN.",
        "| Buildings requested | Buildings placed | Threads | Ticks | Seconds | Ticks/s | Peak working set (MiB) | State hash |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    lines.extend(
        f"| {int(size):,} | {int(actual):,} | {threads} | {ticks} | {seconds:.3f} | {tps:.2f} | {peak} | `{state_hash}` |"
        for size, threads, ticks, actual, seconds, tps, peak, state_hash in rows
    )
    table = "\n".join(lines) + "\n"
    if args.output:
        args.output.write_text(table, encoding="utf-8")
    else:
        print(table, end="")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.CalledProcessError, RuntimeError, ValueError) as exc:
        print(f"scale benchmark failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
