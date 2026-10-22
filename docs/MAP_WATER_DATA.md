# WRSR map water data: level, extents, and ship interaction

**Evidence scope:** read-only inspection of shared content and map-package files, plus existing static analysis artifacts for the local WRSR installation context recorded as build `23935965`. The game was not launched and saves were not modified. `VERIFIED` means the file bytes, header, or reflected shader declaration were directly inspected; `OBSERVED` records names/configuration without claiming full semantics; `INFERRED` is a testable interpretation; `UNKNOWN` marks unresolved behavior. These findings are limited to the inspected build context and samples.

## Findings

### Surface height and sea, river, and lake distinction

- **OBSERVED:** shared `waterriver/script.ini` declares `$HEIGHT 0.000000`, `$AMPLITUDE 2.500000`, and `$SIZE 40000.000000`. It also declares the water color, tile speeds/sizes, foam, and structure mixing parameters. This is shared water configuration, not a per-map value.
- **VERIFIED (shader metadata):** the shared `default_water` shader container exposes constants named `Height`, `Amplitude`, `GridStepX`, and `GridStepY`. This is consistent with a configured base height plus shader displacement, but does not prove the engine's world-space interpretation or how the values are set at runtime.
- **OBSERVED (one inspected map):** its `script.ini` declares `$WORLDPOSITION`, `$HEIGHTSCALE`, `$HEIGHTMAPSIZE`, and `$HEIGHTVISIBLEUNDERWATER`. No explicit sea-level key was found in that script. The terrain origin's Y component is separate metadata and cannot be silently added to or substituted for the shared water `$HEIGHT` without tracing the loader.
- **UNKNOWN:** whether gameplay/rendering water is universally at world Y=0; whether another map/package field overrides the shared height; whether water height follows terrain or water data; and how a water level is transformed relative to `$WORLDPOSITION.y`.
- **OBSERVED:** a separate `default_waterstrip` shader container exists. **INFERRED:** it serves a strip-like water surface, possibly for rivers or shore features. Its name and existence do not establish where it is used or whether rivers, lakes, and sea use distinct surface rules.
- **UNKNOWN:** the data model separating sea, rivers, and lakes, their extents, local elevations, connectivity, or whether one renderer path handles all three. No verified package water-body geometry/extent schema was found.

### Shared water textures and structure inputs

- **VERIFIED (headers only):** shared `waterriver/heightmap.dds` is 1024x1024; `waterstructure.dds` is 512x512; and `waterstructure.bmp` is 1024x1024, 8-bit indexed. These facts establish dimensions/encoding metadata only, not a height range, channel meaning, coordinate transform, or shoreline representation.
- **OBSERVED:** shared water assets also include normal, bump, mirror, foam, foam-tile, and noise textures, and the water script supplies wave/material parameters. The shader metadata names structure and foam-related constants and a `Height`/`Amplitude` pair.
- **UNKNOWN:** whether `heightmap.dds` is a displacement field, a lookup texture, or another shader input; whether the two `waterstructure` files are interchangeable, alternative formats, or used by different paths; and how any of these textures map to world coordinates. No pixels have been assigned geographic or shoreline semantics here.
- **UNKNOWN:** exact water shader texture bindings and the geometry producer/consumer chain. The shader container's reflected identifiers alone do not prove runtime resource bindings or geometry generation.

### `buoy.bin`

- **VERIFIED (sample inventory):** inspected `buoy.bin` files are 12 bytes, all zero in the checked examples. An inventory across 43 files in shipped map/campaign/tutorial/DLC/save locations reported that same size.
- **UNKNOWN:** the file schema, whether 12 zero bytes mean an empty collection or sentinel/header, whether it describes buoy objects, and whether it participates in water rendering, navigation, or ship simulation. Filename and repeated size are insufficient to implement a decoder.

### Ships, buoyancy, and pathing

- **OBSERVED:** shared content contains ship assets; static game analysis artifacts contain water-related gameplay identifiers, including harbor-over-water settings, water connections, and water resource terms. Those identifiers concern game systems but do not reveal ship movement or the meaning of `buoy.bin`.
- **UNKNOWN:** ship buoyancy/vertical placement, water queries, navigation graph representation, shallow-water rules, river/current handling, and the relationship between ships and `buoy.bin`. No traced ship movement or water-height query path and no controlled in-game observation was available for this report.

## Answers and implementation guidance

1. **Best current answer for surface height:** a shared water configuration sets `$HEIGHT` to zero, and the water shader has a corresponding `Height` parameter. This is evidence for a shared renderer base-height setting, not proof of a universal sea-level constant or the authoritative gameplay height. No per-map scalar was identified in the inspected map script. Heightmap-derived water remains unsupported by evidence. Keep the actual WRSR world water level **UNKNOWN** pending build-matched loader/runtime tracing.
2. **Sea versus rivers/lakes:** only a separate strip-water shader name suggests another visual path. The inspected data does not establish water-body categories or their rules. `buoy.bin` is undecoded; shared water height/structure textures are not shown to be map-specific shorelines.
3. **Ships/pathing:** ship content and water-related gameplay identifiers exist, but their movement and water query semantics are **UNKNOWN**. Do not encode buoyancy, draft, or pathing rules from these clues.
4. **Minimal OpenRepublic reproduction:** model water height as an explicit versioned import/config value, initially sourced from an OpenRepublic-authored setting. For an experimental rendering comparison only, use the observed shared `$HEIGHT=0` as a clearly labeled candidate base and preserve it separately from terrain elevation metadata. Do not infer water coverage from `mask.dds`, terrain samples below zero, or `buoy.bin`. Until an extent format is established, use explicit OpenRepublic-authored water polygons/regions for synthetic content and keep WRSR package shoreline **UNKNOWN**. Later support should add a bounded importer only after a loader trace or controlled map observation validates the water extent, coordinate transform, and height convention.

## Evidence boundary and next steps

No water decoder was added: none of the examined water files has an established, validated semantic schema suitable for a fail-closed reader. The next useful evidence is a build-matched trace from map loading through water geometry and shader constant/texture binding, plus a controlled in-game comparison of terrain heights and visible sea/river/lake surfaces. Separately trace ship vertical placement and navigation queries before implementing maritime simulation. Keep private paths, fingerprints, payloads, shader bytes, decompiler output, and local notes outside this repository.
