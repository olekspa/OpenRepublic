# OpenRepublic project plan

**Prepared:** 21 October 2026
**Status:** Research and public repo are established. The independent synthetic C++ simulation runs a fixed-tick production and truck-delivery loop on a player-editable road graph. Built-in and player-placed Farms/Mills own separate bounded inventories; Storage is an inventory-bearing entity. A queued Farm→Mill→Storage plan routes cargo over connected road nodes, survives save/reload, rejects closures that break any assigned leg, and can retarget flour cargo when its destination is full. Save v7 persists inventories, the active plan, and queued construction/plan commands; v1–v6 migration is tested. The current Release client passes 6/6 CTest checks and the separate headless build passes 5/5, including new synthetic MTL syntax tests. The Wicked Engine app has bounded runtime evidence for earlier scene/HUD controls, facility placement and save/reload, startup Lua rules, paused/running tick-boundary reload, and rejection of invalid values without replacing active rules. A 21 October runtime check opened the current client, paused the simulation, entered and canceled storage placement without committing a placement, selected built-in storage OR-1003 and read its inventory, then assigned that selected storage as the truck-plan draft endpoint. It did not queue a route or write a save; placement acceptance, a changed route, and cargo delivery through the new UI remain open. The developer GLB probe now validates a bounded embedded-resource, static-mesh glTF 2.0 profile against the exact bytes before writing a temporary snapshot for the pinned Wicked Editor importer. A syntax-only MTL reader now recognizes the bounded directive shapes observed in three private local samples and preserves unknown directives without resolving texture references; it does not establish MTL semantics, texture-slot meanings, or general WRSR support. The Release source/build checks pass, but visual/material fidelity is still unverified; this remains trusted-local processing, not hostile-file isolation. The WISCENE probe uses Wicked Engine's native archive path. An OpenRepublic-owned reader decodes bounded legacy R32F DDS heightmaps and parses adjacent terrain metadata; the app rendered a copied local 2048x2048 sample as a centered, decimated 257x257 mesh. Its candidate height transform, map placement, and row orientation remain unverified; this is not full terrain/world compatibility. Direct NMF/material fidelity, measured performance, controlled WRSR formula comparison, and full alpha acceptance remain open.

**Project:** independent WRSR-compatible client and simulation engine

The 21 October 2026 Release simulation tests now also run a 10,000-tick synthetic economy acceptance: uninterrupted and mid-run save/reloaded continuations match canonical state at every tick, facility/storage inventory bounds hold at every tick, and cumulative flour delivery reconciles to flour still stored while consumption is absent. This verifies only OpenRepublic's current synthetic rules and same-build replay path, not WRSR economic parity.

## 1. Mission

OpenRepublic will explore whether an independent, open-source client can preserve the economic and logistical character of Workers & Resources: Soviet Republic (WRSR) while using a modern rendering stack, a maintainable simulation architecture, better performance where measurements support it, and a more approachable modding model.

The intended base is Wicked Engine, an independent C++ simulation library, Lua for data and gameplay rules, and optional external C# AI or experiment controllers. The target is a standalone program, not a replacement renderer injected into `SOVIET64.exe`. A user's local, legitimately obtained WRSR installation may supply selected content for read-only compatibility experiments. The engine should also run with synthetic/open example content so core development does not depend on the game.

This plan is a sequence of experiments, not a claim that the proprietary formats or full gameplay can be reproduced. A first viewer and a first playable economy loop are meaningful results even if full parity proves infeasible.

## 2. Define compatibility before promising it

“WRSR-compatible” covers distinct capabilities. Track them independently and only promote a row when its gate passes.

| Capability | Feasibility target | Alpha promise |
|---|---|---|
| Local asset discovery | Identify supported content in a user's installation and preserve source/version provenance. | Selected formats only; original files stay in place and read-only. |
| Asset preview | Display a representative set of buildings, materials, and terrain after a validated import path. | A documented bounded subset; no claim that every asset/variant works. |
| Map/world import | Reconstruct coordinates, terrain, and object placement for a selected region. | One explicitly scoped sample region if the feasibility gates pass. |
| Mod definitions | Understand documented or observed data files and provide an OpenRepublic schema. | OpenRepublic-native mods first; original mod conversion is a later adapter. |
| Simulation behavior | Match selected production/transport outcomes under controlled conditions. | One measured vertical slice; no claim of complete economic parity. |
| Saves | Read or write the game's binary save format. | Out of scope. OpenRepublic uses its own versioned save format. |
| Steam integration | Launch, overlay, Workshop, achievements, or cloud saves. | Out of scope. WRSR launching through Steam is only a local research/validation method. |
| Runtime requirement | Run with either local WRSR content or non-proprietary examples. | Game installation needed only for WRSR-content mode, not synthetic scenarios. |

Do not describe the first alpha as loading original saves or mods. UI labels, file extensions, or a rendered model are not evidence of simulation compatibility.

## 3. Architecture direction

```text
Local WRSR install (read-only)         Synthetic / open examples
          |                                       |
          +-------- versioned content readers ---+
                              |
                    normalized asset/definitions
                       /                 \
              Wicked Engine          C++ simulation
              render / camera        headless authority
              selection / UI         fixed tick / world state
                       \                 /
                        runtime coordinator
                               |
               +---------------+----------------+
               |                                |
       Lua data/rules                  optional external AI
       reload at tick boundary         C# over versioned IPC
```

### Boundaries

- Keep authoritative world state and simulation independent of Wicked Engine's renderer, scene objects, and frame rate.
- Start with a single-threaded, fixed-step simulation and canonical iteration/command order. Measure before introducing parallel work.
- Drive rendering from immutable or read-only simulation snapshots. A slow frame must not alter how much simulation a tick means.
- Use stable OpenRepublic IDs, not pointers or addresses from the game's process. Version public file and message schemas.
- Apply player commands at tick boundaries. Record commands and seed/configuration for reproducible replay within a stated same-build/same-platform contract; cross-platform bitwise determinism is a separate target.
- Keep external C# AI out of the authoritative process initially. Send versioned observations and commands through a local IPC protocol; reject stale or invalid command batches.
- Treat Lua as a data/rule extension point with schema checks, a tick-boundary reload, and rollback if a rules bundle fails validation. It is not a substitute for profiling native simulation code.
- Keep the first import path read-only. Never edit the Steam install or write converted files back over original content.

### Initial module sketch

```text
src/
  app/          window, input, runtime coordination
  sim/          headless world, tick, commands, inventory, routing, production
  content/      manifests, versioned readers, normalized definitions
  render/       Wicked Engine adapter and view-only scene cache
  scripting/    Lua integration and validated rule reload
  controller/   optional local IPC for AI/experiment clients
tools/          inspectors, import diagnostics, benchmark runner
examples/       synthetic, redistributable data only
tests/          deterministic sim and reader fixtures without game payloads
docs/           public plans, decisions, evidence ledger, formats
```

This is a target layout, not a commitment to create empty projects before the first vertical slice. Keep the first code change small and buildable.

## 4. What existing work tells us

### WRSR-specific modding and analysis

- **TesmioLoader** demonstrates that WRSR has been extended through a native plugin/loader route, with file redirection, plugins, and executable-version-specific research. It is useful for understanding observed game behavior, installation differences, and test probes. It operates alongside/in the original game process and is not the architecture for OpenRepublic.
- **TesmioLoader community plugins** demonstrate changes to resources, deposits, localization, and vehicle rules in the original game. Their code has independent licenses and provenance; it is not automatically available for reuse in this MIT project.
- **RepublicMP** provides useful case studies in command/event transport, tick handling, state synchronization, and diagnosing divergence. It is a mod of WRSR. Its protocol and reported behavior do not prove that WRSR is bitwise deterministic or define an OpenRepublic protocol.
- Existing local WRSR reverse-engineering work has bounded building/save candidates, road and utility observations, offline planners, and native-hook studies. Some records are verified at the byte or call-site level while their semantics remain inferred or unknown. The prior project's planners and contracts are not recovered WRSR APIs or schemas. Import no code or data automatically; record a source/version/status and re-evaluate each proposed reuse in the private research ledger.

### Comparable reimplementation

**MeitouClient** is a current public example of a separate engine reimplementation for Kenshi, using C#/.NET and a Vulkan-first renderer while requiring a local game copy. It is useful as a project-structure and scope reference: independent client, local content, documented fidelity gaps, and incremental playable slices. Its code and GPL license are not inputs to this MIT repository, and its existence does not establish feasibility or rights for WRSR.

### Engine choice

Wicked Engine is a plausible first renderer because its upstream project is MIT-licensed, presents itself as a C++ framework, exposes a Lua API, and supports DirectX 12 on Windows with Vulkan available. Start by pinning and building an exact upstream revision. Keep the simulation separable so that an engine bottleneck can be measured and an alternative renderer can be evaluated without rewriting economic rules.

## 5. First 14-day feasibility sprint

The 14 days are an intensive planning envelope, not a deadline guarantee. Record person-hours and pass/fail/unknown for each gate so a blocked format task does not hide a successful renderer build.

| Gate | Work | Evidence to exit |
|---|---|---|
| F0: workstation | Pin Wicked Engine; build its documented Windows target using the installed VS/MSVC/Windows SDK; open a blank scene. Record commits and tool versions. | Repeatable build commands, exact revision, clean build log, blank window. No game files required. |
| F1: content survey | Catalog candidate extensions and text definitions; identify representative categories (one simple building, one material/texture path, one terrain sample, one placed object). Track game build and DLC/mod status locally. | A format/coverage table with source provenance and no payload copied to Git. Each guessed field remains explicitly guessed. |
| F2: model/material path | Prefer an official exporter or documented interchange route first. Otherwise create a bounded, read-only parser/importer for one representative asset with structured errors. | One recognizable asset with measured scale/material path, repeatable diagnostic output, and a clear statement of unsupported variants. |
| F3: terrain/coordinates | Decode the bounded legacy R32F height layer and metadata; then compare a small map/region against two known landmarks and elevation references. Treat save-derived object coordinates and height transforms as candidates until independently checked. | Terrain DDS/metadata parsing, a decimated preview, and a generic caller-supplied X/Z bilinear sampler with synthetic tests are implemented. The remaining exit requires a repeatable region view with verified coordinate transform and stated tolerance, or a concise evidence-backed blocker report. |
| F4: placement/viewer | Build an OpenRepublic scene manifest, camera, object selection, and metadata inspector. Keep originals untouched. | Replayable viewer scene with source IDs/provenance and two landmark alignment checks. |
| F5: go/conditional/pivot | Review build reliability, representative-format coverage, effort, rights questions, and remaining unknowns. | A decision memo: proceed to world slice, spend a bounded follow-up on one blocker, or pivot to synthetic/open content. |

### Current F0 status (21 October 2026)

- Pinned upstream Wicked Engine revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` as a Git submodule.
- Built `Editor_Windows` and the upstream `Template_Windows` sample in Release/x64 with VS 2022/MSVC 14.44 by passing `PlatformToolset=v143`. Upstream project files request v145, which is not installed here; no upstream files were modified.
- The compile emitted warnings from bundled `lodepng` and `zstd` sources. The build succeeded, and those vendor warnings were not suppressed or patched.
- The upstream `Template_Windows` sample opens on DirectX 12 and reaches its blank scene; its overlay reported Wicked Engine 0.72.126. The displayed frame rate was not a controlled benchmark.
- The first OpenRepublic-owned Windows prototype used a plane, cube, and directional light. At runtime the expected window opened, the engine logged `Wicked Engine Initialized`, and the process closed cleanly with exit code 0. A window-specific capture confirmed that earlier scene only.
- The current app source implements WASD/arrow camera movement, Space/Ctrl vertical movement, wheel zoom, and ray picking for synthetic buildings (`OR-1001`–`OR-1003`) with selection feedback. It also has pause, one-tick step, road-open/close, and live simulation-status controls; the Release/x64 target compiles with those controls.
- The independent `src/sim` module advances a synthetic farm→mill→storage chain on the bounded road graph. Built-in and placed facilities own inventories, production/processing runs per entity, and the truck follows a typed Farm→Mill→Storage plan. Save v7 persists the graph, inventories, active plan, queued placements/plans, and unacknowledged results; v1–v6 migrations are tested. Full-storage flour can be retargeted without losing cargo; road changes that disconnect a planned leg are rejected transactionally. These are OpenRepublic-owned synthetic rules, not recovered WRSR formulas.
- `src/scripting` uses Wicked Engine's bundled Lua 5.4.8 library through an independent short-lived Lua state for rule files. It loads text only, exposes an empty `_ENV` without standard libraries, limits source size to 64 KiB, Lua allocations to 8 MiB, and execution to about 100,000 VM instructions; it requires exactly four schema fields, validates integer ranges, and commits the candidate only after full validation. The demo rule file loads at app startup; the GUI reload control applies immediately while paused or at the next fixed tick while running. Rejected changes preserve current simulation rules. Tests cover schema, malformed input, sandbox, allocation, size, and instruction limits.
- The developer-only `--glb-probe <file.glb>` validates exact in-memory bytes against an OpenRepublic-owned, bounded glTF 2.0 profile before importing a temporary snapshot through the pinned Wicked Editor `ModelImporter_GLTF.cpp`. The profile accepts embedded BIN and buffer-view images, finite static triangle meshes with supported vertex layouts, bounded accessors, unit-length node quaternions within a 1e-3 norm-squared tolerance, and validated material/texture/sampler references; it caps aggregate float-component and index-entry validation at 16,777,216 scalar reads and rejects URI resources, extensions, sparse accessors, morph data, animation, skins, cameras, unsupported attribute layouts, and inconsistent node graphs. Synthetic fixtures cover valid and invalid cases, references, binary ranges, geometry/index bounds, non-finite values, quaternion validation, aggregate scan limits, and transactional result handling. The client checks imported indexed triangle data before merging the staging scene. `tools/create_synthetic_glb.py` remains an OpenRepublic-only fixture; earlier runtime logs reported 1 mesh, 1 material, and 2 transforms for it. A private scratch model-derived GLB earlier reported 3 meshes, 2 materials, and 4 transforms but contained no embedded images, so it does not establish WRSR material fidelity. A separate `--scene-probe <file.wiscene>` call added one transform from a Wicked Engine sample archive. The GLB importer is still Editor code, is not sandboxed, and pixel decoding, visual fidelity, original NMF parsing, and standalone content rights remain open.
- Bounded runtime checks verified readable HUD, pause/step, road edge 202 close/reopen, mill picking (`OR-1002` highlight/title), mouse-wheel zoom, synthetic placement and GUI save/load. Lua startup showed the bundled rates. While paused, a temporary edit to farm `5 every 3 ticks` and mill `2/tick` applied immediately; out-of-range farm value `13` was rejected while the `5/3/2` rates remained active. While running, a second valid edit (`4/1/2`) applied at the next fixed tick boundary. The build-local rule copies were restored from source defaults afterward. Keyboard movement remains unverified. Prototype placement has no costs, build stages, material logistics, or WRSR placement rules; screenshots were not committed.
- Direct NMF parsing, external texture/material fidelity, terrain/world placement, graphics API selection for the OpenRepublic target, and performance remain open. The one private model-derived GLB was read from an ignored scratch copy; no original game file or converted model entered Git.

Reproduce the OpenRepublic target from a VS 2022 Developer shell (quote the SDK argument in PowerShell):

```powershell
cmake -S . -B build/openrepublic-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143 "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic OpenRepublicSimulationTests OpenRepublicTerrainReaderTests OpenRepublicLuaRulesTests --parallel
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
```

The executable is `build/openrepublic-vs2022/src/app/Release/OpenRepublic.exe`; run it with `build/openrepublic-vs2022/` as the working directory so the generated `Content` directory is found. The headless simulation can also be configured independently with `-DOPENREPUBLIC_BUILD_CLIENT=OFF`; tests remain enabled by default.

If authentic terrain or placements are blocked, an accurate isolated viewer or a synthetic but fully inspectable world is a conditional result. Do not spend the sprint adding graphics effects while import, coordinates, or source provenance remain unresolved.

### Go / conditional / pivot criteria

- **Go:** blank engine build repeats; the OpenRepublic-owned app reaches engine initialization; at least one bounded asset path and one world/placement path have evidence; the most important unknowns have named next checks; no proprietary material entered Git.
- **Conditional:** mesh/material import works but terrain, placement, or legal interpretation is unresolved. Give the blocker a fixed 2–4 week/person-hour budget and avoid expanding the renderer during that investigation.
- **Pivot:** selected game data cannot be used in a standalone viewer under acceptable terms, or the format cost is disproportionate. Continue the independent simulation on synthetic/open content and pursue written clarification or a user-provided supported interchange workflow.

The threshold for an importer is coverage by format features and variants, not “ten files” alone. Ten nearly identical models do not establish general support.

## 6. Staged roadmap after the feasibility sprint

### Stage A — Interactive world (planning range: weeks 3–6 intensive)

Stream a bounded region; load supported terrain/buildings; maintain a coordinate transform; add selection, metadata, placement/removal, and simple road visuals. Define OpenRepublic's own scene/save schema. Use synthetic content for CI and an optional local WRSR sample for manual evaluation.

**Exit:** same input loads to the same transforms; selected source IDs are traceable; selection/picking and basic placement work; a scripted camera path has stable memory and frame-time captures.

### Stage B — Roads, vehicles, cargo (overlapping weeks 4–10)

Implement a directed road graph with validated intersections and connectivity. Add shortest-path routing, route invalidation on graph edits, vehicles with capacity/speed, and storage/loading/unloading. Begin with one route and one resource before scaling.

**Exit:** a vehicle traverses a player-built route, picks up and delivers one resource, inventory totals reconcile, broken edges invalidate/recompute its route, and the result survives OpenRepublic save/reload.

**Current checkpoint (21 October 2026):** Save v7 now persists entity-owned Farm/Mill/Storage inventories, an active Farm→Mill→Storage truck plan, and queued tick-boundary storage/plan/construction commands. Tests prove independent production, player-built road access, a full assigned cargo loop, mid-cargo save/reload continuation, plan recovery when a storage is full (including a partially blocked one-node delivery route), and transactional rejection of road closures that break any planned route leg. v1–v6 migrations include nonzero legacy stock, a dynamic v6 road graph, and old player placements overlapping the newly explicit built-in entities; pending plans depending on earlier queued facilities/roads remain reloadable. The viewer now uses a 2.0-world-unit grid to match the road lattice; migrated logical cells are retained, so their synthetic world positions can shift from the earlier 1.25-unit presentation scale. Legacy entities sharing built-in cells receive a display-only offset intended to keep them visible and spatially separated for picking; selection of these migrated overlaps remains runtime-unverified. These are OpenRepublic coordinate/display rules, not a verified WRSR map transform. The client and headless Release CTest suites pass 4/4 and 3/3. A bounded runtime check confirmed that the client opens, pauses, enters/cancels storage placement, selects OR-1003 and displays its inventory, and changes the draft storage endpoint through the selection control. It did not place storage, activate a changed route, deliver cargo through the UI, or save the demo. The graph, capacities, and recipes are synthetic OpenRepublic examples, not recovered WRSR topology or formulas.

### Stage C — One measured economic loop (overlapping weeks 6–12)

Choose one original production chain only after a controlled reference scenario exists. Record initial state, game build, settings/difficulty, elapsed game time, staffing/inputs, and outputs. Implement the OpenRepublic recipe separately from any observed original behavior and compare measured outputs across multiple scenarios.

**Exit:** documented input/output behavior matches the chosen reference within a stated tolerance; 10,000 simulated ticks preserve the selected inventory/accounting invariants and replay consistently under the supported determinism contract.

**Synthetic long-horizon status (21 October 2026):** The current demo economy passes a 10,000-tick Release test, including a save at tick 4,999, reload, and continuation to tick 10,000. Uninterrupted, checkpointed, and resumed canonical saves match at every tick; storage/facility bounds are checked at every tick; delivered flour reconciles to stored flour. This closes the prototype's synthetic stability/replay check only. The reference scenario and any WRSR formula comparison remain open, so Stage C's WRSR-behavior exit is not complete.

### Stage D — First playable alpha (stretch target: weeks 8–16 intensive)

Deliver an independent sandbox with one region (or an open example), roads/trucks, storage, one production chain, a basic budget, pause/speed controls, an OpenRepublic save/load, Lua data reload, logs, diagnostics, and a known-gaps screen.

**Exit:** a new scenario can be started without WRSR running; a player can place a producer and processor, connect a road, deliver inputs, observe production, and save/reload the result. A recorded acceptance run and benchmark include machine/build/config details.

### Stage E — Broader systems (after alpha)

Add citizens and commuting, construction stages, distribution offices, utilities, rail/signal systems, richer economy, Workshop/mod adapters, and selected vanilla save imports only as separately scoped projects with versioned evidence and their own gates. Multiplayer follows only after simulation state, command ordering, and save snapshots are stable.

### Stage F — Modern graphics and scale (after profiling)

Profile realistic scenes before optimizing. Benchmark synthetic targets (for example, 10k buildings/vehicles) as measurements, not promises. Consider large-scale job systems, terrain LOD, more aggressive streaming, optional ray tracing/upscalers, and headless server modes only when the measured bottleneck and API/licensing requirements are known. RTX/DLSS/Frame Generation and one-million-citizen targets are long-term research, not alpha commitments.

## 7. Validation and performance protocol

1. **Reader correctness:** known and corrupt/truncated inputs; bounds checks; byte provenance; deterministic diagnostics; unsupported variants fail clearly.
2. **World reconstruction:** record input map/build, coordinate transform, scale, two or more landmarks, placement tolerances, and rendered screenshot/camera path. Screenshots supplement numeric checks.
3. **Economy:** controlled original-game scenarios with the same settings and elapsed time; record raw observations separately from inferred formula. Compare several input/staffing conditions.
4. **Simulation invariants:** unique IDs, valid graph edges, no accidental resource creation/loss, nonnegative storage where required, and save/load state equivalence.
5. **Replay:** same build, same platform, same seed/configuration, same ordered command stream should reproduce the documented state hash. Cross-build and cross-CPU determinism are separate claims.
6. **Integration:** producer -> vehicle -> processor -> storage -> export/consumption, with road removal and rerouting cases.
7. **Performance:** capture CPU simulation tick p50/p95, GPU frame p50/p95, memory high-water mark, content-load time, time per in-game month, tested entity counts, GPU/driver, resolution, build type, toolchain, and cache condition. Compare original vs OpenRepublic only on a controlled equivalent workload. Rendering FPS and simulation throughput are separate metrics.

Do not claim a speedup from different scenes, settings, game builds, or hardware. Keep a baseline artifact before optimizing.

## 8. Legal, content, and release gates

- The official WRSR general modding page describes creating and submitting in-game mods, including modifying WRSR models for Workshop use. It does not state that model use in an independent standalone game is permitted.
- A legitimate local copy and a read-only loader are project assumptions/safeguards; they are not a conclusion about rights, terms, or reverse-engineering law. Before distributing an asset-reading runtime or a build that uses original content, obtain written rights-holder clarification or appropriate legal review for the exact design and jurisdictions.
- Never distribute WRSR executables, DLLs, proprietary meshes/textures/audio/maps, extracted caches, saves, or files derived from them. Keep raw captures, fingerprints of proprietary files, Ghidra databases, and detailed local analysis outside this public repository.
- Keep independent implementations and factual behavior observations separate from translated/decompiled original source. Follow each source project's license and attribution rules. No dependency or SDK enters without a license/provenance review.
- Keep an asset provenance manifest local. Public test fixtures should be synthetic or explicitly licensed/open.
- Add a review of trademark/unofficial branding, terms of service, reverse-engineering restrictions, and release artifacts before a public runnable release. The initial public repository contains documentation and project-authored files only.

## 9. Risks and responses

| Risk | Effect | Response |
|---|---|---|
| Standalone use of original content is not cleared | Viewer/distribution design may change | Keep early prototype local; get rights-holder clarification/legal review before shipping content or asset tooling. |
| Terrain, placement, or save formats are expensive/partial | Visual compatibility stalls | Bound each investigation; use community facts and differential copies; keep a synthetic/open content path. |
| Community code has incompatible obligations | New source cannot be published as planned | Learn from behavior and docs; implement independently; maintain provenance and license inventory. |
| Game updates invalidate offsets/layout assumptions | Import regressions or wrong reads | Version-gate readers; store exact file identity privately; fail closed on unsupported versions. |
| Tick math or iteration order drifts | Desync, save/replay variance | Single-threaded canonical order first; explicit commands/snapshots; test each platform claim separately. |
| Renderer competes with simulation for ownership/performance | Hard-to-reason state and optimization | Renderer consumes snapshots; simulation is headless and authoritative. |
| Broad features outgrow the team | No complete slice | Build one end-to-end logistics/economy loop; defer citizen parity, multiplayer, and advanced effects. |
| Visual demo is mistaken for compatibility | Misleading project claims | Publish a capability matrix, known gaps, tested game build, and evidence with each milestone. |

## 10. Research sources

Checked 21 October 2026; upstream documentation may change, so recheck versions when starting a new implementation task.

- [Wicked Engine upstream source, MIT license, C++ and Lua documentation](https://github.com/turanszkij/WickedEngine)
- [Wicked Engine documentation](https://github.com/turanszkij/WickedEngine/blob/master/Content/Documentation/WickedEngine-Documentation.md)
- [REA source](https://github.com/morluto/rea)
- [REA installation and Ghidra guidance](https://github.com/morluto/rea/blob/main/docs/installation.md)
- [REA experimental Windows Ghidra P0 scope](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
- [TesmioLoader](https://github.com/MaxLegend/TesmioLoader), [architecture notes](https://github.com/MaxLegend/TesmioLoader/blob/master/docs/01-architecture.md), and [reverse-engineering notes](https://github.com/MaxLegend/TesmioLoader/blob/master/docs/03-reverse-engineering.md)
- [RepublicMP](https://github.com/E3418/RepublicMP)
- [WRSR community plugins and tools](https://github.com/Kespri/WRSR-MODS)
- [Official WRSR general modding documentation](https://wiki.hoodedhorse.com/Workers_Resources_Soviet_Republic/General_modding)
- [MeitouClient reimplementation example](https://github.com/VlcekM/MeitouClient)

## 11. Definition of success

The first alpha is a standalone, reproducible build with one understandable world, an independently modeled and validated road graph, a vehicle delivering cargo, one measured production rule, a versioned OpenRepublic save, Lua-driven data/rules, profiling output, and a clear compatibility matrix. Full WRSR parity is not the alpha definition.
