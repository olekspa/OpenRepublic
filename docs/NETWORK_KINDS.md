# Network kinds and compiled constants (24 October 2026)

**VERIFIED (static review of the original's linear-network initialiser, build context 23935965; no decompiled code recorded):** one routine configures every linear-network class (roads, rails, pipes, wires, paths) from a small integer *network kind* and loads that kind's materials from the `paths` and `editor/models` folders. Per kind it sets a block of float constants in the network object at fixed member offsets and a few flag bytes. The constants are compiled into the executable, which is why no road-type definition file exists in the media. The member offsets are named below only by offset; **what each constant means is UNKNOWN unless stated INFERRED**. Constants are exact IEEE floats decoded from the instruction immediates.

## Kinds seen and their materials (VERIFIED where a kind-specific branch loads the material)

| Kind | Observed role (INFERRED from loaded materials) | Distinct materials loaded |
|---|---|---|
| 1 and 2 | road networks (a flag is set for these two) | `road_poormud`, rail-style set shared at init, then sidewalks, lines, one-way line sets, tunnel and tram sets |
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

- **INFERRED, not established:** the two values at +0x40 and +0x48 behave like a lower and upper limit of the same quantity (road kinds 35 and 50, footpaths 4.5 and 6, wires 10 and 10), which fits speed limits in km/h or a similar rate; +0x34 (12, 6 and 2) is a candidate for width or lane-group width in metres; +0x18 and +0x20 are candidates for half-widths or edge offsets. None of this has a consumer trace; do not use these values for rendering or gameplay until one exists.
- **UNKNOWN:** how the kind maps to the package files (`road.bin`, `road_secondary.bin`, `rail.bin`, `pedestrianway.bin`, `heating.bin`, `waterpipes.bin`, `conveyor.bin`, `pipe.bin`, `electro_*`) and how the per-segment kind/sub selector (see [MAP_SURVEY.md](MAP_SURVEY.md)) and per-connection type ids relate to these constants. The most likely route to width, speed and lane data is tracing consumers of the +0x34, +0x40 and +0x48 members.

## Data cross-check: 12 m spacing (24 October 2026)

**OBSERVED (one road.bin, private analysis):** for each primary road segment the distance to the nearest point of any segment that shares no node with it clusters sharply at 12.0 m (269 of about 800 segments with a neighbour within 20 m; neighbouring half-metre bins hold 14 to 38). That agrees with the kind-0 constant +0x34 = 12.0 above, so 12 m is plausibly a snapping or lane-group spacing unit of the original's town layout (**INFERRED**; a road width of 12 m is not implied because the peak is between separate segments). It does not support 6.0 (the kind-1 value) as a spacing.
