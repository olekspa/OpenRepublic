# WRSR parity roadmap (23 October 2026)

Latest consolidated status and handoff: [HANDOFF_2026-10-24.md](HANDOFF_2026-10-24.md).

This document tracks the distance between OpenRepublic and Workers & Resources: Soviet Republic (WRSR) by area. It is a planning aid, not a compatibility claim: nothing here establishes parity. Evidence labels follow AGENTS.md (VERIFIED, OBSERVED, INFERRED, UNKNOWN); build context for original-game evidence is the single locally inspected build. Chronological detail lives in [PROGRESS_LOG.md](PROGRESS_LOG.md), field-level evidence in [REVERSE_ENGINEERING_LEDGER.md](REVERSE_ENGINEERING_LEDGER.md).

Full feature parity is the target, not the status of the existing showcase or
synthetic economy. This is an initial coverage map; a systematic feature and
acceptance audit is still required before it can serve as an exhaustive WRSR
checklist. Source, synthetic replay, original-game comparison, and graphics or
hardware acceptance are separate gates. New gameplay belongs in `src/economy*`
under D-008; the older `src/sim` is a demo.

## Status by area

| Area | OpenRepublic today | Original-game evidence | Gap / next step |
|---|---|---|---|
| Building definitions | Parser retains typed rates, storage, electrical geometry and raw economic directives. 359 distinct directives inventoried ([BUILDING_DIRECTIVES.md](BUILDING_DIRECTIVES.md)). | VERIFIED loader semantics only for `$WORKERS_NEEDED`, `$PRODUCTION`, `$CONSUMPTION`, `$CONSUMPTION_PER_SECOND`. | Trace loaders for the next most frequent directives (connections, attraction factors, heating, pollution, resource sources). |
| Production/consumption | Synthetic and imported-catalog economy; deterministic, scalable (see [ECONOMY_PERFORMANCE.md](ECONOMY_PERFORMANCE.md)). | Rates imported from definitions; unit and per-day interpretation INFERRED. | Validate rate units against an in-game observation or loader trace. |
| Construction | Native placement is instant; a bounded ordered source profile and aggregate probe retain cost/phase/recipe/station data without gameplay conversion. | Loader operations and one runtime worker-threshold/progress pathway VERIFIED statically for build 23935965; work/material units, delivery/removal and complete automatic formulas UNKNOWN. | Trace non-worker delivery/removal and raw work-argument consumers; implement the native lifecycle independently. See [construction evidence](CONSTRUCTION_EVIDENCE.md). |
| Workers / population | Opt-in synthetic population/workforce ([POPULATION.md](POPULATION.md)). | `$WORKERS_NEEDED` VERIFIED; citizen behaviour UNKNOWN. | Evidence for housing, needs and attraction before replacing synthetic rules. |
| Trade | Opt-in synthetic border trade ([TRADE.md](TRADE.md)). | None. | Not a parity claim; needs original price/exchange evidence. |
| Utilities | Finite networks with generators and consumers ([UTILITY_NETWORKS.md](UTILITY_NETWORKS.md)). | Electrical connection geometry decoded; port semantics UNKNOWN. | Native port semantics; heating and water directives. |
| Roads | Decoded node and segment polylines; terrain-conforming ribbons. | Segment header (0x88 bytes) preserved verbatim; width, lanes, speed, class UNKNOWN. No road definition directory in the media root. | Follow consumers of the road segment object (render/pathing). Brief saved (GHIDRA-ROAD-2). |
| Terrain | Heightmap mesh, vertex colour terrain, real tree placements, grass tile. | Splat/blend semantics UNKNOWN; a bounded raw RGBA terrain-mask reader exists. | Decode layer meaning; compare against original captures. |
| Water | Procedural lake level, shore tones, foam, ocean (presentation). | VERIFIED by static trace: `$COLOR`, `$AMPLITUDE`, `$HEIGHT` bind to shader constants WaterColor, Amplitude, Height. Map-specific level, header tail UNKNOWN. | Locate the map header tail consumer; decide whether to adopt the original constants in the showcase. |
| Map header | Prefix (u32 + UTF-16 string) read; tail opaque. | Fixed 0x20c prefix read by the map browser. World size, climate, start date UNKNOWN. | Trace the header consumer in the main executable. |
| Building runtime state | Supported `buildings_game.bin` placement framing decodes exactly; runtime state is not imported. | `buildings.bin` framing and per-building state UNKNOWN. Placement framing does not establish runtime semantics. | Writer/consumer trace and identity/time-matched cross-check required before a state decoder. |
| Saves | OpenRepublic-native versioned format only. | Original save format deliberately out of scope. | None planned. |
| Rendering | Wicked Engine showcase (RT, DLSS patch set, volumetrics, wind, grass). | No original-game screenshot comparison done. | Owner-supplied captures or a working input route to the original game. |
| AI/LLM | Controllers, scenario suite, scoring, repeated matched runs. | N/A (OpenRepublic feature). | More scenarios as mechanics land. |
| Modding | Sandboxed Lua scene scripts; economy Lua rule reload. | N/A | Lua economy rules; mod packaging. |

## Remaining coverage to audit

The entries below explicitly keep major systems in scope. UNKNOWN means parity
has not been established; a content reader or synthetic analogue is only a
foundation for the corresponding gameplay system.

| Area | Current foundation / boundary | Next acceptance gate |
|---|---|---|
| Passenger transit and commuting | Synthetic residents and staffing; no validated commuting or passenger-service behavior. | Establish one passenger path, stop/access identity, travel and staffing timing with build-matched evidence, then a reproducible route fixture. |
| Rail, ships, aircraft and other transport | Bounded network/content readers and road-based synthetic hauling; vehicle/service parity UNKNOWN. | Audit supported transport modes, schedules, depots, cargo rules and routing; implement one explicit mode at a time. |
| Citizen needs, education, health and services | Synthetic housing/food/workforce only; original needs, demographics and service access UNKNOWN. | Evidence-backed need/service contract and owned fixture including unavailable services and recovery. |
| Water, sewage, heating, waste and pollution | Finite synthetic utility networks; some content declarations retained. Original capacities, topology and consequences UNKNOWN. | Trace declarations and consumers before mapping them into separate native models and failure cases. |
| Agriculture, seasons and weather | Presentation and synthetic production foundations; authentic crop, season and climate mechanics UNKNOWN. | Define a build-matched time/season contract and one observed seasonal production lifecycle. |
| Finance and foreign trade | Synthetic priced imports/exports and treasury; authentic currencies, loans, prices and border logistics UNKNOWN. | Establish prices/units and exchange timing before claiming financial or trade parity. |
| Construction, demolition and planning | Native queued placement/removal and road tools; material delivery, work, refunds and demolition behavior UNKNOWN. | Construction milestone below; audit planning tools independently of core simulation. |
| Emergencies and disruptions | Existing disabled-road/building controls are synthetic; original emergency, disaster and response systems unassessed. | Coverage inventory and one evidence-backed incident/response fixture. |
| Human interface and accessibility | Native construction/economy tools and scene controls; broad original workflow coverage unassessed. | End-to-end human acceptance for each mechanic, including inspection, explanations, recovery and persistence. |
| Mod packaging and compatibility | Lua scene sandbox and native economy-rule reload; no original mod compatibility claim. | Versioned owned mod contract, metadata preservation, validation and dependency/license boundaries. |
| Persistence and world scale | Bounded native saves/checkpoints and synthetic replay; original saves out of current scope. | Repeated combined-system replay, explicit world/history/size limits and measured representative workloads. |

## Next development milestones

1. **Workforce reliability and scale.** Fix disabled workplaces reserving
   residents, replace repeated world scans with a stable assignment pass, and
   reject invalid population saves or unsafe catalog reloads. Preserve authored
   population/trade metadata through Lua reload. Acceptance: enabled/disabled
   recovery, partial staffing, serial/parallel agreement, checkpoint continuation,
   transactional rejection tests and a reproducible scale measurement. These
   remain OpenRepublic rules; WRSR citizen parity is UNKNOWN.
2. **Construction evidence and one complete owned lifecycle.** Trace
   `$COST_*` loader fields and consumers before importing costs. Specify an
   opt-in native site/material/work contract separately from original-game
   formulas. Acceptance: a small authored fixture spanning placement, material
   shortage/delivery, work gating and completion, with shared API commands,
   explanations and checkpoint replay. Cancellation/demolition enter only with
   explicit semantics. Static loader mapping alone does not establish gameplay
   units, phase timing or cost parity.
3. **Mobility and needs evidence gate.** Choose one passenger/commuting or
   service-access loop after the coverage audit. Acceptance before claiming
   parity: build-matched writer/consumer evidence, controlled original outcome,
   an owned simulation fixture, and the same authoritative API/UI path. Road
   rendering and global worker allocation alone do not meet this gate.

## Ordering rationale

1. **Evidence first.** Mechanics built on guessed semantics are wasted effort. Loader traces (header tail, road header, directive loaders) are the cheapest way to turn UNKNOWN into VERIFIED.
2. **Economy fidelity second.** Construction cost, workers and utilities drive the core loop; each lands as an opt-in mechanic with labelled assumptions so existing hashes and scenarios stay stable.
3. **Rendering parity third.** Visual matching needs original captures; until then presentation changes are labelled presentation, not parity.
4. **Release gate throughout.** Distribution rights for content and NVIDIA runtimes ([DECISIONS.md](DECISIONS.md) D-005) are separate from parity.

## Evidence and execution boundaries

Historical bridge authentication and original-game input failures are recorded
in the progress log; they are not assumed to describe the current tool session.
Construction and road semantics still require evidence regardless of tool
availability. Private analysis, inputs and raw traces remain outside Git.

## Update, 24 October 2026 (evening)

New evidence since the table above was written (all labels as in the ledger; build context 23935965, one map package unless noted):

- **Map header:** the fixed 0x20c structure is VERIFIED from the writer and the loader (versions 121 to 124): camera position and look-at, game day and year, extended-block flag and the two record-list counts are named; `ParseMapHeaderFixedBlock` exposes them. New-game start rules (start-year options and terrain-dependent start day) are VERIFIED ([GAME_SETUP.md](GAME_SETUP.md)). Remaining slots are UNKNOWN.
- **Networks:** one decoder layout covers eleven package files (roads, rails, heating, water pipes, pedestrian ways, conveyors, pipes, power lines and the road/rail "secondary" files), each ending exactly after its group records ([MAP_PACKAGE_FILES.md](MAP_PACKAGE_FILES.md)). Secondary roads look like building-local roads (INFERRED). Rendering of heating, water and pedestrian geometry is not built.
- **Roads:** the segment loader mapping is VERIFIED (node indices, list counts, the +0x40/+0x44 selector pair); width, lanes, speed and the type-id table are still UNKNOWN.
- **Building runtime state:** the per-building writer layout (0x548 block, 0x190 block, collections, native strides) is VERIFIED; slot meanings are UNKNOWN. Observed behaviour: a 0 to 1 float (+0x3e8) fractional almost only on `temp` records, an enum-like slot (+0x3e4) and a flag (+0x410) varying within a type.
- **Economy data:** all building-definition directives are inventoried; construction costs are retained raw and the opt-in diagnostic reports them unsupported.
- **Tooling:** Codex workers are blocked by the account's usage limit until about 4:38 PM local; evidence work continues inline with the private Ghidra project (a raised-payload decompile script lets the largest routines decompile).

Suggested next steps, in order: (1) trace consumers of the building-object offsets to name the state slots (construction progress, enabled, workers, stock); (2) find the road-type table (width, speed, lanes) via the segment object's +0x120/+0x124 consumers; (3) render heating, water-pipe and pedestrian geometry from the shared network decoder; (4) name the remaining header settings from the new-game menu; (5) the original-game screenshot comparison once captures exist.
