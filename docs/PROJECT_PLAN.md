# OpenRepublic project plan

Latest consolidated status and handoff: [HANDOFF_2026-10-24.md](HANDOFF_2026-10-24.md).

**Native desktop save slots:** named user-data saves, bounded metadata listing,
three rotating simulation-time autosaves, new-game catalog choices, and a
load/quit flow are documented in [DESKTOP_SAVES.md](DESKTOP_SAVES.md). The
feature uses OpenRepublic checkpoints only; WRSR save compatibility is UNKNOWN.

**Construction profile increment (23 October 2026):** ordered cost/work/recipe
declarations and opaque station rows now have a bounded source profile and an
aggregate-only inspection target. All 488 named local definitions extract;
474 retain construction declarations, with unresolved selectors/formulas
explicitly diagnosed. Full Release builds and CTest **79/79 headless, 80/80
client** pass. [Receipt](DEVELOPMENT_REPORT_2026-10-23_CONSTRUCTION_PROFILE.md).
This complements parallel construction work without changing native gameplay.
Original material-delivery/removal and work-argument semantics remain research
gates; native construction lifecycle and full WRSR parity remain incomplete.

**Current development increment (23 October 2026):** workforce assignment is
linear per production phase/snapshot; disabled workplaces release residents.
Population save/reload bounds and Lua population/trade metadata preservation
are covered by full Release builds and **78/78 headless, 79/79 client tests**.
Matched synthetic workforce benchmarks retain exact hashes. See the
[workforce receipt](DEVELOPMENT_REPORT_2026-10-23_WORKFORCE.md), expanded
[full-parity roadmap](PARITY_ROADMAP.md), and newly established
[construction loader evidence](CONSTRUCTION_EVIDENCE.md). Next gameplay goal:
an explicitly authored native construction lifecycle, with original cost import
still gated on numeric/geometry evidence. Full WRSR feature parity remains
active and incomplete. Workforce source/tests are committed locally as `70f551a`;
the receipt retains the state of its original working-tree checkpoint.

**Current handoff (23 October 2026):** see [HANDOFF_2026-10-23.md](HANDOFF_2026-10-23.md) for the consolidated status, evidence boundaries, build/run instructions, open risks, and next steps.

**Latest - electrical declarations and import identities (22 October 2026):** desktop/headless imports retain exact native source identities independently of rendering. The content reader retains optional electrical coordinates with diagnostics; all 488 local definitions and 143 declarations pass. Both Release builds and CTest68/68 client,67/67 headless pass. Automatic topology still requires port/domain semantics. See the [receipt](DEVELOPMENT_REPORT_2026-10-22_ELECTRICAL_PORTS.md). Global goal remains incomplete.

**Latest - exact placement identities (22 October 2026):** a strict sequential reader now validates observed formats 121/124 through exact EOF and exposes source building-table indices. All 44 supported packages in a 65-file inventory pass native decoding; legacy scans retain unknown identity. Full Release builds succeed with 68/68 client and 67/67 headless tests. This closes a prerequisite for joining utility references; port semantics and automatic imported topology remain open. See [exact-placement receipt](DEVELOPMENT_REPORT_2026-10-22_EXACT_PLACEMENTS.md). The global goal remains incomplete.

**Latest - utility diagnostics and attachment candidates (22 October 2026):** measured generation, eligible demand, consumption, unused supply and shortages now reach both GUI and agents without changing save/hash state. Live GUI/reference checks pass at ticks3 and5, including disconnect/reconnect behavior. The content reader also retains bounded original-format building-reference candidates; automatic topology remains unsupported pending identity/port evidence. See [diagnostics receipt](DEVELOPMENT_REPORT_2026-10-22_UTILITY_DIAGNOSTICS.md) and [attachment boundaries](DEVELOPMENT_REPORT_2026-10-22_NETWORK_ATTACHMENTS.md). The broader goal remains incomplete.


**Latest - utilities (22 October 2026):** native utility kinds, finite component supply, fuel/coproduct gates, atomic demand, GUI/API connect/disconnect and strict v4 persistence are integrated. Authored chain and GUI/reference replay pass; full Release builds succeed with **64/64 client**, **63/63 headless** tests. See [receipt](DEVELOPMENT_REPORT_2026-10-22_UTILITIES.md). Next: evidence-backed real-content topology/generator integration and network diagnostics. The original utility/economy parity objective remains incomplete.

**Latest - storage profiles (22 October 2026):** optional authored exact class/resource capacity mappings now flow through both desktop app-owned regions and the headless region host. Supported storage-only definitions import with no recipe. Full Release builds succeed and CTest passes **60/60 client**, **59/59 headless**; synthetic hauling and checkpoint continuation pass. General/shared pools and utility behavior remain unsupported. See the [receipt](DEVELOPMENT_REPORT_2026-10-22_STORAGE_PROFILES.md). Next: establish supported production/utility boundaries and evidence-backed profiles before real-content delivery claims. Global goal active/incomplete.

**Real-content follow-up:** the integrated region toolbar now has a live exact-binding/iron-haul acceptance check on one real-content region. The independent order waits for missing source stock; original production-state import and gameplay parity remain open. Vehicle labels stay readable with detailed values in tooltips. See the [real-region evidence](DEVELOPMENT_REPORT_2026-10-22_REAL_REGION.md).

**Reviewed local milestone (22 October 2026):** native construction/resource flow, Run/Pause, order/road repair, strict persistence, bounded command history and app-owned region logistics are integrated. The milestone review repairs API handle exhaustion before command admission. Full Windows Release checks pass57/57 headless and58/58 client. See the [milestone review](DEVELOPMENT_REPORT_2026-10-22_MILESTONE_REVIEW.md) and [region-logistics evidence](DEVELOPMENT_REPORT_2026-10-22_REGION_LOGISTICS.md). Historical uncommitted/pause statements below describe earlier checkpoints. The global game goal remains incomplete; real-content and remaining save/load acceptance are open.

**Construction-cost importer boundary (23 October 2026):** the economy-host has an opt-in `--construction-costs` diagnostic that reports retained raw `$COST_*` declarations without conversion. Source review found no native construction-site lifecycle in this checkout; existing placement creates completed buildings and haul orders target ordinary inventories. Conversion factors and directive semantics remain UNKNOWN; no scored construction scenario is supported by the current scenario API. See [receipt](DEVELOPMENT_REPORT_2026-10-23_CONSTRUCTION_COSTS.md), [compatibility catalog](COMPAT_CATALOG.md) and [construction evidence boundary](REVERSE_ENGINEERING_LEDGER.md). The requested native costs-to-site feature remains incomplete.

## Earlier continuation receipts

**Latest source integration:** native Save checkpoint pauses and exports through the typed API to a new file. Windows build and focused3/3 pass; dialog opening is observed, end-to-end GUI save remains unverified. See the [save UI report](DEVELOPMENT_REPORT_2026-10-22_SAVE_UI.md).

**Latest integration:** native road selection and enable/disable controls now use the shared authoritative API. Release build/focused5/5 pass; GUI disable/reopen matches exact headless replay. See the [road-controls report](DEVELOPMENT_REPORT_2026-10-22_ROAD_TOOLS.md).

**Latest source integration:** native haul-order inspection, retarget and cancellation now use exact IDs through the shared API. Release build and focused7/7 pass; GUI loaded retarget preserves truck/cargo, and compact HUD fixes default-window picking. Cancellation acceptance remains pending. See the [order-tools report](DEVELOPMENT_REPORT_2026-10-22_ORDER_TOOLS.md).

**Latest integration:** native continuous Run/Pause now uses the shared authoritative API and a bounded four-ticks/second desktop clock. Focused tests pass; live Run/Pause and manual-step acceptance now matches headless replay. See the [Run/Pause receipt](DEVELOPMENT_REPORT_2026-10-22_RUN_PAUSE.md). The construction receipt below retains the last full-suite and live GUI evidence.

**Active continuation (22 October 2026):** development continues over source base `d409d91`. The native economy now has a fresh authored construction sandbox and complete GUI resource-flow controls, in addition to retargeting, save/access validation, replanning, catalog-bound checkpoints, desktop restoration and command-history compaction. Full Windows Release builds succeed; CTest passes **55/55 headless** and **56/56 client**. A GUI-built four-building/twelve-road/three-haul chain consumes bread; an actual pipe checkpoint matches 24 further ticks after host restart. [Current report](DEVELOPMENT_REPORT_2026-10-22_NATIVE_CONSTRUCTION.md) separates source/tests, live graphical observations and UNKNOWN keyboard acceptance. Changes remain uncommitted and the broader goal remains active. Next: native order inspection/retarget/cancellation and road enable controls, continuous run/pause, in-app checkpoint files, then matched scale profiling and larger-world persistence limits.

**Prepared:** 22 October 2026 (America/New_York)
**Current status (22 October 2026 03:10 EDT / 07:10 UTC): Development is paused at the owner's request; the persistent project goal remains active and incomplete.** Public `main` and `origin/main` are synchronized at `101c8a8`. The v7 OpenRepublic captures remain the latest sun evidence: vector `(0.643, 0.415, 0.643)` aligned with the camera, intensity 6.0, both runs exited 0 with matched 1260×748 output and 62.426% depth coverage, and the baseline is byte-identical to v5. The enhanced central-city crop mean luminance rose 11.7/255. These do not establish WRSR lighting or image parity. Synthetic performance reports give 1.1 ms/tick at 40,000 buildings without hauls and 0.080/0.282 ms/tick at 100/400 haul orders. The batch haul protocol/autopilot work is committed; valid rule reloads now have replay history and rejected reloads are excluded. One shared uncommitted change remains in `tests/economy/economy_bench.cpp`, without a new measurement report. **Next on resume:** inspect and validate that benchmark change, then profile whole-world sorting under haul-heavy input. Separately, add an OpenRepublic-authored synthetic texture to the height mesh; authentic forest/terrain semantics remain **UNKNOWN**. Exact content identities and captures remain private under D-003/D-005.

**Earlier app-owned region-economy status (22 October 2026 01:03 EDT / 05:03 UTC):** Public `main` is synchronized through `da8f460` (`Record verified map LOD capture results`). The working tree makes the bounded map-region economy app-owned by default: the importer builds its `economy::World`, the typed `EconomyApi` handles human actions, and an exact loaded-placement index maps rendered instances to imported building IDs. The explicit `--economy-url` mode remains a separate-host path with approximate type/position matching. Windows Release client and client/headless Release CTest each passed 29/29; a focused importer regression covers source-key identity and duplicate-key transactional rejection. External controller processes initially could not reach the app-owned world, and package identity is not fingerprinted. These are OpenRepublic results; WRSR simulation parity and visual fidelity are not established. The `a28ac80` map LOD preview change has separate real-map runtime evidence for selection of LOD0/1/2 and a successful fallback from a missing LOD2. One repeated LOD0 capture failed once without an output, then succeeded on an isolated retry; this capture-reliability issue remains unexplained.

An OpenRepublic-owned Release capture run resolved the local `school` type and its declared model/material for WRSR build `23935965`, then saved three regular and three material-retaining force-unlit views at 1260x748. Every view passed the app's nonzero depth-coverage gate. The diagonal views show a recognizable complete multi-storey school with repeated windows; this proves model render-path integration, not fidelity. The same-input private ModelViewer reference shows the whole school, but no saved reference image is available in the current local capture directory and the views are not camera/bounds-matched. Model orientation, geometry fidelity, and material fidelity remain **UNKNOWN**. Captures, inputs, and exact hashes remain in private research storage. The owner authorized publication of OpenRepublic-authored source; this does not authorize distribution of WRSR assets or resolve rights for binary distribution using original content. D-005 remains a distribution gate.

**Publication status:** OpenRepublic-authored source through `101c8a8` is published on `origin/main` at this checkpoint. WRSR assets, saves, private captures, and decompiled research remain outside the repository. No binary/content release is cleared by this source publication, and no separate release has been published.

The dated technical updates below retain the status and publication decisions from when they were written. For current status, use this section and the 22 October D-005 update in [project decisions](DECISIONS.md).

**Project:** independent WRSR-compatible client and simulation engine

**NMF render-path update (21 October 2026):** for build `23935965`, static loader review labels flags `0x100`/`0x200` `_TC0`/`_TC1` and confirms eight-byte payloads per vertex. OpenRepublic retains these streams; interpreting each payload as two floats and adapting them to Wicked UV sets 0/1 remains **INFERRED**. Mixed-object UV aggregation is covered by synthetic tests and zero-fills missing object streams as an OpenRepublic preview policy; it does not establish an original loader default or material texture-slot use. Matched regular and neutral-unlit three-view captures from the same paired sample show a dark, stepped lit render and brighter neutral silhouettes. They record the current render appearance only; no source-axis or matched-camera ModelViewer comparison is available. At the time of this 21 October note, no matched ModelViewer view was available. The later zoomed-out reference shows the whole school but remains unmatched, so recognition and visual fidelity remain **UNKNOWN**. Source publication was subsequently owner-authorized on 22 October; D-005 still gates content and binary distribution.

**NMF verification update (21 October 2026):** the app's existing CPU geometry normalization/flattening path is now isolated in a bounded, transactional helper with synthetic coverage for source extents, transforms, multi-object index rebasing, subset ranges/tails, mixed source-normal presence, zero-area triangles, and normal magnitudes. Windows client Release build/CTest passes 14/14 and headless Release build/CTest passes 13/13 after this extraction and capture diagnostic. A read-only attempt to obtain a same-model ModelViewer reference did not produce a capturable reference image; no geometry or shading correction is justified yet. The installed building NMF/MTL pair exits the OpenRepublic probe successfully and produces two regular-render captures that remain dark/stepped, plus two constant-neutral forced-unlit captures with approximately matching sampled silhouette coverage. The latter verifies that the same preview geometry reaches the unlit raster path; because MTL color, UV aggregation, source-normal application, and lighting are bypassed together, it does not identify a shading defect or pass recognition/fidelity acceptance. Keep the comparison and original axis, winding, dimensions, normals, and material interpretation **UNKNOWN** until independent matched evidence is available. This is OpenRepublic adapter verification, not new WRSR format evidence.

**NMF diagnostic audit update (21 October 2026):** the pinned Wicked logger uses a fixed-size `wilog` buffer that truncated the earlier verbose preview record. OpenRepublic now emits a compact CPU-side geometry audit and compatibility-limit record separately. The audit reports parser/helper counts and expected draw-range indices/triangles; these values do not confirm GPU submission, PSO readiness, or rasterization. Private byte comparison verified that the captured local NMF and MTL copies match installed source files, but this session did not obtain a targetable ModelViewer window or a matched reference capture. The secondary community Blender importer does not provide a validated full-model reference; parsing alignment and topology correspondence remain unresolved. Geometry orientation, source normals, topology fidelity, recognition, and material fidelity therefore remain **UNKNOWN**; no renderer correction is justified yet.

**NMF raster verification update (21 October 2026):** the existing application-owned capture path now requires an isolated NMF scene and, for every diagnostic view, reads back Wicked's single-sample main depth buffer. Fresh regular and neutral-unlit Release runs passed the nonzero-coverage check for all three diagnostic views (two diagonals and one level +X side view); invalid readback or zero coverage fails the capture. This verifies that the tested preview writes depth-prepass pixels in OpenRepublic. It does not verify surface correctness, topology completeness, orientation, recognition, final shading, or fidelity. No camera/bounds-matched ModelViewer reference is available, so the geometry and material verdict remains **UNKNOWN**; keep the asset-reading runtime local under D-005.

**Reference and capture verification update (21 October 2026):** a private same-input ModelViewer reference now shows the complete school after the user zoomed out, enabling qualitative comparison of the whole building. OpenRepublic's existing application-owned 11-view capture controls add optional zoom and elevation without changing default camera views. Some OpenRepublic angles show a strongly stepped/ribbed silhouette and dark regular shading against the reference's continuous windowed facade; because camera orientation and bounds are not matched, this is an **OBSERVED** image difference, not proof of incorrect or missing geometry. Geometry, material fidelity, and visual acceptance remain **UNKNOWN**. Local texture-to-map/UV binding remains inferred. The NMF asset-reading runtime remains unpublished under D-005.

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
               Lua data/rules                  Agent API v0
               reload at tick boundary         local JSONL stdio host
                                               Python controller example
```

### Boundaries

- Keep authoritative world state and simulation independent of Wicked Engine's renderer, scene objects, and frame rate.
- Start with a single-threaded, fixed-step simulation and canonical iteration/command order. Measure before introducing parallel work.
- Drive rendering from immutable or read-only simulation snapshots. A slow frame must not alter how much simulation a tick means.
- Use stable OpenRepublic IDs, not pointers or addresses from the game's process. Version public file and message schemas.
- Route world queries and gameplay mutations through one typed `src/agent_api` gateway over `src/sim`; it must call the same simulation validation used by the human UI and tests. Rendering, Lua rules, and external controllers do not become competing state owners.
- The typed C++ Agent API is the internal contract. Its world query exposes the current simulation rules and cumulative flour deliveries from one authoritative snapshot; the local JSONL host serializes the `uint64_t` counter as a decimal string. A local host and external Python controller are verified against the same synthetic simulation. The host is optional when the pinned submodule's nlohmann/json header is absent; it is trusted-local, has no network listener, and its observer/controller flag is not authentication. MCP, network service, C# controller, and production multi-client access remain future work.
- Apply player commands at tick boundaries. Record commands and seed/configuration for reproducible replay within a stated same-build/same-platform contract; cross-platform bitwise determinism is a separate target.
- Keep external C# AI out of the authoritative process initially. Send versioned observations and commands through a local IPC protocol; reject stale or invalid command batches.
- Treat Lua as a data/rule extension point with schema checks, a tick-boundary reload, and rollback if a rules bundle fails validation. It is not a substitute for profiling native simulation code.
- Keep the first import path read-only. Never edit the Steam install or write converted files back over original content.

### Initial module sketch

```text
src/
  app/          window, input, runtime coordination
  sim/          headless world, tick, commands, inventory, routing, production
  agent_api/    typed in-process queries, permissions, revisions, events, commands
  agent_host/   optional bounded local JSONL stdio adapter
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
| F3: terrain/coordinates | Decode the bounded legacy R32F height layer and metadata; then compare a small map/region against two known landmarks and elevation references. Treat save-derived object coordinates and height transforms as candidates until independently checked. | Terrain DDS/metadata parsing, a decimated preview, a generic caller-supplied X/Z bilinear sampler, and a preview grid that retains caller-supplied world X/Z coordinates and supports eight caller-selected raster orientations have synthetic test coverage. The remaining exit requires a repeatable region view with verified coordinate transform and stated tolerance, or a concise evidence-backed blocker report. |
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
- The developer-only `--glb-probe <file.glb>` validates exact in-memory bytes against an OpenRepublic-owned, bounded glTF 2.0 profile before importing a temporary snapshot through the pinned Wicked Editor `ModelImporter_GLTF.cpp`. The profile accepts embedded BIN and buffer-view images, finite static triangle meshes with supported vertex layouts, bounded accessors, unit-length node quaternions within a 1e-3 norm-squared tolerance, and validated material/texture/sampler references; it caps aggregate float-component and index-entry validation at 16,777,216 scalar reads and rejects URI resources, extensions, sparse accessors, morph data, animation, skins, cameras, unsupported attribute layouts, and inconsistent node graphs. Synthetic fixtures cover valid and invalid cases, references, binary ranges, geometry/index bounds, non-finite values, quaternion validation, aggregate scan limits, and transactional result handling. The client checks imported indexed triangle data before merging the staging scene. `tools/create_synthetic_glb.py` remains an OpenRepublic-only fixture; earlier runtime logs reported 1 mesh, 1 material, and 2 transforms for it. A private scratch model-derived GLB earlier reported 3 meshes, 2 materials, and 4 transforms but contained no embedded images, so it does not establish WRSR material fidelity. A separate `--scene-probe <file.wiscene>` call added one transform from a Wicked Engine sample archive. The GLB importer is still Editor code, is not sandboxed, and pixel decoding, visual fidelity, broader NMF parsing, and standalone content rights remain open.
- Bounded runtime checks verified readable HUD, pause/step, road edge 202 close/reopen, mill picking (`OR-1002` highlight/title), mouse-wheel zoom, synthetic placement and GUI save/load. Lua startup showed the bundled rates. While paused, a temporary edit to farm `5 every 3 ticks` and mill `2/tick` applied immediately; out-of-range farm value `13` was rejected while the `5/3/2` rates remained active. While running, a second valid edit (`4/1/2`) applied at the next fixed tick boundary. The build-local rule copies were restored from source defaults afterward. Keyboard movement remains unverified. Prototype placement has no costs, build stages, material logistics, or WRSR placement rules; screenshots were not committed.
- The NMF header reader retains the bounded count/identifier profile. The geometry reader is limited to build `23935965` loader evidence and one local sample: static type-0 nodes only, root/no-child metadata, identity raw matrix fields, and exactly one LOD per node. The inspected renderer chooses one LOD per draw; the parser does not flatten LODs. For build `23935965`, the loader has no byte-consumption branch for flag `0x1` (meaning unknown) and consumes flag `0x40000` as two per-triangle arrays of 16 then 24 bytes. It preserves the unproven node +4 size field but advances through type-0 payloads by declared counts/flags and then requires the header-declared file size and sequential parse cursor to match. It caps input at 256 MiB, object nodes at 4096, aggregate vertices at 4 million, indices at 12 million, absolute position components at 1,000,000, and 65,536 subset records plus 12 million aggregate subset indices. It retains raw names/matrices/AABBs; +0x10/+0x18 morph interpretations remain inferred. In build `23935965`, loader format creation appends `_NRM` for flag `0x8` and adds a float3 element, and `LoadFromMemory` copies that stream into its per-vertex field. The preview applies the raw values before Wicked render-data upload and uses generated smooth normals only as fallback. The loader's caller-controlled X reflection is not applied by this direct preview, and the original model call setting remains unknown. It combines accepted objects while preserving each observed subset draw range and submaterial-table index; the preview uses an eight-entry deterministic debug-color palette (repeating after eight entries) and normalizes geometry to a 10-unit span. This does not recover original MTL values or texture binding, and auxiliary subset indices remain opaque and are not treated as vertex indices. This is not general NMF support or a validated visual match. External texture/material fidelity, terrain/world placement, game units, visual fidelity, graphics API selection, and performance remain open. No original game file or model-derived output entered Git.
- The typed Agent API v0 and local JSONL stdio host are verified by Release build, CTest, and an external Python controller. The API bounds one step call at 10,000 ticks, retained command results at 64, entity pages at 32 records, the event journal at 128 entries, and event pages at 32 entries. The event journal and revision are process-local. Network access, MCP, C# access, and authentication are not implemented. See `docs/AGENT_API.md` for the protocol, limits, and role caveat.

Reproduce the OpenRepublic target from a VS 2022 Developer shell (quote the SDK argument in PowerShell):

```powershell
cmake -S . -B build/openrepublic-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143 "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic OpenRepublicSimulationTests OpenRepublicAgentApiTests OpenRepublicTerrainReaderTests OpenRepublicLuaRulesTests --parallel
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

**Continuation status (22 October 2026, 02:12 EDT):** the default local map-region path builds an app-owned `economy::World`; map picks carry the exact source placement index from that loaded region through import; and human inspect/toggle/haul/step calls use its typed `EconomyApi`. The opt-in Windows named-pipe endpoint (`--economy-pipe-name`) borrows that same API, shares its session/revision/command state, requires session binding after handshake, and disables embedded shutdown. A live pipe smoke on a local read-only region exercised hello, capabilities, catalog/world queries, stale-session rejection, revision-checked toggle submission, stepping, polling, and changed world state; closing with an idle connected client also exited cleanly. These checks do not establish WRSR behavior or graphical acceptance. The placement index is exact only for one loaded vector, not durable across package revisions; the package is not fingerprinted. A new `--synthetic-region-fixture` launch path creates OpenRepublic-only primitive visuals and plain placement/road data, then feeds the existing generic importers and synthetic catalog. The Release target built, and the first app frame displayed its region controls plus three imported buildings, but its 3D viewport appeared black and the status panel clipped text. Computer Use stopped on the user's physical Escape before any click, so picking and economy actions remain **UNKNOWN**. A revised status summary built and linked to separate private scratch output but was not launched. No tests ran for the fixture increment. **Next goal target:** diagnose the black synthetic viewport and status clipping, then complete a graphical fixture run for selection, inspection, toggle, compatible haul, stepping, status, and truck-marker presentation. The endpoint remains a trusted-local surface, not authentication; its single-client idle-read limit remains in D-009 and the API guide. The actual-content graphics showcase remains separate and still lacks verified terrain materials, vegetation, and native vehicle rendering.

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

**Graphics showcase continuation (22 October 2026):** a Release-only map-region capture mode now writes an actual Wicked Engine render target after a bounded warm-up, fixed-camera application, and nonzero depth-coverage check. A matched baseline/enhanced pair on one real map region confirmed same dimensions and scene/camera summaries. The enhanced look exercises directional shadows, a graded sky, volumetric clouds, height fog, SSAO, FXAA, bloom, sharpen, and more restrained flat ground materials. This is a renderer-path demonstration, not performance evidence or an original-game visual match. The current region lacks imported terrain textures, vegetation, native traffic/rail/water, and decoded road curves; these remain separate visual-content targets. Exact local input identities and screenshot hashes stay outside the repository.

The synthetic/open fixture does not advance that real-content showcase. Its first interactive frame had a black 3D viewport, so scene visibility is now an immediate acceptance blocker before returning to content/material work.

**Camera-side lighting follow-up (22 October 2026 02:52 EDT):** the enhanced sun direction already matches the fixed camera's outward ray. The preset now uses intensity 6.0 rather than 4.2 to make that light more visible on the same captured city. A fresh matched v7 Release pair preserved the baseline byte-for-byte versus v5 and kept equal scene summaries, 1260×748 dimensions, and 62.426% depth coverage; the enhanced central city crop mean luminance increased by 11.7/255. This only improves OpenRepublic presentation lighting. It does not resolve the scene's missing terrain surfaces, vegetation, native traffic/rail/water, curved roads, or facade material fidelity. **Next:** resolve one build-specific forest or terrain source path from retained evidence before implementing an adapter.

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
