#!/usr/bin/env python3
"""Read-only, anonymized terrain and placement survey for local map packages.

The parser supports the same bounded legacy R32F heightmap and observed
placement layouts as OpenRepublic's current content readers. It is a local
research aid, not a general or fully verified WRSR package decoder.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import os
import re
import struct
import zlib
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

REQUIRED = ("script.ini", "heightmap.dds", "buildings_game.bin")
MAX_HEIGHTMAP_BYTES = 256 * 1024 * 1024
MAX_BUILDINGS_BYTES = 512 * 1024 * 1024
MAX_TREES_BYTES = 64 * 1024 * 1024
MAX_ROADS_BYTES = 256 * 1024 * 1024
PLACEMENT_HEADER = 0x548
PLACEMENT_MATRIX = 0x31C
TREE_RECORD = 17


class SurveyError(Exception):
    pass


@dataclass
class Terrain:
    width: int
    height: int
    values: list[list[float]]
    world_x: float
    world_y: float
    world_z: float
    size_x: float
    size_z: float
    height_scale: float
    sample_stride: int

    @property
    def spacing_x(self) -> float:
        return self.size_x / max(1, self.width - 1)

    @property
    def spacing_z(self) -> float:
        return self.size_z / max(1, self.height - 1)

    def world_height(self, raw: float) -> float:
        return self.world_y + raw * self.height_scale

    def world_x_at(self, col: int) -> float:
        return self.world_x - self.size_x / 2 + col * self.spacing_x

    def world_z_at(self, row: int) -> float:
        return self.world_z - self.size_z / 2 + row * self.spacing_z


def _read_metadata(path: Path, width: int, height: int) -> tuple[float, ...]:
    raw = path.read_bytes()
    if len(raw) > 65536:
        raise SurveyError("script.ini exceeds 64 KiB")
    text = raw.decode("utf-8-sig", errors="strict")
    wanted = {"WORLDSIZE": 2, "WORLDPOSITION": 3, "HEIGHTSCALE": 1, "HEIGHTMAPSIZE": 2}
    found: dict[str, tuple[float, ...]] = {}
    for line in text.splitlines():
        match = re.match(r"^\s*\$([A-Za-z0-9_]+)\s+(.+?)\s*$", line)
        if not match:
            continue
        key = match.group(1).upper()
        if key not in wanted:
            continue
        tokens = match.group(2).split()
        if len(tokens) != wanted[key]:
            raise SurveyError(f"malformed ${key}")
        try:
            values = tuple(float(t) for t in tokens)
        except ValueError as exc:
            raise SurveyError(f"invalid ${key}") from exc
        if not all(math.isfinite(v) and abs(v) <= 1e9 for v in values):
            raise SurveyError(f"non-finite or implausible ${key}")
        found[key] = values
    if set(found) != set(wanted):
        raise SurveyError("script.ini missing terrain metadata")
    if found["HEIGHTMAPSIZE"] != (float(width), float(height)):
        raise SurveyError("$HEIGHTMAPSIZE does not match DDS dimensions")
    sx, sz = found["WORLDSIZE"]
    if sx <= 0 or sz <= 0 or found["HEIGHTSCALE"][0] == 0:
        raise SurveyError("world dimensions and height scale must be nonzero")
    px, py, pz = found["WORLDPOSITION"]
    return sx, sz, px, py, pz, found["HEIGHTSCALE"][0]


def read_terrain(folder: Path, target: int) -> Terrain:
    path = folder / "heightmap.dds"
    size = path.stat().st_size
    if size < 128 or size > MAX_HEIGHTMAP_BYTES:
        raise SurveyError("DDS file size is outside the supported bounds")
    with path.open("rb") as f:
        head = f.read(128)
        if head[:4] != b"DDS ":
            raise SurveyError("DDS magic is missing")
        h = struct.unpack_from("<7I", head, 4)
        header_size, flags, height, width, pitch, depth, mip = h
        pf_size, pf_flags, fourcc = struct.unpack_from("<3I", head, 76)
        caps, caps2 = struct.unpack_from("<2I", head, 108)
        if (header_size != 124 or pf_size != 32 or flags & 0x100F != 0x100F
                or pf_flags != 4 or fourcc != 114 or depth != 0 or mip > 1
                or pitch != width * 4 or width == 0 or height == 0
                or width * height > 64 * 1024 * 1024
                or caps & 0x1000 == 0 or caps & (0x8 | 0x400000)
                or caps2 & (0xFE00 | 0x200000)):
            raise SurveyError("unsupported legacy R32F DDS layout")
        if size != 128 + width * height * 4:
            raise SurveyError("DDS payload size does not match dimensions")
        payload = f.read()
    params = _read_metadata(folder / "script.ini", width, height)
    stride = max(1, math.ceil(max(width, height) / target))
    grid = []
    for row in range(0, height, stride):
        sampled = []
        for col in range(0, width, stride):
            value = struct.unpack_from("<f", payload, (row * width + col) * 4)[0]
            if not math.isfinite(value):
                raise SurveyError("sampled heightmap contains NaN or infinity")
            sampled.append(value)
        grid.append(sampled)
    sx, sz, px, py, pz, scale = params
    return Terrain(width, height, grid, px, py, pz, sx, sz, scale, stride)


def read_buildings(path: Path) -> list[tuple[float, float, float]]:
    size = path.stat().st_size
    if size < 4 or size > MAX_BUILDINGS_BYTES:
        raise SurveyError("buildings_game.bin exceeds the 512 MiB survey limit")
    data = path.read_bytes()
    count = struct.unpack_from("<I", data)[0]
    if count > 2_000_000:
        raise SurveyError("building count exceeds decoder limit")
    out: list[tuple[float, float, float]] = []
    offset = 4
    for _ in range(count):
        while offset + PLACEMENT_HEADER <= len(data):
            head = data[offset:offset + 64]
            end = head.find(b"\0")
            if 3 <= end <= 63 and re.fullmatch(rb"[A-Za-z0-9_]+", head[:end]):
                m = struct.unpack_from("<16f", data, offset + PLACEMENT_MATRIX)
                if (all(math.isfinite(v) for v in m)
                        and abs(m[5] - 1) < 1e-3 and abs(m[15] - 1) < 1e-3
                        and all(abs(m[k]) < 1e-4 for k in (1, 3, 7, 9, 11, 4, 6))
                        and abs(m[0] * m[0] + m[2] * m[2] - 1) < 1e-2
                        and abs(m[8] * m[8] + m[10] * m[10] - 1) < 1e-2
                        and all(abs(v) <= 1_000_000 for v in m[12:15])):
                    out.append((m[12], m[13], m[14]))
                    offset += PLACEMENT_HEADER
                    break
            offset += 1
        else:
            raise SurveyError(f"located {len(out)} of {count} building records")
    return out


def densest(buildings: list[tuple[float, float, float]], radius: float = 250.0) -> tuple[int | None, int]:
    if not buildings:
        return None, 0
    r2 = radius * radius
    # Spatial buckets preserve the exact circle test/tie rule while avoiding
    # the quadratic all-pairs scan used by the simple reference implementation.
    buckets: dict[tuple[int, int], list[int]] = {}
    for i, (x, _y, z) in enumerate(buildings):
        key = (math.floor(x / radius), math.floor(z / radius))
        buckets.setdefault(key, []).append(i)
    best, best_count = 0, 0
    for i, (x, _, z) in enumerate(buildings):
        gx, gz = math.floor(x / radius), math.floor(z / radius)
        possible = (j for oz in (-1, 0, 1) for ox in (-1, 0, 1)
                    for j in buckets.get((gx + ox, gz + oz), ()))
        n = sum((buildings[j][0] - x) ** 2 + (buildings[j][2] - z) ** 2 <= r2 for j in possible)
        if n > best_count:
            best, best_count = i, n
    return best, best_count


def read_tree_count(path: Path) -> tuple[int | None, str | None]:
    try:
        size = path.stat().st_size
        if size < 12 or size > MAX_TREES_BYTES:
            raise SurveyError("trees.bin size is outside the supported bounds")
        data = path.read_bytes()
        species = struct.unpack_from("<I", data)[0]
        if species > 100_000:
            raise SurveyError("species count exceeds decoder limit")
        cursor, total = 8, 0
        for i in range(species):
            if cursor + 68 > len(data):
                raise SurveyError("truncated species header")
            name = data[cursor:cursor + 64]
            end = name.find(b"\0")
            if end <= 0 or not re.fullmatch(rb"[A-Za-z0-9_]+", name[:end]):
                raise SurveyError("invalid or unterminated species name")
            count = struct.unpack_from("<I", data, cursor + 64)[0]
            total += count
            if total > 4_000_000:
                raise SurveyError("tree placement count exceeds decoder limit")
            cursor += 68
            block = count * TREE_RECORD
            if block > len(data) - cursor:
                raise SurveyError("truncated tree records")
            # Count the declared fixed-stride placement records after checking
            # that their full payload is present; this survey does not consume
            # positions and therefore does not imply full tree decoding.
            cursor += block
        if len(data) - cursor != 4:
            raise SurveyError("unsupported trailing payload")
        return total, None
    except (OSError, SurveyError) as exc:
        return None, str(exc)


def read_road_nodes(path: Path) -> tuple[int | None, str | None]:
    try:
        size = path.stat().st_size
        if size < 12 or size > MAX_ROADS_BYTES:
            raise SurveyError("road.bin size is outside the supported bounds")
        data = path.read_bytes()
        nodes, segments, _groups = struct.unpack_from("<3I", data)
        if nodes > 4_000_000 or segments > 4_000_000:
            raise SurveyError("road header count exceeds decoder limits")
        offset = 12
        for _ in range(nodes):
            if offset + 0x48 > len(data):
                raise SurveyError("truncated road node block")
            x, y, z, nc, sc, tc, ec, xc = struct.unpack_from("<3f5I", data, offset)
            if not all(math.isfinite(v) for v in (x, y, z)) or max(nc, sc, tc, ec, xc) > 1_000_000:
                raise SurveyError("invalid road node fields")
            end = offset + 0x48 + 4 * (3 * xc + nc + sc + tc + ec)
            if end > len(data):
                raise SurveyError("truncated road node arrays")
            for i in range(nc):
                neighbor = struct.unpack_from("<I", data, offset + 0x48 + 12 * xc + 4 * i)[0]
                if neighbor >= nodes:
                    raise SurveyError("road node references missing neighbor")
            offset = end
        return nodes, None
    except (OSError, SurveyError, struct.error) as exc:
        return None, str(exc)


def percentile(sorted_values: list[float], p: float) -> float:
    if not sorted_values:
        return 0.0
    pos = (len(sorted_values) - 1) * p
    lo = int(pos)
    hi = min(lo + 1, len(sorted_values) - 1)
    return sorted_values[lo] * (hi - pos) + sorted_values[hi] * (pos - lo)


def sampled_grid(t: Terrain, target: int) -> tuple[list[list[float]], int, int, float, float]:
    stride = t.sample_stride
    grid = [[t.world_height(value) for value in row] for row in t.values]
    return grid, stride, len(grid[0]), t.spacing_x * stride, t.spacing_z * stride


def local_relief_and_water(grid: list[list[float]], t: Terrain, buildings: list[tuple[float, float, float]],
                           cluster_index: int | None, dx: float, dz: float) -> tuple[float | None, float | None, float | None, float | None]:
    if cluster_index is None:
        return None, None, None, None
    bx, _, bz = buildings[cluster_index]
    values: list[float] = []
    below: set[tuple[int, int]] = set()
    for r, row in enumerate(grid):
        z = t.world_z - t.size_z / 2 + r * dz
        if abs(z - bz) > 3000:
            continue
        for c, h in enumerate(row):
            x = t.world_x - t.size_x / 2 + c * dx
            if (x - bx) ** 2 + (z - bz) ** 2 <= 3000 ** 2:
                values.append(h)
                if h < 0:
                    below.add((r, c))
    relief = max(values) - min(values) if values else None
    if not below:
        return relief, None, None, None
    # Find connected components on sampled raster cells. Area is an estimate;
    # below-zero is a candidate water proxy, not a verified water mask.
    largest = 0
    while below:
        seed = below.pop()
        queue = [seed]
        count = 0
        while queue:
            r, c = queue.pop()
            count += 1
            for q in ((r - 1, c), (r + 1, c), (r, c - 1), (r, c + 1)):
                if q in below:
                    below.remove(q)
                    queue.append(q)
        largest = max(largest, count)
    return relief, largest * dx * dz / 1_000_000, len(values), None


def terrain_metrics(t: Terrain, buildings: list[tuple[float, float, float]], cluster_index: int | None,
                    target: int) -> dict[str, float | int | None]:
    grid, stride, _cols, dx, dz = sampled_grid(t, target)
    heights = sorted(h for row in grid for h in row)
    n = len(heights)
    min_h, max_h = heights[0], heights[-1]
    p05, p95 = percentile(heights, .05), percentile(heights, .95)
    slope_sum = 0.0
    slope_n = 0
    for r in range(len(grid) - 1):
        for c in range(len(grid[r]) - 1):
            sx = abs(grid[r][c + 1] - grid[r][c]) / max(dx, 1e-12)
            sz = abs(grid[r + 1][c] - grid[r][c]) / max(dz, 1e-12)
            slope_sum += math.degrees(math.atan(math.hypot(sx, sz)))
            slope_n += 1
    local_relief, largest_below_km2, local_n, _ = local_relief_and_water(
        grid, t, buildings, cluster_index, dx, dz)
    below_count = sum(h < 0 for h in heights)
    nearest_below = None
    if cluster_index is not None and below_count:
        bx, _, bz = buildings[cluster_index]
        best2 = float("inf")
        for r, row in enumerate(grid):
            z = t.world_z - t.size_z / 2 + r * dz
            for c, h in enumerate(row):
                if h < 0:
                    x = t.world_x - t.size_x / 2 + c * dx
                    best2 = min(best2, (x - bx) ** 2 + (z - bz) ** 2)
        nearest_below = math.sqrt(best2) if math.isfinite(best2) else None
    return {
        "width_m": t.size_x, "depth_m": t.size_z,
        "height_min_m": min_h, "height_max_m": max_h, "height_range_m": max_h - min_h,
        "below0_pct": 100 * below_count / n,
        "above100_pct": 100 * sum(h > 100 for h in heights) / n,
        "above200_pct": 100 * sum(h > 200 for h in heights) / n,
        "above400_pct": 100 * sum(h > 400 for h in heights) / n,
        "relief_p95_p05_m": p95 - p05,
        "mean_slope_deg": slope_sum / slope_n if slope_n else 0.0,
        "relief_3km_m": local_relief,
        "largest_below0_component_3km_km2": largest_below_km2,
        "nearest_below0_m": nearest_below,
        "sample_stride": stride,
        "sample_count": n,
    }


def png_greyscale(path: Path, grid: list[list[float]]) -> None:
    h, w = len(grid), len(grid[0])
    flat = [v for row in grid for v in row]
    lo, hi = min(flat), max(flat)
    span = hi - lo or 1.0
    raw = b"".join(b"\0" + bytes(max(0, min(255, round((v - lo) * 255 / span))) for v in row) for row in grid)
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
                    + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def discover(roots: Iterable[Path]) -> tuple[list[Path], list[str]]:
    candidates: set[Path] = set()
    errors: list[str] = []
    for root in roots:
        if not root.is_dir():
            errors.append(f"root unavailable: {root}")
            continue
        try:
            for current, dirs, files in os.walk(root):
                names = set(files)
                if all(name in names for name in REQUIRED):
                    candidates.add(Path(current).resolve())
                # Avoid descending into package contents that are obvious leaf maps.
        except OSError as exc:
            errors.append(f"root scan error {root}: {exc}")
    return sorted(candidates, key=lambda p: str(p).casefold()), errors


def score(row: dict) -> float:
    m = row["metrics"]
    # Transparent heuristic: town density (40%), useful 3 km relief (35%),
    # nearby below-zero terrain candidate (15%), and map-wide relief (10%).
    density = min(1.0, row["cluster_count"] / 80.0)
    relief = min(1.0, (m["relief_3km_m"] or 0.0) / 300.0)
    water = 0.0 if m["nearest_below0_m"] is None else max(0.0, 1 - m["nearest_below0_m"] / 5000.0)
    overall = min(1.0, (m["relief_p95_p05_m"] or 0.0) / 500.0)
    return 100 * (.40 * density + .35 * relief + .15 * water + .10 * overall)


def fmt(value, digits=1) -> str:
    return "—" if value is None else f"{value:.{digits}f}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", action="append", type=Path, required=True, help="package search root; repeatable")
    ap.add_argument("--private-dir", type=Path, required=True, help="private output directory outside the repository")
    ap.add_argument("--public-doc", type=Path, default=Path("docs/MAP_SURVEY.md"))
    ap.add_argument("--target-samples", type=int, default=512, help="max raster edge sampled; higher is slower")
    ap.add_argument("--top", type=int, default=5)
    args = ap.parse_args()
    if args.target_samples < 16 or args.target_samples > 4096:
        ap.error("--target-samples must be between 16 and 4096")
    roots = args.root
    folders, scan_errors = discover(roots)
    rows = []
    for index, folder in enumerate(folders, 1):
        row = {"index": index, "path": folder}
        try:
            terrain = read_terrain(folder, args.target_samples)
            buildings = read_buildings(folder / "buildings_game.bin")
            cluster_index, cluster_count = densest(buildings)
            metrics = terrain_metrics(terrain, buildings, cluster_index, args.target_samples)
            tree_count, tree_error = read_tree_count(folder / "trees.bin")
            road_count, road_error = read_road_nodes(folder / "road.bin")
            row.update(ok=True, terrain=terrain, buildings=buildings, cluster_index=cluster_index,
                       cluster_count=cluster_count, building_count=len(buildings), tree_count=tree_count,
                       tree_error=tree_error, road_count=road_count, road_error=road_error, metrics=metrics)
            row["score"] = score(row)
        except (OSError, SurveyError, struct.error, OverflowError) as exc:
            row.update(ok=False, error=str(exc))
        rows.append(row)
        print(f"[{index}/{len(folders)}] {folder.name}: {'ok' if row['ok'] else row['error']}")

    ranked = sorted((r for r in rows if r["ok"]), key=lambda r: (-r["score"], r["index"]))
    top = ranked[:args.top]
    args.private_dir.mkdir(parents=True, exist_ok=True)
    for rank, row in enumerate(top, 1):
        grid, *_ = sampled_grid(row["terrain"], args.target_samples)
        png_greyscale(args.private_dir / f"candidate-{row['index']:03d}-relief.png", grid)

    private = ["# Private map survey report", "", "All package names, exact local paths, indices, and recommendations below are private.",
               "", "Survey status: **OBSERVED** local file readings; decoder layouts retain the evidence limits in the public methodology.",
               "", f"Candidates discovered: {len(folders)}; terrain/building decodes accepted: {len(ranked)}; scan errors: {len(scan_errors)}.",
               "", "## Ranked candidates", "", "| Rank | Index | Package | Exact local path | Score | Buildings | Trees | Road nodes | Densest building index | 250 m count | 3 km relief (m) | Below-zero component (km²) |",
               "|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for rank, row in enumerate(top, 1):
        m = row["metrics"]
        idx = row["cluster_index"]
        private.append(f"| {rank} | {row['index']} | {row['path'].name} | `{row['path']}` | {row['score']:.1f} | {row['building_count']} | {row['tree_count'] if row['tree_count'] is not None else 'UNKNOWN'} | {row['road_count'] if row['road_count'] is not None else 'UNKNOWN'} | {idx if idx is not None else 'NONE'} | {row['cluster_count']} | {fmt(m['relief_3km_m'])} | {fmt(m['largest_below0_component_3km_km2'], 3)} |")
    private += ["", "Recommended launch arguments use zero-based placement indices, matching the current reader:", ""]
    for rank, row in enumerate(top, 1):
        if row["cluster_index"] is None:
            private.append(f"- Rank {rank}, index {row['index']}: no building placement is available.")
        else:
            private.append(f"- Rank {rank}, index {row['index']} ({row['path'].name}): `--map-building {row['cluster_index']}`")
    private += ["", "## Input provenance (private)", "", "Installed game build context: OBSERVED Steam app manifest build ID `23935965`. Per-file SHA-256 values identify the inputs used for the ranked candidates; they are not compatibility claims.", "", "| Index | Input file | SHA-256 |", "|---:|---|---|"]
    for row in top:
        for name in ("script.ini", "heightmap.dds", "buildings_game.bin", "trees.bin", "road.bin"):
            path = row["path"] / name
            if path.is_file():
                private.append(f"| {row['index']} | `{name}` | `{sha256_file(path)}` |")
            else:
                private.append(f"| {row['index']} | `{name}` | MISSING |")
    private += ["", "## Full ranked metrics", "", "| Rank | Index | World (m) | Height min/max/range (m) | Below 0 / above 100 / 200 / 400 (%) | P95-P05 (m) | Mean slope (deg) | 3 km relief (m) | nearest below-zero (m) | trees | road nodes | sample stride |", "|---:|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for rank, row in enumerate(ranked, 1):
        m = row["metrics"]
        private.append(f"| {rank} | {row['index']} | {m['width_m']:.0f}×{m['depth_m']:.0f} | {m['height_min_m']:.1f}/{m['height_max_m']:.1f}/{m['height_range_m']:.1f} | {m['below0_pct']:.2f}/{m['above100_pct']:.2f}/{m['above200_pct']:.2f}/{m['above400_pct']:.2f} | {m['relief_p95_p05_m']:.1f} | {m['mean_slope_deg']:.2f} | {fmt(m['relief_3km_m'])} | {fmt(m['nearest_below0_m'])} | {row['tree_count'] if row['tree_count'] is not None else 'UNKNOWN'} | {row['road_count'] if row['road_count'] is not None else 'UNKNOWN'} | {m['sample_stride']} |")
    private += ["", "## Decode gaps", ""]
    for row in rows:
        if not row["ok"]:
            private.append(f"- Index {row['index']} ({row['path']}): terrain/building decode failed: {row['error']}")
        else:
            if row["tree_error"]:
                private.append(f"- Index {row['index']} ({row['path']}): trees unavailable: {row['tree_error']}")
            if row["road_error"]:
                private.append(f"- Index {row['index']} ({row['path']}): road nodes unavailable: {row['road_error']}")
    private.extend(f"- {error}" for error in scan_errors)
    (args.private_dir / "report.md").write_text("\n".join(private) + "\n", encoding="utf-8")

    public = ["# Map package scenic survey", "", "## Results table", "",
              "Indices are anonymized, deterministic labels assigned from the case-insensitively sorted paths discovered under the configured roots. This table does not reveal package names or paths. **OBSERVED** means local bytes were decoded by this survey's bounded reader; it is not a game runtime or compatibility claim.", "",
              "| Rank | Package index | Score / 100 | World (km) | Relief P95-P05 (m) | 3 km town relief (m) | 250 m building cluster | Below-zero area within 3 km (km²) | Buildings | Trees | Road nodes |", "|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for rank, row in enumerate(top, 1):
        m = row["metrics"]
        public.append(f"| {rank} | {row['index']} | {row['score']:.1f} | {m['width_m']/1000:.1f} × {m['depth_m']/1000:.1f} | {m['relief_p95_p05_m']:.1f} | {fmt(m['relief_3km_m'])} | {row['cluster_count']} | {fmt(m['largest_below0_component_3km_km2'], 3)} | {row['building_count']} | {row['tree_count'] if row['tree_count'] is not None else 'UNKNOWN'} | {row['road_count'] if row['road_count'] is not None else 'UNKNOWN'} |")
    public += ["", "## Generic methodology", "",
               "The script searches each supplied root recursively for folders containing `script.ini`, `heightmap.dds`, and `buildings_game.bin`. Provide each authorized input location with the repeatable required `--root PATH` option; also provide `--private-dir PATH` outside the repository. Results are local observations. The script only reads inputs.", "",
               "- **World dimensions and height:** Read `$WORLDSIZE`, `$WORLDPOSITION`, `$HEIGHTSCALE`, and `$HEIGHTMAPSIZE`. Decode the bounded legacy single-level DDS FourCC 114 R32F payload. Apply `raw × HEIGHTSCALE + WORLDPOSITION.y`. The centered X/Z transform and identity raster orientation are verified in the project ledger for one map only; other packages inherit that uncertainty.",
               "- **Sampling and terrain metrics:** Raster sampling is regular with a default maximum edge of 512 samples. Only sampled source values are checked for finiteness. Height min/max/range, below-zero and above-threshold fractions, and interpolated P95-P05 are sample estimates. Mean slope averages `atan(hypot(dx-height gradient, dz-height gradient))` over adjacent sampled cells in degrees. Sampling stride appears in the private report.",
               "- **Buildings and dense cluster:** Parse the declared count and candidate placement records using the current bounded OpenRepublic reader's observed record shape (survey file-size cap 512 MiB). Coordinates are matrix translations X/Z. For every building, count placements within a 250 m horizontal radius including itself; return the earliest file index on ties, matching `FindDensestPlacement`. The exact placement schema remains inferred from one build's samples.",
               "- **Trees and roads:** Tree counts validate species headers and the declared 17-byte placement payload lengths; positions are not decoded here, and species semantics/yaw/scale remain unknown. Road node counts are accepted after validating the file header and node blocks. Missing or undecodable files show `UNKNOWN`, never zero. Both layouts have build-scoped evidence limits recorded in the project ledger.",
               "- **Town-near relief and below-zero component:** Within a sampled 3 km radius of the densest building, relief is sampled max-minus-min. The largest 4-connected below-zero sampled component is converted to km² using sampled cell spacing. Below zero is only a candidate water/lowland proxy: water extent, sea level, and map mask semantics are unverified. It must not be described as confirmed coastline or lake.",
               "- **Transparent score:** `100 × (0.40 × min(cluster_count/80,1) + 0.35 × min(local_relief/300m,1) + 0.15 × max(0,1-nearest_below_zero/5000m) + 0.10 × min(map_relief/500m,1))`. This favors a dense town, substantial nearby terrain, and a nearby below-zero candidate. It is a heuristic for choosing a visual showcase, not a game metric.",
               "- **Status and limitations:** Height transform has one-map validation; placement record scanning is inferred; tree decoding is partial; road node parsing is build-scoped. Threshold fractions and relief are sampled, and all below-zero area claims are hypotheses. No game launch, save modification, or graphics/hardware acceptance is part of this survey.",
               "", "The private report and candidate relief images are written to the configured private directory. No exact private paths, package names, or fingerprints are written by this script to this document.", ""]
    args.public_doc.parent.mkdir(parents=True, exist_ok=True)
    args.public_doc.write_text("\n".join(public), encoding="utf-8")
    print(f"Accepted {len(ranked)} of {len(folders)} candidates. Private report: {args.private_dir / 'report.md'}")
    print(f"Public anonymized table: {args.public_doc}")
    return 0 if ranked else 2


if __name__ == "__main__":
    raise SystemExit(main())
