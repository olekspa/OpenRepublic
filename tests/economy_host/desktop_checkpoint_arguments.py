#!/usr/bin/env python3
"""Reject malformed desktop native-economy startup before creating a GPU window."""
from pathlib import Path
import subprocess
import sys
import tempfile


def rejected(exe, args, message):
    result = subprocess.run([exe, *args], capture_output=True, timeout=5)
    if result.returncode != 2 or b"checkpoint startup failed" not in result.stderr:
        raise RuntimeError(f"{message}: exit={result.returncode}; stderr={result.stderr!r}")


def rejected_native(exe, args, message):
    result = subprocess.run([exe, *args], capture_output=True, timeout=5)
    if result.returncode != 2 or b"native economy startup failed" not in result.stderr:
        raise RuntimeError(f"{message}: exit={result.returncode}; stderr={result.stderr!r}")


def rejected_profile(exe, args, message):
    result = subprocess.run([exe, *args], capture_output=True, timeout=5)
    if result.returncode != 2 or b"storage profile startup failed" not in result.stderr:
        raise RuntimeError(f"{message}: exit={result.returncode}; stderr={result.stderr!r}")


def main():
    exe = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="openrepublic-desktop-checkpoint-args-") as folder:
        directory = Path(folder)
        absent = str(directory / "absent.checkpoint")
        rejected_profile(exe, ["--storage-profile"], "missing storage profile path")
        rejected_profile(exe, ["--storage-profile", ""], "empty storage profile path")
        rejected_profile(exe, ["--storage-profile", absent], "profile requires imported region")
        region_args = ["--map-package", str(directory), "--content-root", str(directory), "--map-region"]
        rejected_profile(exe, [*region_args, "--storage-profile", absent], "missing storage profile file")
        malformed = directory / "malformed.profile"
        malformed.write_text("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR BOX grain nan\n")
        rejected_profile(exe, [*region_args, "--storage-profile", str(malformed)], "invalid storage factor")
        rejected_profile(exe, [*region_args, "--storage-profile", absent, "--storage-profile", absent],
                         "duplicate storage profile")
        rejected_profile(exe, [*region_args, "--storage-profile", absent, "--economy-url", "http://127.0.0.1:8950"],
                         "remote economy owns its catalog")
        rejected(exe, ["--economy-checkpoint"], "missing path")
        rejected(exe, ["--economy-checkpoint", ""], "empty path")
        rejected(exe, ["--economy-checkpoint", absent], "missing file")
        rejected(exe, ["--economy-checkpoint", absent, "--economy-checkpoint", absent], "duplicate path")
        rejected_native(exe, ["--native-economy", "--native-economy"], "duplicate native economy flag")
        rejected_native(exe, ["--native-economy", "--economy-checkpoint", absent],
                        "native economy and checkpoint conflict")
        for name, content in (("empty", b""), ("invalid", b"invalid checkpoint"),
                              ("oversized", b"x" * (2 * 1024 * 1024 + 129))):
            path = directory / (name + ".checkpoint")
            path.write_bytes(content)
            rejected(exe, ["--economy-checkpoint", str(path)], name)
        for flag, values in (("--synthetic-region-fixture", []), ("--content-root", ["missing"]),
                             ("--map-package", ["missing"]), ("--map-region", []),
                             ("--map-building", ["0"]), ("--map-radius", ["0"]),
                             ("--nmf-probe", ["missing"]), ("--scene-probe", ["missing"]),
                             ("--terrain-probe", ["missing"]), ("--glb-probe", ["missing"]),
                             ("--economy-url", ["http://127.0.0.1:8950"]),
                             ("--scene-pipe-name", ["not-opened"]),
                             ("--capture-preview", ["missing.png"]), ("--nmf-unlit-preview", []),
                             ("--storage-profile", ["missing.profile"])):
            rejected(exe, ["--economy-checkpoint", absent, flag, *values], "exclusive flag " + flag)
            rejected_native(exe, ["--native-economy", flag, *values], "exclusive native economy flag " + flag)
    print("PASS: desktop checkpoint/native-economy startup rejects bad arguments before UI initialization.")


if __name__ == "__main__":
    main()
