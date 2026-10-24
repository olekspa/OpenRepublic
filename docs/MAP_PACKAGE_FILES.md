# Map package file inventory (24 October 2026)

Names and approximate sizes of the files in one inspected local map package (an observed sample, build context 23935965), with OpenRepublic's decoding status. No file contents, hashes or paths are recorded. Sizes come from a single package and vary by map; they indicate where the data volume is, not a format.

Labels: **DECODED** = bounded reader exists in `src/content` with tests; **PARTIAL** = some structure decoded; **NONE** = no OpenRepublic reader; every semantic below is **UNKNOWN** unless a ledger entry says otherwise. A file listed here that the original executable's save/load routines reference by name (static string cross-reference) is marked *referenced*.

| File | Sample size | OpenRepublic status | Notes |
|---|---:|---|---|
| `header.bin` | 33 KB | PARTIAL (prefix + fixed block) | See [MAP_SURVEY.md](MAP_SURVEY.md); the writer layout is VERIFIED, slot meanings UNKNOWN. +0x1a4/+0x1b0 are camera position/look-at (VERIFIED by consumer calls). |
| `heightmap.dds` | 16 MB | DECODED (sampler) | See [MAP_TERRAIN_DATA.md](MAP_TERRAIN_DATA.md). |
| `mask.dds`, `maskdetail.dds`, `farmap.dds`, `grassmap.dds`, `emissivemap.dds` | 16 MB, 136 B, 256 B, 640 B, 640 B | PARTIAL (raw RGBA mask reader for terrain masks) | Terrain blend/mask semantics UNKNOWN. |
| `resourcemap.dds`, `resourcemap2.dds` | 4 MB each | NONE | Names suggest resource-deposit maps (INFERRED from names only). |
| `quadtree.dta`, `collision.dta` | 333 KB, 8 MB | NONE | Written by the quadtree/terrain save routines. |
| `buildings.bin` | 51 KB | NONE | Framing not established; see ledger. |
| `buildings_game.bin` | 48 MB | DECODED (placement records) | Per-building runtime state UNKNOWN. |
| `road.bin` | 18 MB | DECODED (nodes, segments, points; 0x88 header preserved raw) | Loader entry is one routine that reads three u32 and then node and segment records; class/width/lanes UNKNOWN. |
| `road_secondary.bin` | 8.7 MB | NONE | Loaded by the same routine into a second road network (*referenced* next to `road.bin`). Meaning of "secondary" UNKNOWN. |
| `rail.bin`, `rail_secondary.bin` | 120 MB, 13 MB | PARTIAL (`rail.bin` nodes/points) | The secondary file is loaded alongside the primary (*referenced*). |
| `electro_high.bin`, `electro_low.bin` | 21 MB, 9 MB | PARTIAL (nodes/points; power-line meaning INFERRED) | |
| `heating.bin` | 5 MB | NONE | Heating network (INFERRED from name). |
| `waterpipes.bin`, `pipe.bin` | 9 MB, 6.5 KB | NONE | Water/pipe networks (INFERRED from names). |
| `conveyor.bin` | 579 KB | NONE | |
| `pedestrianway.bin` | 4 MB | NONE | Pedestrian paths (INFERRED). |
| `crossroads.bin`, `semaphores.bin`, `lines.bin` | 716 B, 111 KB, 3 KB | NONE | Junction/signal/line data (INFERRED from names). |
| `factoryconnection.bin` | 45 KB | NONE | Likely building connection data (INFERRED). |
| `vehicles.bin` | 1.6 MB | NONE | Vehicle instances. |
| `workers.bin` | 36 MB | PARTIAL (current writer/reader framing traced) | Counted variable-length worker records; bounded raw-record framing only; no named semantic fields. See ledger. |
| `messages.bin`, `events.bin`, `runningScripts.bin`, `research.bin`, `stats.ini`, `namepoints.bin`, `movies.bin`, `toolco.bin`, `pollution.bin`, `trees_game.bin`, `christmas.bin`, `windows.bin` | various | NONE | Game-state, UI and statistics files. `stats.ini` is a text aggregate. |
| `trees.bin` | 16 MB | DECODED (species + XYZ) | |
| `airplane.bin`, `buoy.bin`, `cableway.bin`, `fence.bin` | 4 to 12 B | NONE | Near-empty in this sample (probably counts only). |
| `script.ini` | 2 KB | PARTIAL (text) | Terrain origin and height scale directives. |
| `material.mtl`, `material_fall.mtl`, `material_winter.mtl` | ~0.7 KB | PARTIAL | Terrain material lists per season (INFERRED from names). |

## Reading order for parity

1. Finish the economy-relevant runtime state: `buildings_game.bin` state fields and `workers.bin`, `vehicles.bin`, `stats.ini`.
2. Networks: `road_secondary.bin`, `heating.bin`, `waterpipes.bin`, `conveyor.bin`, `pedestrianway.bin`.
3. Terrain and resources: `mask.dds`, `resourcemap*.dds`, `grassmap.dds`.

Each needs a build-matched writer or consumer trace before any field is named; the save writer routine (which writes these files in sequence) is already located and decompiled in the private Ghidra project.

## One generic network format, eleven files (24 October 2026)

**OBSERVED (one map package, build context 23935965; private Python port of the decoder kept outside Git):** the road layout (three u32 counts for nodes, segments and groups; node blocks; segment records; then group records) parses to **exact end of file** for all of `road.bin`, `road_secondary.bin`, `rail.bin`, `rail_secondary.bin`, `heating.bin`, `waterpipes.bin`, `pedestrianway.bin`, `conveyor.bin`, `pipe.bin`, `electro_low.bin` and `electro_high.bin` (and `cableway.bin` is the empty 12-byte header). Each group record is a u32 (meaning UNKNOWN) followed by a u32 count and that many u32 segment indices (the original reader resolves them against the segment list). So heating pipes, water pipes, pedestrian paths, conveyors, secondary roads and secondary rails share the road decoder's geometry. **VERIFIED (static review, FUN_14056ef50):** the group record shape matches the original reader. Per-network semantics (what "secondary" means; the group value) are UNKNOWN. `ExtractRoadNetwork` now decodes the groups when they fit and end the file exactly (`groups`, `groupsComplete`); older behaviour is unchanged when they do not.

## Small state files: observed structure (24 October 2026)

**OBSERVED (one package, build context 23935965; private Python analysis, nothing copied):**

- `research.bin`: a u32 count (5) then 88-byte records, each a NUL-padded ASCII research-item identifier (64 bytes, for example `engineering_init`, `chemistry_init`) followed by numeric fields. The first field after the name is a float that is 0.0 for four items and 1.0 for one, so it behaves like a completion/progress value (**INFERRED**); the second is 0xffffffff in every record. Item identifiers presumably key into a research tree definition (UNKNOWN where).
- `pollution.bin`: a u32 count of 10,000 then 12-byte cells (a 100 x 100 grid is **INFERRED** from the count, row order UNKNOWN). In every cell the first u32 is 0; the next 4 bytes are a float in 0..1 that is nonzero in only 119 cells (spatially clustered, consistent with a pollution intensity field, INFERRED from the file name); then two byte flags that are 1 in exactly 1,900 cells and two further bytes with varied values (255 distinct values; packed meaning UNKNOWN).
- `events.bin`: a u32 count (304) then variable-length records; the first records hold small integers and floats. Layout UNKNOWN.
- Network-style files with the shared layout are listed in the section above; `vehicles.bin` starts with a count of 512 and `workers.bin` with 19,640; their record sizes are not constant and are UNKNOWN.

## workers.bin follow-up (24 October 2026)

**VERIFIED (static writer/reader trace, build context 23935965):** `workers.bin` starts with a u32 count followed by that many variable-length records. Each current-writer record has a 0x718-byte block plus 16 bytes; a nonzero gate byte at record +0x700 adds 9 bytes, and a nonzero flag at +0x70c adds 128 bytes. The corresponding reader adds a game-internal-version gate (> 0x71) around the 16-byte and 9-byte tails, so older internal versions are not covered by the current-writer expression. **OBSERVED (16 local packages, private scan only):** leading u32 values range 13,619 to 30,354; a fixed 1,832-byte stride is inconsistent across files. **UNKNOWN:** whether those packages match this executable/internal version, exact-EOF validation against a matched package, and worker field semantics. The reader resolves multiple fields as indices into the loaded building table and one into the vehicle table, but home/workplace meanings remain UNKNOWN. See the workers.bin entry in [the reverse-engineering ledger](REVERSE_ENGINEERING_LEDGER.md).

## What "road_secondary" is (24 October 2026)

**OBSERVED (one package; private Python analysis, nothing copied):** `road_secondary.bin` holds 4,133 segments between 8,257 nodes, and nearly every node has exactly one neighbour (8,248 of 8,257), so it is a set of **isolated short pieces** (mean polyline length 21.6 m, 6.5 points) rather than a junction graph; `road.bin` by contrast is a connected graph (1,607 nodes, 1,732 segments, mean 50.7 m, nodes of degree 1 to 5). The segment attribute at +0x40 is 1 for 71% of secondary segments (primary: spread over -1, 0, 1, 2, 3, 10). Only 1% of secondary segment midpoints lie within 3 m of a primary road point (median 24 m), so they are not parallel lanes or sidewalks of the main roads; but **76% lie within 25 m of a building origin** (32% for primary segments), and the nearest buildings are mostly parking lots, offices, storage yards, customs and technical-service buildings. **INFERRED, not established:** the secondary road layer carries building-local roads (yard, parking and service areas) that are stored apart from the main road graph. Its semantic role in the original (pathing, rendering, ownership) is UNKNOWN; the same "secondary" split exists for rails (`rail_secondary.bin`: 194 nodes, 97 segments, also mostly isolated pieces).
