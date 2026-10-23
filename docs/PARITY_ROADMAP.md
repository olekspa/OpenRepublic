# WRSR parity roadmap (24 October 2026)

This document tracks the distance between OpenRepublic and Workers & Resources: Soviet Republic (WRSR) by area. It is a planning aid, not a compatibility claim: nothing here establishes parity. Evidence labels follow AGENTS.md (VERIFIED, OBSERVED, INFERRED, UNKNOWN); build context for original-game evidence is the single locally inspected build. Chronological detail lives in [PROGRESS_LOG.md](PROGRESS_LOG.md), field-level evidence in [REVERSE_ENGINEERING_LEDGER.md](REVERSE_ENGINEERING_LEDGER.md).

## Status by area

| Area | OpenRepublic today | Original-game evidence | Gap / next step |
|---|---|---|---|
| Building definitions | Parser retains typed rates, storage, electrical geometry and raw economic directives. 359 distinct directives inventoried ([BUILDING_DIRECTIVES.md](BUILDING_DIRECTIVES.md)). | VERIFIED loader semantics only for `$WORKERS_NEEDED`, `$PRODUCTION`, `$CONSUMPTION`, `$CONSUMPTION_PER_SECOND`. | Trace loaders for the next most frequent directives (connections, attraction factors, heating, pollution, resource sources). |
| Production/consumption | Synthetic and imported-catalog economy; deterministic, scalable (see [ECONOMY_PERFORMANCE.md](ECONOMY_PERFORMANCE.md)). | Rates imported from definitions; unit and per-day interpretation INFERRED. | Validate rate units against an in-game observation or loader trace. |
| Construction | Native construction places buildings and roads; no material/work cost. | Cost directives (`$COST_*`, about 474 definitions) are retained raw. Semantics UNKNOWN. | Design a construction-site mechanic (deliver materials, spend work) driven by the retained fields once the loader is traced. Brief saved for a worker. |
| Workers / population | Opt-in synthetic population/workforce ([POPULATION.md](POPULATION.md)). | `$WORKERS_NEEDED` VERIFIED; citizen behaviour UNKNOWN. | Evidence for housing, needs and attraction before replacing synthetic rules. |
| Trade | Opt-in synthetic border trade ([TRADE.md](TRADE.md)). | None. | Not a parity claim; needs original price/exchange evidence. |
| Utilities | Finite networks with generators and consumers ([UTILITY_NETWORKS.md](UTILITY_NETWORKS.md)). | Electrical connection geometry decoded; port semantics UNKNOWN. | Native port semantics; heating and water directives. |
| Roads | Decoded node and segment polylines; terrain-conforming ribbons. | Segment header (0x88 bytes) preserved verbatim; width, lanes, speed, class UNKNOWN. No road definition directory in the media root. | Follow consumers of the road segment object (render/pathing). Brief saved (GHIDRA-ROAD-2). |
| Terrain | Heightmap mesh, vertex colour terrain, real tree placements, grass tile. | Splat/blend semantics UNKNOWN; a bounded raw RGBA terrain-mask reader exists. | Decode layer meaning; compare against original captures. |
| Water | Procedural lake level, shore tones, foam, ocean (presentation). | VERIFIED by static trace: `$COLOR`, `$AMPLITUDE`, `$HEIGHT` bind to shader constants WaterColor, Amplitude, Height. Map-specific level, header tail UNKNOWN. | Locate the map header tail consumer; decide whether to adopt the original constants in the showcase. |
| Map header | Prefix (u32 + UTF-16 string) read; tail opaque. | Fixed 0x20c prefix read by the map browser. World size, climate, start date UNKNOWN. | Trace the header consumer in the main executable. |
| Building runtime state | Not read. Placements decode exactly. | `buildings_game.bin` per-building state UNKNOWN. | Writer/consumer trace needed before any save import. |
| Saves | OpenRepublic-native versioned format only. | Original save format deliberately out of scope. | None planned. |
| Rendering | Wicked Engine showcase (RT, DLSS patch set, volumetrics, wind, grass). | No original-game screenshot comparison done. | Owner-supplied captures or a working input route to the original game. |
| AI/LLM | Controllers, scenario suite, scoring, repeated matched runs. | N/A (OpenRepublic feature). | More scenarios as mechanics land. |
| Modding | Sandboxed Lua scene scripts; economy Lua rule reload. | N/A | Lua economy rules; mod packaging. |

## Ordering rationale

1. **Evidence first.** Mechanics built on guessed semantics are wasted effort. Loader traces (header tail, road header, directive loaders) are the cheapest way to turn UNKNOWN into VERIFIED.
2. **Economy fidelity second.** Construction cost, workers and utilities drive the core loop; each lands as an opt-in mechanic with labelled assumptions so existing hashes and scenarios stay stable.
3. **Rendering parity third.** Visual matching needs original captures; until then presentation changes are labelled presentation, not parity.
4. **Release gate throughout.** Distribution rights for content and NVIDIA runtimes ([DECISIONS.md](DECISIONS.md) D-005) are separate from parity.

## Blockers recorded on 24 October 2026

- The pinned Codex CLI reports "Not logged in", so bridge workers cannot start. Logging in is an owner action. See the 06:50 entry in the progress log.
- Driving the original game's menu with synthetic input did not work, so no original-game comparison exists yet.

## Saved worker briefs (private scratch, not in Git)

GHIDRA-ROAD-2 (road struct consumers) and CONSTR-COSTS (opt-in construction costs from retained `$COST_*` fields) are ready to start once Codex is authenticated.
