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
| `workers.bin` | 36 MB | NONE | Citizen/worker state (INFERRED); the largest unread runtime-state file. |
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
