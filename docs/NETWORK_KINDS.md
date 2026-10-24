# Network kinds and compiled constants (24 October 2026)

**VERIFIED (static review of the original's linear-network initialiser, build context 23935965):** one routine configures linear-network objects from an integer *network kind* and loads kind-specific materials from the `paths` and `editor/models` folders. It sets compiled float constants at fixed member offsets and a few flag bytes; no separate road-type definition file was identified in the inspected media. The exact constants are recorded below. Their semantic meanings remain unresolved unless explicitly supported by a consumer trace.

## Kinds seen and their materials (VERIFIED where a kind-specific branch loads the material)

| Kind | Observed role (INFERRED from loaded materials) | Distinct materials loaded |
|---|---|---|
| 1 and 2 | rail networks (the +0x25c flag is set for these two) | rail gravel, rails, wires, tunnels, and related shared materials |
| 3 | pedestrian paths | `brick_footpath_*` (second set), `road_poormud_only` (a siberia variant when terrain type is 3) |
| 4 | footpaths | `brick_footpath_*`, `footpath_asphalt_inside` |
| 5 | connection overlay | `connection_lines`, `connection_outsidelines` |
| 8 and 9 | power wires | `editor/models/eletric_wires.mtl`, `scaff_underground_pipes` |
| 13 (0xd) | airport surfaces | `airport_mud`, `airport_gravel`, `airport_taxiway`, `airport_inside`, `airport_outside`, runway numbers, `runway_os`, `runway_zebra` |
| others (0, 7, 14, 15, 16) | not identified from materials alone | none specific |

## Constants by member offset (float values; offsets are in the native network object)

Defaults for every kind: +0x44 = 2500, +0x48 = 150, +0x38 = 4.8, +0x3c = 0.5; then +0x50 = 6, +0x54 = 6, +0x58 = 3, +0x5c = 1 (overridden below).

| Kind | Overrides |
|---|---|
| 1 | +0x34 = 6.0; flags +0x30 = 0x01, +0x31 = 0x01, +0x32 = 1; later (road kinds) +0x18 = 1.5, +0x20 = 1.5, +0x24 = 0.38, +0x2c = 0.06, +0x40 = 35, +0x48 = 50, +0x50 = 5, +0x54 = 6, +0x58 = 5, +0x5c = 0.04 |
| 0 | +0x34 = 12.0 |
| 7, 14, 16 | +0x34 = 2.0, flag +0x33 = 1, +0x38 = 0.25, +0x3c = 0.25 |
| 4 | +0x38 = 2.7, +0x3c = 0.5; later +0x18 = 2, +0x20 = 2, +0x24 = 0.38, +0x2c = 0.1, +0x40 = 4.5, +0x48 = 6, +0x50 = 3, +0x54 = 2.5, +0x58 = 4, +0x5c = 0.25 |
| 8 | +0x34 = 2.0, +0x18 = 104, +0x1c = 2.5, +0x28 = 0.38, +0x20 = 10, +0x24 = 20, +0x2c = 0.1, +0x40 = 10, +0x44 = 15, +0x48 = 10, +0x38 = 0.25, +0x3c = 0.25, +0x50 = 1, +0x54 = 1 |
| 9 | +0x34 = 2.0, +0x18 = 25, +0x1c = 2.5, +0x28 = 0.38, +0x20 = 2, +0x24 = 8, +0x2c = 0.1, +0x40 = 7, +0x44 = 15, +0x48 = 7, +0x38 = 0.25, +0x3c = 0.25, +0x50 = 1, +0x54 = 1 |
| non-road base (kinds 0, 3, 5, 7, 13, 14, 15, 16) | +0x18 = 4, +0x20 = 4.6, +0x2c = 0.05, +0x40 = 17, +0x50 = 9, +0x54 = 4, +0x58 = 3, +0x5c = 0.3, +0x48 = 25 |
| 3 | +0x20 = 4 |
| 13 | +0x18 = 8, +0x20 = 9.8, +0x40 = 35, +0x48 = 35 |
| 5 | +0x40 = 15, +0x48 = 18 |
| 15 | +0x18 = 1, +0x24 = -0.05, +0x20 = 2, +0x40 = 0.1, +0x48 = 10, +0x5c = 1 |
| 14 | +0x5c = 0.125, +0x18 = 2.8, +0x40 = 7, +0x20 = 1, +0x48 = 15, +0x50..+0x58 = 1 |
| 7 | +0x5c = 0.25, +0x40 = 11 |

## Interpretation status

- **UNKNOWN (consumer audit, static code review, build context 23935965):** the semantic uses of +0x18, +0x20, +0x34, +0x40 and +0x48 have not been established. All 57 functions listed as referencing the network object were decompiled in private scratch output and screened for these member offsets, but the reviewed uses did not provide a conclusive trace from a member load to a domain meaning. The numeric patterns below are **INFERRED hypotheses only**: +0x34 could be a width or lane-group measure; +0x18/+0x20 could be geometry offsets; +0x40/+0x48 could be bounds on a quantity such as speed. Units and roles are UNKNOWN. Do not use these values for rendering or gameplay as widths or speed limits.
- **UNKNOWN:** how the per-segment kind/sub selector (see [MAP_SURVEY.md](MAP_SURVEY.md)) and per-connection type ids relate to these constants. The package-file-to-network-kind mapping is established separately below.

## Data cross-check: 12 m spacing (24 October 2026)

**OBSERVED (one road.bin, private analysis):** for each primary road segment the distance to the nearest point of any segment that shares no node with it clusters sharply at 12.0 m (269 of about 800 segments with a neighbour within 20 m; neighbouring half-metre bins hold 14 to 38). This is a spatial observation only. It does not establish the meaning of +0x34: the measured nearest points are between separate, unconnected segments, and no code or controlled in-game observation links the 12 m spacing to that member. A road width of 12 m is not established. The kind-1 value is 6.0.

## File to network kind mapping (added; VERIFIED)

**VERIFIED (static review of the package loader and the new-game setup routine, build context 23935965):** each package file is loaded into one network object, and each object is configured with a fixed network kind. The mapping is:

| Package file | Network kind |
|---|---:|
| `road.bin` | 0 |
| `rail.bin` | 1 |
| `rail_secondary.bin` | 2 |
| `road_secondary.bin` | 3 |
| `pedestrianway.bin` | 4 |
| `factoryconnection.bin` | 5 |
| `conveyor.bin` | 6 |
| `pipe.bin` | 7 |
| `electro_high.bin` | 8 |
| `electro_low.bin` | 9 |
| `buoy.bin` | 11 (0xb) |
| `cableway.bin` | 12 (0xc) |
| airport network (file not seen in the loader excerpt) | 13 (0xd) |
| `heating.bin` | 14 (0xe) |
| `fence.bin` | 15 (0xf) |
| `waterpipes.bin` | 16 (0x10) |

With this mapping the constant table above reads per file. **UNKNOWN (consumer audit):** no reviewed consumer establishes +0x34 as cross-section width or +0x40/+0x48 as speed limits, and no units are established. Those interpretations remain **INFERRED hypotheses** from the value patterns only. The 12 m nearest-neighbour peak between unconnected road segments is not width evidence. These constants are not used by OpenRepublic code; the road mesh keeps its own presentation width. The road-kind flag at +0x25c is set for rail kinds 1 and 2, not main road kind 0.
