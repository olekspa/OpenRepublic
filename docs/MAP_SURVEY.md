# Map package scenic survey

## Results table

Indices are anonymized, deterministic labels assigned from the case-insensitively sorted paths discovered under the configured roots. This table does not reveal package names or paths. **OBSERVED** means local bytes were decoded by this survey's bounded reader; it is not a game runtime or compatibility claim.

| Rank | Package index | Score / 100 | World (km) | Relief P95-P05 (m) | 3 km town relief (m) | 250 m building cluster | Below-zero area within 3 km (km²) | Buildings | Trees | Road nodes |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 27 | 95.5 | 20.0 × 20.0 | 333.6 | 521.4 | 150 | 0.964 | 1769 | 1518777 | 1530 |
| 2 | 19 | 92.1 | 20.0 × 20.0 | 165.6 | 401.6 | 148 | 1.477 | 370 | UNKNOWN | UNKNOWN |
| 3 | 23 | 91.4 | 20.0 × 20.0 | 188.3 | 388.6 | 239 | 1.081 | 3284 | 929829 | 2589 |
| 4 | 25 | 84.7 | 20.0 × 20.0 | 385.6 | 441.7 | 122 | — | 1771 | 357298 | 1357 |
| 5 | 21 | 84.1 | 20.0 × 20.0 | 343.8 | 312.7 | 59 | 4.507 | 668 | 1753830 | 1088 |

## Generic methodology

The script searches each supplied root recursively for folders containing `script.ini`, `heightmap.dds`, and `buildings_game.bin`. Provide each authorized input location with the repeatable required `--root PATH` option; also provide `--private-dir PATH` outside the repository. Results are local observations. The script only reads inputs.

- **World dimensions and height:** Read `$WORLDSIZE`, `$WORLDPOSITION`, `$HEIGHTSCALE`, and `$HEIGHTMAPSIZE`. Decode the bounded legacy single-level DDS FourCC 114 R32F payload. Apply `raw × HEIGHTSCALE + WORLDPOSITION.y`. The centered X/Z transform and identity raster orientation are verified in the project ledger for one map only; other packages inherit that uncertainty.
- **Sampling and terrain metrics:** Raster sampling is regular with a default maximum edge of 512 samples. Only sampled source values are checked for finiteness. Height min/max/range, below-zero and above-threshold fractions, and interpolated P95-P05 are sample estimates. Mean slope averages `atan(hypot(dx-height gradient, dz-height gradient))` over adjacent sampled cells in degrees. Sampling stride appears in the private report.
- **Buildings and dense cluster:** Parse the declared count and candidate placement records using the current bounded OpenRepublic reader's observed record shape (survey file-size cap 512 MiB). Coordinates are matrix translations X/Z. For every building, count placements within a 250 m horizontal radius including itself; return the earliest file index on ties, matching `FindDensestPlacement`. The exact placement schema remains inferred from one build's samples.
- **Trees and roads:** Tree counts validate species headers and the declared 17-byte placement payload lengths; positions are not decoded here, and species semantics/yaw/scale remain unknown. Road node counts are accepted after validating the file header and node blocks. Missing or undecodable files show `UNKNOWN`, never zero. Both layouts have build-scoped evidence limits recorded in the project ledger.
- **Town-near relief and below-zero component:** Within a sampled 3 km radius of the densest building, relief is sampled max-minus-min. The largest 4-connected below-zero sampled component is converted to km² using sampled cell spacing. Below zero is only a candidate water/lowland proxy: water extent, sea level, and map mask semantics are unverified. It must not be described as confirmed coastline or lake.
- **Transparent score:** `100 × (0.40 × min(cluster_count/80,1) + 0.35 × min(local_relief/300m,1) + 0.15 × max(0,1-nearest_below_zero/5000m) + 0.10 × min(map_relief/500m,1))`. This favors a dense town, substantial nearby terrain, and a nearby below-zero candidate. It is a heuristic for choosing a visual showcase, not a game metric.
- **Status and limitations:** Height transform has one-map validation; placement record scanning is inferred; tree decoding is partial; road node parsing is build-scoped. Threshold fractions and relief are sampled, and all below-zero area claims are hypotheses. No game launch, save modification, or graphics/hardware acceptance is part of this survey.

The private report and candidate relief images are written to the configured private directory. No exact private paths, package names, or fingerprints are written by this script to this document.

## Map header prefix follow-up (23 October 2026)

- **OBSERVED (read-only local install inventory, build context 23935965):** 31 `header.bin` files were present in the inspected content tree. Twenty-nine had sibling `script.ini` and `heightmap.dds`; two did not. The header sizes varied from 1,376 to 37,856 bytes. This inventory is not a claim that all files are map packages or share one schema.
- **OBSERVED:** each inspected file begins with a little-endian 32-bit value followed by a non-empty, NUL-terminated UTF-16LE string candidate. The leading values varied (121, 122, and 124) and the string lengths varied. These boundaries are byte-pattern observations only; no producer/consumer trace identifies the value or text as a version, title, map name, or other semantic field.
- **VERIFIED (OpenRepublic source/tests only):** `ParseMapHeaderPrefix` reads that bounded prefix, rejects truncated/invalid UTF-16 and oversized inputs transactionally, and leaves remaining bytes opaque. The 64 KiB input and 256-code-unit string caps are OpenRepublic safety limits, not observed format maxima. Synthetic tests do not validate original-game compatibility.
- **UNKNOWN:** world dimensions, water level/flags/extents, climate or season, start date, and any other fields in the opaque tail. Cross-file variation and terrain/script correlations did not establish field offsets or types for these values. Do not use this prefix reader to infer or populate them.

The header reader deliberately exposes neutral candidate names for the prefix and does not parse the variable-size tail. No original files or payloads were copied into the repository.

## Map header consumer trace (23 October 2026)

- **VERIFIED (static control-flow review; build 23935965 context):** the map-selection/browser routine tries `headerE.bin`, then falls back to `header.bin`. It reads 0x20c bytes and copies UTF-16 code units beginning at byte offset +4 up to a NUL terminator for a displayed text value. This path does not interpret the leading four bytes.
- **UNKNOWN:** the browser path does not establish a complete package-header schema. It exposes no fields after that copied text; water level/flags, world size, and climate remain opaque. A separate runtime consumer or writer trace is required before adding tail fields.

## Header writer layout, static review (24 October 2026)

**VERIFIED (static control-flow review of the original save/map writer, build context 23935965; analysed by me with the private Ghidra project, no decompiled code recorded here):** the routine that writes `header.bin` (or `headerE.bin` when its mode flag is set) first fills a 0x20c-byte structure and writes it with one `fwrite`. The structure begins with the u32 version, which the writer sets to 0x7c (124). A UTF-16LE text field starts at +0x004 (it is copied up to its terminator, capacity 0x180 bytes before the numeric block). The numeric block occupies +0x184..+0x207 as 33 little-endian 32-bit slots, followed by byte flags at +0x208, +0x209 and +0x20a (+0x20b padding).

Slot types known from the writer: +0x184 and +0x188 are floats narrowed from doubles in the game object; +0x1a4..+0x1af and +0x1b0..+0x1bb are each copied from three adjacent 4-byte globals (INFERRED three-component vectors); +0x1d8 is the constant 1; +0x1dc, +0x1e0, +0x1e4 and +0x204 are counts (the last is incremented in a loop over a list); +0x1f0, +0x1f4 and +0x1f8 are bool-as-u32 values from byte fields; the remaining slots are plain 32-bit copies of game-object fields whose names and meanings are **UNKNOWN**; +0x208 is a boolean derived from a terrain-object field, +0x209 a comparison of a byte to 1, +0x20a is zero.

After the 0x20c structure the writer emits a 0x54-byte block of mostly 32-bit values and flags (written from game-object fields at 0x5a4, 0xaf8..0xb04, one derived integer and booleans), then repeated 0x80-byte records and 8-byte records (counts come from the structure above). Per-record contents are **UNKNOWN**. The same routine writes `road.bin` and the other map files in sequence.

**UNKNOWN:** names and meanings of the numeric slots (including world size, water level, climate, start date), the semantics of the 0x54-byte block and the 0x80/8-byte records, and layouts for header versions other than 124 (121 and 122 occur in the inspected packages). OpenRepublic exposes the verified fixed block as raw slots through `ParseMapHeaderFixedBlock` (version 124 only); no slot is named or consumed.

**Import loader consumers (24 October 2026, VERIFIED by static review, build context 23935965):** the loader that reads the 0x20c structure passes the three floats at +0x1a4 to the camera's set-position call and the three at +0x1b0 to its set-look-at call; +0x1d8 equal to 1 gates the extended block; +0x1e0 and +0x1dc are the counts of two lists of 0x80-byte text records read after the 0x54-byte block (the +0x1e0 list first). All remaining slots are copied into global settings whose consumers are not yet traced (UNKNOWN). See [MAP_PACKAGE_FILES.md](MAP_PACKAGE_FILES.md) for the wider package inventory.
