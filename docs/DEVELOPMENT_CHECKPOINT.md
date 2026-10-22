# OpenRepublic development checkpoint

**Real-content follow-up:** the integrated region toolbar now has a live exact-binding/iron-haul acceptance check on one real-content region. The independent order waits for missing source stock; original production-state import and gameplay parity remain open. Vehicle labels stay readable with detailed values in tooltips. See the [real-region evidence](DEVELOPMENT_REPORT_2026-10-22_REAL_REGION.md).

**Reviewed local milestone (22 October 2026):** native construction/resource flow, Run/Pause, order/road repair, strict persistence, bounded command history and app-owned region logistics are integrated. The milestone review repairs API handle exhaustion before command admission. Full Windows Release checks pass57/57 headless and58/58 client. See the [milestone review](DEVELOPMENT_REPORT_2026-10-22_MILESTONE_REVIEW.md) and [region-logistics evidence](DEVELOPMENT_REPORT_2026-10-22_REGION_LOGISTICS.md). Historical uncommitted/pause statements below describe earlier checkpoints. The global game goal remains incomplete; real-content and remaining save/load acceptance are open.

## Earlier continuation receipts

**Latest source integration:** native Save checkpoint pauses and exports through the typed API to a new file. Windows build and focused3/3 pass; dialog opening is observed, end-to-end GUI save remains unverified. See the [save UI report](DEVELOPMENT_REPORT_2026-10-22_SAVE_UI.md).

**Latest integration:** native road selection and enable/disable controls now use the shared authoritative API. Release build/focused5/5 pass; GUI disable/reopen matches exact headless replay. See the [road-controls report](DEVELOPMENT_REPORT_2026-10-22_ROAD_TOOLS.md).

**Latest source integration:** native haul-order inspection, retarget and cancellation now use exact IDs through the shared API. Release build and focused7/7 pass; GUI loaded retarget preserves truck/cargo, and compact HUD fixes default-window picking. Cancellation acceptance remains pending. See the [order-tools report](DEVELOPMENT_REPORT_2026-10-22_ORDER_TOOLS.md).

**Latest integration:** native continuous Run/Pause now uses the shared authoritative API and a bounded four-ticks/second desktop clock. Focused tests pass; live Run/Pause and manual-step acceptance now matches headless replay. See the [Run/Pause receipt](DEVELOPMENT_REPORT_2026-10-22_RUN_PAUSE.md). The construction receipt below retains the last full-suite and live GUI evidence.

## Current continuation - native construction and resource flow (22 October 2026)

The [native construction report](DEVELOPMENT_REPORT_2026-10-22_NATIVE_CONSTRUCTION.md) records `--native-economy`, an empty authored 64x64 world, exact native-cell placement through the retained scene transform, straight unit-road commands, explicit resource/vehicle selectors, removal and Cancel controls, 240-tick steps and selected recipe progress. Placement remains authoritative on stepping; a draft preview does not imply validity. Road batches apply independently. Removal uses the existing inventory-discarding native command and rejects referenced haul endpoints.

**VERIFIED:** full Windows Release builds succeed; CTest passes **55/55 headless** and **56/56 client**. New helper/core regressions cover inverse coordinates, storage intersections and a complete deterministic production chain. **OBSERVED:** GUI-only construction creates four buildings and twelve roads (16 applied); three explicit hauls produce market consumption of 108 cycles by tick 241. An overlapping placement rejects visibly at tick 242. Actual pipe export is 1,378 bytes at tick 242/revision 262/hash 5904435322412779883; a fresh host matches 24 subsequent ticks to tick 266/hash 13674903108919557831. Palette dismissal and Cancel leave zero pending work and unchanged hash; injected Escape dismissal was not visibly verified and remains **UNKNOWN**. Task-owned windows/hosts are closed. No original game or proprietary input was used. Changes remain uncommitted; next work is native order/road controls, continuous run/pause and in-app checkpoint workflows.

## Earlier continuation - explicit command-history compaction (22 October 2026)

The [command-history report](DEVELOPMENT_REPORT_2026-10-22_COMMAND_HISTORY.md) records explicit terminal-prefix compaction, persistent retained-ID floors, v3 native saves for compacted worlds, API handle retirement/expiration, strict session/revision-bound JSONL maintenance and capability-discovered autopilot integration after outcome polls. Pending work, entity IDs, cargo and production survive; actual pruning increments revision once without a tick. Export remains read-only. Uncompacted worlds retain v2 bytes/hashes; an earlier native checkpoint still matches its pre-change hash. Existing files are not rewritten by this work.

**VERIFIED:** full Windows Release builds succeed; concurrent CTest passes **54/54 headless** and **55/55 client**, including 16 autopilot unit cases. A 75,008-command process stress fixture exceeds the save bound, retires 74,996 results/API mappings with pending cargo work preserved, exports 1,395 bytes and matches 24 resumed ticks with all 12 units conserved. Actual autopilot CLI zero/all retention modes retire 33/0 results. Independent review's handle/native-ID alias was repaired with an offset-ID regression; final review found no further actionable issue. No GUI/original-game comparison or performance measurement ran for this increment. Changes remain uncommitted; the full goal remains active. Next: native construction/resource-flow UI, complete Farm-to-Mill-to-Bakery-to-Market acceptance, matched profiling and larger live-world/queue persistence limits.


## Earlier continuation - desktop checkpoint restoration (22 October 2026)

The [desktop checkpoint report](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md) records startup restore into the same authoritative economy used by GUI/pipe controllers, exact native-ID picking, X/Z footprint/road/truck primitives, snapshot reconciliation, raw inventory amounts and explicit Frame world. Bounded startup and float-precision validation precede window creation. Ordinary updates preserve the camera/origin; explicit framing recenters geometry and preserves selected native ID without changing simulation state.

**VERIFIED:** full Windows Release builds succeed; concurrent CTest passes **50/50 headless** and **51/51 client**. A restored desktop begins at the exact checkpoint hash and matches four reference-host steps through pending retarget, building removal/placement and road changes, conserving all 24 units. Live pipe export resumes identically. **OBSERVED:** the new market selects as OR-6; framing retains selection, GUI toggle/steps move a loaded truck through the vertical turn, and restored mill OR-3 runs at tick 44 with grain 2/8 and flour 6/8. The actual pipe export CLI then resumes in a headless host at tick 44, revision 97, hash 6564545351195520907. Task-owned test processes are closed. No original game/proprietary fixture was used; changes remain uncommitted and the broader goal remains active. Next useful work is versioned long-running history/size semantics, native construction/resource-flow controls and matched scale profiling.


## Earlier continuation — catalog-bound native checkpoints (22 October 2026)

The [checkpoint report](DEVELOPMENT_REPORT_2026-10-22_CHECKPOINTS.md) records native checkpoint v1, transactional owned restore and exact expected-catalog loading, query-only revision-checked export, immutable Base64 chunk reads, an atomic file-export CLI and standalone host `--checkpoint` startup. API sessions, handles and event journals restart; authoritative world state, queued commands/results, counters and the active catalog persist. Later Lua tuning uses the embedded catalog as its new base. Full Windows client/headless Release builds and concurrent CTest each pass **49/49**.

A separate-process fixture resumes four units aboard a truck with a pending retarget, starts at equal tick/revision/hash, then matches the original hash for 32 ticks while conserving 12 total units. Actual loopback HTTP CLI export is byte-identical and honors default no-overwrite / explicit force. Framing/canonical-catalog/size errors, binary chunks, permissions, snapshot invalidation and startup flag/file errors have focused coverage. Changes remain local and uncommitted; no original game process or proprietary fixture was used. Desktop checkpoint restore/render bindings, live pipe export, larger/long-running world persistence, cross-platform replay and WRSR comparison remain incomplete.

## Earlier continuation — autonomous replanning and GUI follow-up (22 October 2026)

The [replanning report](DEVELOPMENT_REPORT_2026-10-22_REPLANNING.md) records additive read-only route/capacity-aware suggestions for loaded full-storage stalls and controller submission/polling. Output is capped at 32 emitted suggestions without starving later feasible orders; reservations account conservatively for loaded inbound and suggested cargo. The controller refreshes after rejected placement attempts and stale advice and avoids duplicate retargets across its bounded refresh. Full client/headless Release builds and CTest pass **44/44** each, including ten Python controller unit cases and a matched separate-process comparison: at tick 23, four units delivered to alternate storage versus idle zero, all 24 units conserved, original order/truck retained.

The owner answered the foreground-coordination question and released the PC for testing. The post-change GUI pass observed farm inventory 20/20 at tick 26, small road-aligned truck marker movement across single steps at ticks 27/28, and mill grain/flour 8/8 at tick 52. A second short-window pass observed overflow wheel scrolling, scrollbar dragging and selection preserved on panel clicks, with no apparent scene zoom from scrolling. The synthetic fixtures were closed. No pending foreground question remains. Changes are uncommitted and the broad goal remains active; native economy persistence/catalog identity and matched profiling are next useful background goals. No original game process or proprietary test input was used.

## Earlier continuation — building access and region UI (22 October 2026)

The [access and UI report](DEVELOPMENT_REPORT_2026-10-22_ACCESS_AND_UI.md) records the current local increment. Building placement/load now preserve footprint, access and enabled/disabled road reservations, with separate import diagnostics and accepted-only source identity. Full client/headless Release builds and CTest each pass **41/41**. A session-bound controller verified the four placement failure categories. A live synthetic GUI run showed the scene, picking, toggle/step, and Farm-to-Mill production; the earlier black viewport did not reproduce. Added single-tick control, wrapped status/native synthetic quantities, smaller road-aligned markers, and overflow input guards. These presentation changes build, but the final selected-status, single-step movement and overflow scroll pass is pending foreground coordination after desktop input contention. Do not issue further GUI inputs until the owner answers the pending coordination question. Continue independent background work meanwhile. Changes remain uncommitted; the full playable-alpha goal is active and incomplete.

## Earlier continuation — economy save integrity (22 October 2026)

The [save-integrity continuation report](DEVELOPMENT_REPORT_2026-10-22_SAVE_INTEGRITY.md) records the current local increment. The loader validates typed numeric tokens, canonical ID order/counters, dense command history, queued-result consistency, booleans and counter budgets transactionally. Valid histories above 1,024 records now reload; oversized writes return an explicit empty result at the shared 1 MiB limit. Queueing, stepping and rule reloads preserve counter execution space. Full Windows client and headless Release builds succeeded and both CTest suites passed **40/40**. Changes remain local and uncommitted; the full playable-alpha goal remains active and incomplete. Next: building access invariants, then graphical synthetic-economy acceptance.

## Earlier continuation — cargo-preserving haul replanning (22 October 2026)

Development resumed at the owner's request over source base `d409d91`. The full playable-alpha project remains incomplete. See [the current development report](DEVELOPMENT_REPORT_2026-10-22_LOGISTICS.md) for reviewed project state, implemented features, reproduction commands, and ranked follow-up goals.

**VERIFIED:** Windows client and headless Release builds succeeded; client CTest **39/39** and headless CTest **39/39** passed. New tests cover loaded/empty/full/disconnected haul retargets, cancellation without cargo loss, return-to-source behavior, deterministic v2 saves with v1 migration, strict controller parsing/permissions/revisions, and transactional rule reloads. A separate session-bound controller process delivered four units to the redirected depot, none to the old depot, and conserved twelve total units through tick 32. No GUI or original-game comparison ran.

**Working tree:** five documentation files were already modified at task start and have been preserved. This increment remains local and uncommitted; no publication was performed. The earlier benchmark-edit and revision statements below are historical, superseded by the later handoff and current source inventory.

## Historical documentation checkpoint (superseded)

**Checkpoint:** 2026-10-22 03:10 EDT / 07:10 UTC (owner-requested documentation checkpoint and development pause)
**Repository:** OpenRepublic workspace
**Published base revision:** `101c8a8` (`Add rules_history for replaying rule reloads`), synchronized with `origin/main` at checkpoint time.
**Branch:** `main`
**Development status:** Paused at the owner's request. **Persistent goal status:** Active and incomplete.
**Working tree:** `main` and `origin/main` are at `101c8a8`. Shared commits measured the 100/400-order haul profile (`737a372`), recorded the Streamline checklist (`2856513`), added batch haul support (`e41977f`), switched the autopilot to batching (`860096c`), and added replay history for valid rule reloads (`101c8a8`). The host batch method was reported verified with headless CTest 29/29; the latest progress note says valid reloads are listed and rejected reloads are omitted. The only shared uncommitted code change now is `tests/economy/economy_bench.cpp`; it has no new result at this snapshot. This pause task ran no build, test, or capture. Status-document changes remain uncommitted.

## Sun adjustment and capture evidence

The requested sun adjustment is complete for the fixed showcase camera. The enhanced sun vector follows the camera's outward yaw/elevation and is `(0.643, 0.415, 0.643)` for yaw 45 degrees and elevation 24.5 degrees. It is already collinear with the camera's view ray; moving the directional sun farther along that same direction cannot change its lighting direction. The prior enhanced capture build used intensity 4.2, while the current enhanced preset uses 6.0; the light is constructed at 4.0 before the enhanced override. Yaw, elevation, color, and baseline look remain unchanged.

The serial v7 Release capture pair used the same read-only map region, explicit center, radius, LOD, and camera. Both runs exited 0 and logged 75 building instances from 37 models, 81 selected placements, 470 road links drawn as chords, one unresolved type, two failed model loads, target `(0, 7.775, 0)`, distance 375 m, 1260×748 output, and equal 62.426% depth coverage. The v7 baseline is byte-identical to v5. **OBSERVED:** enhanced central-city-crop mean luminance increased by 11.7/255 versus v5; several facades remain dark. Exact source identities, captures, hashes, and logs remain private. This is an OpenRepublic presentation result, not WRSR lighting or image-match evidence.

## Latest development report and next target

The latest synthetic 40,000-building benchmark measured about 1.1 ms/tick with no haul orders. Production-step timings at 1/2/4/8 threads were 1.13/1.01/1.01/1.12 ms/tick, so thread-level production has no meaningful speedup at this workload. The committed 100/400-order haul profile measured 0.080/0.282 ms/tick, about 0.7 microseconds per order per tick; the log reports no bottleneck at these sizes. Commit `e41977f` adds a batch haul-order host method of up to 64 orders; the host path was reported verified with headless CTest 29/29. Commit `860096c` makes the autopilot use batching; the progress log reports the N22 result unchanged at 952.875 stored units, 14 orders, tick 6,016. Commit `101c8a8` adds replay history for accepted rule reloads and omits rejected reloads. The remaining uncommitted benchmark-source change has no new result. Whole-world sorting remains the next economy performance target.

For the graphics stream, the retained current graphics-module forest-import view **OBSERVED** a count/data block, a record loop, a separate types input, and material creation/loading. Counted records plus separate type definitions are **INFERRED**; record field meanings, tree/species identity, units, terrain sampling, mask semantics, and terrain-material selection remain **UNKNOWN**. A similarly named terrain initializer in the older executable evidence cannot establish current-module behavior or justify address reuse. Exact module identity and raw REA output stay private under D-003.

**Next target on resume:** inspect and validate the remaining uncommitted `tests/economy/economy_bench.cpp` change, then profile whole-world sorting under haul-heavy synthetic input. The next safe graphics increment is separately an OpenRepublic-authored synthetic texture on the existing height mesh; it must not claim WRSR terrain semantics. Defer authentic vegetation parsing until current, version-scoped evidence supports fields and coordinate mapping.

## Earlier graphics continuation — camera-aligned sunlight (02:37 EDT)

The owner's latest direction was to move the sun further behind the camera. In the enhanced map showcase, the sun elevation now follows the same camera elevation used to compute the camera eye, while retaining the camera yaw. This makes the sun's world-space direction match the camera's outward direction exactly for the fixed showcase view. The baseline look and the height-mesh presentation tint are unchanged.

The Windows Release `OpenRepublic` target rebuilt successfully. An initial validation pair omitted the explicit center index and automatically selected a different region; that pair is excluded from the comparison. The corrected v5 pair supplied the same explicit center, radius, LOD, and camera as v3. The v5 baseline is byte-identical to v3, confirming an unchanged baseline scene/look; both app processes exited 0. Both logs match at 75 building instances from 37 models, 81 selected placements, 470 road links drawn as chords, one unresolved type, two failed loads, camera target `(0, 7.775, 0)`, distance 375 m, yaw 45°, elevation 24.5°, 1260×748, and 62.426% main-depth coverage. The enhanced log reports sun vector `(0.643, 0.415, 0.643)`, matching the camera vector at that yaw/elevation. Exact capture hashes and inputs are in the private receipt.

**VERIFIED:** Release build, matching scene/camera logs, baseline byte identity, both capture exits, and equal nonzero depth coverage. **OBSERVED:** the lighting direction is now directly behind the camera; the facades remain dark and the broader city showcase is still incomplete. **NOT RUN:** CTest or performance measurement. Original inputs remained read-only; no original-game process was launched. Exact source identities, commands, logs, and captures stay in private research storage.

**Next goal target:** validate one build-scoped terrain-material or vegetation-placement record before importing it into this same scene. Keep unknown source semantics unknown, and do not use synthetic scattering to stand in for authentic placement data.

## Earlier graphics continuation — camera-side sunlight and height-mesh terrain tint (02:31 EDT)

The OpenRepublic Release target rebuilt successfully after a focused presentation change in the existing map-region renderer. The enhanced preset now derives the sun's horizontal direction from the showcase camera yaw, aligns Wicked's directional light and sky to the transform's local +Y axis, removes the brown height-fog treatment, uses a clearer blue sky with lighter volumetric cloud cover, and adds deterministic vertex tinting from the already decoded height mesh, slope, elevation, and world coordinates. The tint is OpenRepublic presentation styling only; it does not interpret map masks or assert original-game surface semantics. The baseline branch retains its prior ground and default-light look.

Fresh baseline and enhanced app runs used the same private read-only map input, explicit region center, LOD 0, and camera target/distance/yaw/elevation. Each log reports 75 building instances from 37 models, 81 placements selected, 470 road links drawn as straight chords, one unresolved type, two failed loads, 1260×748 output, and 62.426% main-depth coverage. Both processes exited 0 and used the same camera. The enhanced log reports a camera-side sun vector of `(0.557, 0.616, 0.557)` at yaw 45°, consistent with the selected 38° sun elevation and the actual Wicked light axis. **OBSERVED:** the enhanced frame has a blue daylight sky, lighter cloud cover, directional shadows, and low-frequency ground variation; the earlier muddy haze is gone. The pair still does not meet the requested hero-image target: many building facades remain dark, the road graph appears as chords, and terrain textures, vegetation, native vehicles, rail, and water are not imported. Exact input paths, hashes, and PNGs remain private under D-005.

**VERIFIED:** Windows Release `OpenRepublic` build and app capture exits/depth gates. **NOT RUN:** CTest and hardware performance measurements. **UNKNOWN:** match to original WRSR screenshots, map-surface texture semantics, tree-placement records, native traffic/rail/water, road-curve reconstruction, and broad building-material fidelity.

**Next goal target:** resolve and implement one version/build-scoped real vegetation-placement or terrain-material path from private read-only evidence, then render it in this same city region. Do not scatter synthetic trees or infer mask meanings; retain the matched baseline/enhanced capture for each visual iteration. The broader graphics showcase and playable OpenRepublic goal remain active and incomplete.

## Earlier synthetic/open fixture first-window issue (02:10–02:12 EDT)

Added `--synthetic-region-fixture`, a self-contained OpenRepublic-authored scene and economy input for exercising the app-owned region path without WRSR files. It creates a ground plane, one visible road object, and three selectable Farm/Mill/Bakery block visuals; a parallel plain-data fixture feeds the generic region and road importers and the synthetic OpenRepublic economy catalog. The fixture source keys directly bind each displayed object to its imported economy building. The option rejects game content roots, map packages, probes, captures, and external economy endpoints. Initial status was shortened to report fixture/import counts and app-owned tick/building/truck counts; detailed importer notes go to the application log.

Windows Release `OpenRepublic` built successfully with this source. The first executable was launched at 02:10 EDT with `--synthetic-region-fixture`; the initial app frame showed the four economy controls and status text reporting three placed buildings from three placements, with zero unknown types, overlaps, or out-of-region placements. **OBSERVED:** the 3D viewport appeared black in that frame, and the status text was clipped by the panel. The Computer Use tool then reported that it had been stopped by the user's physical Escape key before any UI action was taken. Selection, inspection, toggle, haul, stepping, status refresh, and truck-marker presentation remain **UNKNOWN**. A follow-up status-panel source adjustment compiled and linked to private scratch output because the running app held the normal output executable open; that updated executable was not launched. No test suite was run. The initial screenshot was not saved as a local artifact.

**Follow-up observed later (~02:15 EDT):** the synthetic fixture's 3D scene became visible and pick/inspect plus building-toggle actions were exercised. Paused steps advanced observed fixture state through ticks 2, 26, 50, 74, and 98. A Farm haul source and Mill destination were selected and one truck was present. Truck movement was not verified, so the full logistics interaction and marker-motion acceptance remain **UNKNOWN**. This is synthetic UI evidence only and does not establish real-map visual fidelity.

**Remaining acceptance target:** verify truck movement and visible marker updates in a fresh bounded fixture run if graphical interaction becomes available again. Keep this synthetic control-path acceptance distinct from the real-content graphics showcase.

## Previous milestone — app-owned trusted-local controller endpoint

The Windows desktop app can now opt in to a named-pipe controller endpoint with `--economy-pipe-name <name>`. The endpoint borrows the same `EconomyApi` used by the app-owned region UI, requires a process-local session ID after `hello`, keeps mutations on the application update thread, and omits embedded shutdown. Release build, focused dispatcher tests, and a live local controller smoke passed. A read-only imported region was used for transport wiring evidence; synthetic/open-fixture graphical interaction remains the next target.

## Previous milestone — matched-camera map-region graphics showcase

## Initial map-region graphics showcase baseline (01:31 EDT)

Added a one-shot `--showcase-capture` path for the actual map-region renderer. It accepts an explicit `baseline` or `enhanced` look plus fixed yaw, elevation, and region-radius camera distance. The render path waits 120 frames for startup, settles another 48 frames, checks the main depth buffer, and writes the 3D render target without the GUI. It refuses to overwrite existing outputs. Existing NMF capture behavior remains separate.

The enhanced look adds a warm directional key light with shadow casting enabled, brighter ambient fill, a graded Wicked sky, volumetric clouds, height fog, SSAO, FXAA, bloom, sharpening, and more restrained flat terrain/road material values. The two capture runs used the same private map package and automatically selected a center placement: both rendered 21 building instances from 11 models, 70 decoded road links as chords, and the same `(0, 8.260, 0)` target at 375 m, yaw 45°, elevation 24.5°. Both Release runs exited 0, produced 1260×748 frames, and had equal 62.310% main-depth coverage. The images visibly differ in sky, color, and shading.

This is an app-owned rendering comparison on local WRSR-derived content, not an original-game visual-match result. One type was unresolved. The map renderer still has no imported terrain textures, vegetation, native vehicles, rail, or water geometry; roads remain straight chords. A first realistic-sky-LUT tuning pass made the sky black below the cloud band, so the captured enhanced preset uses a graded procedural sky with height fog. The private receipt preserves the exact read-only input provenance, source hashes, commands, and image hashes. No WRSR process was launched, and no original files were modified.

The app-owned endpoint milestone is complete as a local integration increment. The current acceptance target is the synthetic/open fixture interaction described above. In the separate graphics stream, the real-content showcase still needs a supported terrain-material path and authentic vegetation/vehicle content before claiming a full city showcase. The long-term playable OpenRepublic goal remains active and incomplete.

## Previous checkpoint at 01:03 EDT — app-owned map-region economy integrated

The default map-region path now creates an app-owned `economy::World` and typed `EconomyApi`. It builds the bounded catalog from local content definitions, maps accepted region placements into that world, imports the decoded road graph, and routes region inspection/toggle/haul/24-tick-step actions through the typed API. Rendered map instances carry the exact placement-vector index used during import, so selection resolves to the corresponding imported economy building ID without spatial nearest-neighbor guessing. It no longer advances or presents the hidden synthetic Farm/Mill/Storage demo in region mode. An explicit `--economy-url` still selects the legacy separate-host mode; that path retains its process-local session binding and approximate type/position mapping.

The latest recovery increment adds a process-local `sessionId` to every economy-host response and accepts that ID as an optional request binding. A mismatch returns `stale_session` before dispatch. The desktop bridge client handshakes on connection, binds catalog/world/mutation/command-poll requests to the observed ID, and clears its cached world/catalog, queued requests and command handles if the session changes or a response is malformed. It polls one command handle per cycle and rotates pending handles for fairness. The ID is a replacement marker, not authentication. A finite 10,000,000-request host lifetime cap allows multi-day local UI sessions. Release builds for `OpenRepublic`, `OpenRepublicEconomyHost` and host tests succeeded; the new host regression verifies a stale-session `step` is rejected without advancing the tick. Full client and headless Release CTest both pass 29/29. Neither suite directly exercises the overlay's asynchronous HTTP thread or graphical button flow.

The Windows Release client and focused economy-host/importer test targets built after this increment. Full Release CTest passed 29/29 in the client tree and 29/29 in the headless tree. Importer tests verify source-key-to-economy-ID mapping, repeat determinism, and duplicate-key transactional rejection. These checks do not directly exercise graphical picking, buttons, or live marker updates. The app-owned map-region UI has not been runtime-accepted, and external controllers cannot yet connect to its in-process world. The source placement index is exact only within the loaded placement vector and is not a durable identity across map/package revisions; package identity is not fingerprinted. No WRSR process was launched and no game files or saves were modified.

The earlier one-model fidelity milestone remains a separate **UNKNOWN** acceptance item. Its local Release run resolved the `school` type and declared model/material, then captured three regular and three material-retaining force-unlit 1260x748 views through the app-owned render-target path. All six passed nonzero depth coverage and the complete building and repeated window facade are recognizable in diagonal views. Geometry, material fidelity, and visual acceptance remain **UNKNOWN** because the same-input ModelViewer view is not saved in the local capture directory and the cameras/bounds are unmatched. The published source also includes map-region and road import, the data-driven economy host, Lua economy rules, threaded production, and the first rule-based autopilot.

The published `--map-lod` selector/fallback change in `a28ac80` updates `src/app/main.cpp`, `src/app/ContentPreview.h`, `src/app/ContentPreview.cpp`, `src/app/OpenRepublicApplication.h`, and `src/app/OpenRepublicApplication.cpp`. Fallback is counted only after a lower-level preview successfully loads. Windows Release client and AgentHost builds succeeded; fresh Release CTest passed 29/29 in the client tree and 15/15 in the headless tree. On the local read-only WRSR build 23935965 sample, one placed type with all three declared model levels was captured at requested LOD0, LOD1, and LOD2: each request logged one loaded model, zero failed or unresolved models, and zero fallback types; the three 1260x748 captures passed nonzero depth coverage and show visible geometry simplification. A second placement without LOD2 was requested at LOD2 and successfully used LOD1, logging one fallback and zero failed loads. A repeated LOD0 invocation once exited 1 without writing a capture; a later isolated retry exited 0 and produced the same image hash as the first successful LOD0 output. The transient failure is unexplained, and this app path has no direct unit test. Source publication through `da8f460` is complete; D-005 still gates distribution of original content or a binary release using it. Exact source identities, paths, hashes, and captures remain in private research storage.

Next economy target: expose the app-owned world through a versioned trusted-local controller endpoint so external controllers and the human UI share one `World`, session, revision, and command queue. Keep writes bounded and serialized through `EconomyApi`; define endpoint lifetime/recovery and avoid silently falling back to a second world. Then run a synthetic/open fixture interaction and record a graphical acceptance receipt if the UI automation path is available. Separately, obtain a saved same-model ModelViewer reference or recover another trusted reference artifact, then align bounds/framing to compare the diagonal school view. Repeat the private LOD capture once to check whether the single LOD0 process failure recurs. Avoid expanding NMF field research without a specific fidelity question.

The chronological implementation notes below preserve the publication and verification state at the time of each entry. Later status in this header and D-005 supersedes historical statements that the NMF source was unpublished.

## Historical M0 work completed (21 October 2026)

- Traced the previous repeat-placement failure to storage placement remaining active after its first accepted queue request. Successful single-storage queueing now exits placement mode; queue failure leaves the mode available for retry.
- Traced swallowed world clicks to Wicked Engine `GUI::HasFocus()` treating every visible widget hitbox as focus. The large, passive OpenRepublic status label now allows world input to pass through its hitbox while the other GUI focus checks remain in place.
- Reconciled placed-storage render entities by their ordered stable `OR-<id>` identities, not only by count. Selection is restored by stable ID across a rebuild if that entity still exists.
- Applied the same stable-ID selection preservation to dynamically placed Farms and Mills when their scene entities are rebuilt after the loaded facility set changes. A selected facility is cleared if its ID no longer exists.
- Retained the “Next storage” selector as a fallback selection path. It is useful for deterministic entity selection, but it does not substitute for spatial picking acceptance.
- Added simulation regression assertions that accepted Farm, Mill, and Storage IDs resolve to the intended entities and that an accepted plan keeps those exact IDs after save/load.

## Historical validation evidence for M0 UI fixes

- **VERIFIED — Release build:** OpenRepublic client and its simulation, terrain-reader, GLB-container, and Lua-rules test targets built with the VS 2022/MSVC toolchain.
- **VERIFIED — client CTest:** 4/4 tests passed.
- **VERIFIED — headless CTest:** 3/3 tests passed.
- **VERIFIED — diff check:** `git diff --check` reported no whitespace errors. Git emitted a CRLF-to-LF normalization warning for `tests/sim/simulation_tests.cpp`.
- **OBSERVED — previous runtime only:** the 21 October project-plan trace opened the OpenRepublic client, paused it, entered/canceled storage placement, selected built-in storage `OR-1003`, read its inventory, and changed the draft truck-plan storage endpoint. That run did not place a storage, run a changed route, deliver cargo through the UI, or save.
- **UNKNOWN — current UI fixes:** there is no post-fix runtime confirmation of passive-label click-through, automatic placement-mode exit, spatial picking of a placed storage, facility-selection restoration after load, or the complete UI logistics sequence.
- The client was launched for this checkpoint, but the available CUA runtime exposed no native-app API and reported no native app windows. REA native UI capture reported `target_unavailable` (“No app is open”) for the running Windows process. Thus the build is runnable but this session could not interact with its window through the available UI automation interface.
- The existing `build/openrepublic-vs2022/OpenRepublic-demo.save` was observed at 205 bytes with last-write time 2026-10-21 11:56:52 AM before the runtime attempt. No save/load button was used in this checkpoint. No WRSR executable, save, or Steam installation file was changed, and no Rival Republics files were modified.

## M0 runtime items open at the earlier 21 October checkpoint

1. Queue exactly one storage while paused, step one tick, and verify acceptance plus automatic return to selection mode.
2. Click the new storage at its projected screen location and verify the selected `OR-<id>` and its inventory. Also exercise selection in the former status-label hitbox.
3. Build a connecting road, assign the intended Farm, Mill, and Storage endpoints, queue the route, and observe truck movement and cargo/inventory changes.
4. Fill the destination or otherwise exercise the alternate-route/full-storage case through the UI.
5. Save to an isolated OpenRepublic test file, reload it, and verify entity IDs, selection resolution, route state, and inventory without overwriting the demo save.
6. Record exact results, then review and publish only the scoped OpenRepublic code and documentation changes.

Simulation-level route, full-storage, accounting, and save/reload behavior is already covered by OpenRepublic tests; it is not a substitute for these application-level observations.

## Repository state at checkpoint

Only these tracked project files were modified for the current M0 increment: `src/app/OpenRepublicApplication.cpp`, `src/app/OpenRepublicApplication.h`, `tests/sim/simulation_tests.cpp`, and this checkpoint. The app and test source changes have not been committed or pushed. No proprietary game payload or private Rival Republics artifact was added.

## Restart order

1. Recheck the exact repository status and preserve unrelated user edits.
2. Establish a working native-window automation binding, then execute the M0 runtime items above with an isolated save path.
3. If native UI automation remains unavailable, keep each UI item explicitly unverified and continue independent M1 work without promoting compatibility claims.
4. After the M0 evidence and diff review, commit/push the scoped work, then proceed to M1: one bounded original building/material path, with assets and any model-derived outputs remaining private.

## Chronological review update

**2026-10-21 13:54:59 EDT (17:54:59 UTC)**

- An independent read-only review of the final M0 code delta found no actionable defect in the status-label click-through or dynamic-facility selection restoration.
- A live OpenRepublic process was confirmed responsive. The CUA provider exposed no `computer` native-app API or native app inventory; REA's native UI capture returned `target_unavailable` for this Windows application. No clicks, keypresses, or save actions were performed through another mechanism.
- A fresh fetch confirmed `origin/main` still points to `4b686ac`; the scoped local diff remains limited to the three application/test files and this checkpoint.

**2026-10-21 13:55:24 EDT (17:55:24 UTC)**

- The scoped M0 implementation, simulation regression, and checkpoint were committed as `0775ce8` (`Fix logistics selection and document checkpoint`) and pushed to `origin/main`.
- This publication does not close M0 runtime acceptance: all six UI items above remain unverified in this session. Continue the project without describing the runtime gate as passed.
- Because the current native UI automation surface cannot control or capture the Windows window, continue evidence-bounded M1 work in parallel rather than claiming unavailable runtime evidence.

## Chronological continuation — bounded GLB semantic profile

**2026-10-21 14:17:40 EDT (18:17:40 UTC)**

- Added an OpenRepublic-owned semantic preflight for a narrow embedded-resource glTF 2.0 profile. It validates the same in-memory bytes read by the bounded GLB container reader, checks the supplied container metadata, and rejects external URIs, extensions, sparse accessors, unsupported semantics/layouts, unsupported texture coordinate sets, morph data, animation, skins, cameras, unsupported sampler values, invalid references, invalid binary ranges, non-finite geometry/transforms, and invalid node graphs.
- The developer probe writes those validated bytes to a unique temporary snapshot before calling the pinned Wicked Editor importer. Import occurs in a staging scene; every imported mesh must contain finite indexed triangle data before the scene merges into the preview.
- Added synthetic semantic fixtures for valid embedded geometry/material/image references and failure cases including `texCoord: 1`, hierarchy cycles/multiple parents, range errors, unsupported features, invalid values, and transactional result preservation.
- **VERIFIED — Release builds:** client and headless CMake builds both succeeded on the configured VS 2022 toolchain.
- **VERIFIED — client CTest:** 5/5 tests passed, including `OpenRepublicGlbSemanticTests`.
- **VERIFIED — headless CTest:** 4/4 tests passed, including `OpenRepublicGlbSemanticTests`.
- **UNKNOWN — runtime/content fidelity:** no new GLB visual or material comparison was made. The Editor importer and image decoders are not sandboxed; the profile is for trusted local inputs. No claim of general glTF support, direct NMF support, WRSR asset compatibility, or standalone content rights is made.
- No WRSR executable was launched, no save or Steam file was modified, and no proprietary game asset or Rival Republics artifact was added to the repository. The changes remain uncommitted pending independent review and final diff inspection.

**2026-10-21 14:20:08 EDT (18:20:08 UTC) — independent review follow-up**

- A read-only reviewer found two actionable GLB-profile gaps: repeated/aliased accessors could trigger excessive repeated scalar reads, and finite but non-unit rotation quaternions were accepted.
- Added a 16,777,216 aggregate scalar-read budget across finite FLOAT accessor values and indices. FLOAT accessor data is checked once per accessor; index data is checked once per index accessor, caching its maximum value and comparing it with each consuming mesh's POSITION count. Removed the per-primitive POSITION rescan.
- Node rotation quaternions now require squared norm within `1e-3` of one. Added fixtures for zero, non-unit, and valid normalized rotations plus a 65,536-accessor alias fixture that exceeds the aggregate scan budget.
- **VERIFIED — client/headless Release builds:** both succeeded.
- **VERIFIED — client CTest:** 5/5 passed. **Headless CTest:** 4/4 passed. The semantic suite passed with the new resource-cap and quaternion cases.
- Visual/material fidelity, importer sandboxing, WRSR NMF parsing, and local original-content rights remain unverified. The reviewer raised no further findings about the exact-snapshot path or staged-mesh checks. No WRSR runtime/save/Steam writes or proprietary artifacts were used.

**2026-10-21 14:21:31 EDT (18:21:31 UTC) — shared-index regression**

- The review follow-up confirmed the aggregate scan cap and quaternion checks, then identified a missing regression for an index accessor reused across primitives with different POSITION counts.
- Added a synthetic two-mesh fixture that shares one index accessor (`[0, 1, 3]`): the first mesh has four positions, while the second has three and must reject the cached maximum index.
- Rebuilt the semantic test targets in client and headless Release configurations. The focused semantic CTest passed 1/1 in each configuration; final full CTest passed client 5/5 and headless 4/4.
- All results remain source/build/test evidence only. No new graphical import comparison or WRSR runtime test was performed; this increment is still uncommitted pending final review and diff check.

**2026-10-21 14:22:02 EDT (18:22:02 UTC) — publication**

- Final staged diff review and `git diff --cached --check` passed. Committed as `304a9e2` (`Add bounded embedded GLB semantic validation`) and pushed to `origin/main`.
- The repository changes cover a narrow trusted-local GLB semantic profile, exact-snapshot import, synthetic semantic tests, and updated evidence/checkpoint documentation. They do not close the outstanding M0 UI runtime gate, visual/material fidelity checks, importer sandboxing, direct NMF parsing, or rights review.

**2026-10-21 14:25:18 EDT (18:25:18 UTC) — bounded local scratch GLB runtime probe**

- The current Release client passed one pre-existing private model-derived scratch GLB through the embedded GLB semantic preflight and pinned Wicked importer. The application log reported 3 meshes, 2 materials, and 4 transforms; the client process exited with code 0.
- This is **VERIFIED structural import evidence** for that one local input. It is not a visual comparison, image decode/material-fidelity result, direct NMF support, broad WRSR compatibility, or asset-distribution permission. No screenshot comparison was performed.
- The input and its provenance remain private; no model, texture, save, game binary, private path, or asset-derived payload was added to the repository. WRSR was not launched, and no Steam installation or save was modified.
- A separate post-run cleanup helper emitted an unavailable `System.Windows.Forms.SendKeys` type error after the app had exited; this did not alter the app's exit code or the recorded import result.

# Chronological continuation — bounded MTL syntax reader

**2026-10-21 14:34:02 EDT (18:34:02 UTC)**

- Read-only inspection of three private local MTL text samples (two building, one terrain) found flat sibling `$SUBMATERIAL` blocks and one terminal `$END`. The observed directives had these shapes: `$SUBMATERIAL` with one bare token; `$TEXTURE` with one numeric token and one string token; each of `$DIFFUSECOLOR`, `$SPECULARCOLOR`, and `$AMBIENTCOLOR` with four numeric tokens; `$SPECULARPOWER` with one numeric token; `$END` with no arguments. Texture directives repeated, one section omitted `$SPECULARPOWER`, and no unrecognized directive appeared in this bounded sample. A follow-up checked syntax shapes only; no sample contents, paths, names, or fingerprints were copied into the repository.
- Added `ParseMtlSyntax`, an OpenRepublic-owned, side-effect-free parser for that narrow observed profile. It keeps raw values and path-like tokens, preserves unknown directives inside sections, allows repeated directives and omitted `$SPECULARPOWER`, enforces a terminal `$END`, and applies input/line/token/section/directive bounds. It does not resolve paths, load images, assign texture-slot meaning, or claim general MTL/WRSR support.
- Added synthetic fixtures for sibling sections, CRLF and blank lines, repeated texture entries, unknown `$TEXTURE_MTL` preservation, omitted specular power, malformed values/structure, unsupported comments/BOM, NULs, and resource caps.
- **VERIFIED — client Release application target:** the full OpenRepublic build succeeded; CTest passed 6/6.
- **VERIFIED — headless Release:** build succeeded; CTest passed 5/5.
- **UNKNOWN — general MTL grammar and material fidelity:** the three-sample audit does not establish duplicate/order semantics, valid color/power ranges, quoted paths, comment/BOM behavior, alternate roots, or whether other variants use additional directives. Its exact game build and per-file identities were not recorded, so this grammar must be rechecked with private provenance before being treated as version-scoped. No WRSR runtime, save, Steam file, or asset write occurred. No proprietary data was added to Git.

## Chronological continuation — synthetic UI routing and explicit terrain transform

**2026-10-21 14:54:52 EDT (18:54:52 UTC)**

- A bounded no-save OpenRepublic UI sequence placed storage `OR-3001`; accepted road commands 3 and 4 as edges 205 and 206 to connect its cell; selected it through the built-in “Next storage” control; assigned it as the truck-plan destination; and accepted truck-plan command 5 at tick 174.
- **OBSERVED — synthetic OpenRepublic UI only:** by paused tick 176, the truck was empty and `OR-3001` held 2 flour while the built-in warehouse remained full at 5/5. This is bounded evidence that the synthetic route can recover from a full initial destination and deliver to the selected alternate storage. It does not show a fresh farm-to-mill production cycle or WRSR parity.
- **UNKNOWN — remaining UI acceptance:** clicking the guessed screen position of `OR-3001` did not select it; spatial picking of newly placed storage remains unresolved. A UI save/reload after route changes was not attempted, and the demo save was not touched. Later window interaction reported user input, so further app input stopped. Do not promote this partial route observation to complete M0 acceptance.
- Implemented `BuildTerrainPreviewGrid` as a bounded content helper and wired `--terrain-transform originX originZ baseHeight sizeX sizeZ heightScale rowOrder` into the terrain preview. When explicitly supplied with `--terrain-probe`, the values drive bilinear height sampling and bypass adjacent metadata; the preview mesh remains centered for display. Synthetic fixtures cover interpolation, both row orders, limits, invalid input, and transactional failure behavior.
- **VERIFIED — source/build/test evidence:** an independent read-only review found no actionable issue in CLI parsing, helper bounds, or preview wiring. The separate VS 2022 Release client build linked successfully and client CTest passed 6/6; the headless Release build succeeded and headless CTest passed 5/5. The initial in-place client link was blocked by an executable lock, so the successful client link used a separate build directory. `git diff --check` passed before this checkpoint update.
- **UNKNOWN — graphical terrain path:** the new explicit-transform command-line path has not been launched or visually exercised. No real map transform, row orientation, map placement, or WRSR terrain compatibility is verified.
- Current uncommitted source/documentation scope includes the terrain-preview helper and its app integration, focused synthetic tests, and updates to the README, project plan, and reverse-engineering ledger. Build output remains in ignored build directories. No WRSR binary, save, Steam installation file, or proprietary asset was added to Git or modified.
- Resume work from this state: retain the partial M0 UI result and its open gates; next use a synthetic DDS to exercise the explicit CLI/render path when a safe app session is available, while continuing bounded M1 reader work. Review the complete scoped diff and chronological notes before publishing.

**2026-10-21 14:55:33 EDT (18:55:33 UTC) — publication**

- Final staged diff check passed. The scoped terrain-transform preview implementation, synthetic tests, and supporting status/evidence documentation were committed as `96d0e14` (`Add explicit terrain transform preview path`) and pushed to `origin/main`.
- This publication does not close the remaining UI spatial-picking or route save/reload gates, and does not establish graphical transform correctness, WRSR terrain placement, or compatibility. No proprietary game files or private research payloads were published.

## Chronological continuation — typed Agent API v0

**2026-10-21 15:14 EDT (19:14 UTC)**

- Added an in-process typed C++ `AgentApi` facade over the existing authoritative `Simulation`. Capability discovery reports permission-gated actions. Queries expose stable simulation IDs, entity inventories, route state, revision-checked entity pages, and a bounded process-local event journal. Commands use revision checks and the existing simulation validation path, with distinct queued, applied, and rejected outcomes and concrete execution diagnostics.
- Added hard bounds of 10,000 ticks per step call, 64 retained command results, 32 entities per page, 128 retained events, and 32 events per read. Event cursors report when their history fell behind; batched stepping records first-tick command outcomes before aggregating the remaining ticks. Save v7 does not serialize the API revision, diagnostics, or event journal.
- Independent review led to fixes for unbounded stepping, signed-overflow road validation, result-retention growth, denied-query leakage, historical-result acknowledgment, and event order/timestamps. The final review found no actionable issue; synthetic tests cover these cases and the API paging/journal contract.
- **VERIFIED — Release source/build/test evidence:** the client `OpenRepublic` and `OpenRepublicAgentApiTests` targets built. Full client CTest passed 7/7; full headless CTest passed 6/6. The synthetic API controller test completes a Farm→Mill→Storage delivery. This does not establish runtime UI acceptance, WRSR behavior, a serialized agent protocol, or IPC/MCP/C# support.
- No WRSR game was launched, and no Steam installation, save, original asset, or Rival Republics file was modified or added. The source and documentation changes remain under final diff review before publication.

**2026-10-21 15:18 EDT (19:18 UTC) — publication checkpoint**

- The typed Agent API v0 source, tests, and documentation were published to `origin/main` as `2bbc830` (`Add versioned simulation agent API v0`). The checkout was clean after push.
- M1 remains incomplete: the API is still in-process C++ only. Next, add a bounded local stdio protocol and an external scripted controller so an independent process can inspect and operate the synthetic Farm→Mill→Storage simulation without GUI automation.

## Chronological continuation — external Agent API host and bounded MTL dependency resolver

**2026-10-21 15:29 EDT (19:29 UTC)**

- Added an optional local JSONL stdio process host over Agent API v0 and a Python stdlib controller. The external process can inspect the synthetic world, construct roads and storage, assign a transport plan, inspect command results/events, and verify flour delivery by tick 17. The host is local-only; observer/controller selects a permission profile and is not authentication. This does not establish WRSR control or a network service.
- The host rejects malformed, oversized, deeply nested, over-node-limit, duplicate-key, wrong-version, and unknown-field requests. `uint64_t` wire values are decimal strings. An unterminated line over 64 KiB receives one `request_too_large` response and exits with status 5. The process-level controller test has a bounded CTest timeout and cleans up child processes on failures.
- Added an MTL dependency manifest helper for recognized `$TEXTURE` directives against explicit roots. It preserves order and opaque index/path tokens, rejects unsafe path forms, records ambiguous/missing/resolved statuses, and never opens or modifies texture files. A 262,144 aggregate candidate-inspection cap is preflighted across safe references and unique valid canonical roots before per-texture path inspection; over-budget failure leaves caller output unchanged.
- Independent host and resolver reviews found and drove fixes for a hanging unterminated oversized JSONL input and excessive candidate path probes. Final host review and aggregate-cap follow-up found no remaining actionable issues.
- **VERIFIED — client Release build and CTest:** build succeeded; 10/10 tests passed, including the external controller and MTL dependency tests.
- **VERIFIED — headless Release build and CTest:** build succeeded; 9/9 tests passed, including the external controller and MTL dependency tests.
- **VERIFIED — source/diff hygiene:** `git diff --check` passed after the aggregate-budget implementation.
- **UNKNOWN — WRSR behavior:** no WRSR process was launched and no original asset, save, Steam file, or Rival Republics data was modified or added. MTL sample provenance remains incomplete; resolver path-base rules, `$TEXTURE_MTL`, texture-slot meanings, material fidelity, and standalone-use rights remain open.
- At the time this checkpoint was first written, the increment was staged pending final scoped diff review and publication. It advances the external-agent foundation and a metadata-only material dependency stage; it does not complete the playable-slice acceptance criteria or the remaining M0 UI gates.
- Resume by reviewing the exact changed file set and docs, then commit/push only the reviewed OpenRepublic paths. Keep the goal active and continue to the next highest-value playable-slice gap after publication.

**2026-10-21 15:30:30 EDT (19:30:30 UTC) — publication receipt**

- Final review completed; the local JSONL host, external controller, and MTL dependency resolver were committed as `76a5bd1` (`Add local agent host and MTL dependency resolver`) and pushed to `origin/main`. The previous checkpoint status above was superseded by this publication.
- Client Release CTest passed 10/10 and headless Release CTest passed 9/9. The checkout was clean after publication. M1 remains open for richer verified observations and subsequent integrated-world work; no WRSR behavior or proprietary asset use was verified.

## Chronological continuation — production telemetry through Agent API v0

**2026-10-21 15:37:16 EDT (19:37:16 UTC)**

- Extended the typed `WorldView` with the simulation snapshot's active `SimulationRules` and cumulative `deliveredFlourTotal`. `QueryWorld` fills both from the same authoritative snapshot. A denied query returns zeroed telemetry along with its zero/default world view.
- The JSONL `world` result now exposes `production.rules` as bounded integer fields and `production.deliveredFlourTotal` as a decimal string for its `uint64_t` type. The external Python controller validates the schema, confirms the count increases after its route executes, and reconciles it against flour remaining in storage. Agent API v0 remains read-only for rules.
- Added typed API and wire-protocol checks for initial defaults, custom active rules, same-snapshot counters, the decimal-string contract, successful cumulative delivery, and denied-query zeroing.
- **VERIFIED — client Release build and CTest:** the application, API tests, protocol tests, and local host built; full client CTest passed 10/10.
- **VERIFIED — headless Release build and CTest:** the simulation, API tests, protocol tests, and local host built; full headless CTest passed 9/9. The first headless CTest attempt used an older host executable and failed because it lacked the new field; rebuilding the headless host and rerunning the complete suite passed.
- **VERIFIED — diff hygiene:** `git diff --check` passed. No WRSR game or proprietary files were used or modified; no WRSR formulas or compatibility behavior are claimed.
- This increment is scoped to OpenRepublic source, synthetic tests, and public documentation. At this checkpoint it was ready for final diff review and publication. M1 and the playable-alpha gates remain open.

**2026-10-21 15:37:51 EDT (19:37:51 UTC) — publication receipt**

- The reviewed production-telemetry increment was committed as `3ada697` (`Expose production telemetry in agent world view`) and pushed to `origin/main`. The source, tests, and all 12 scoped files are published; no WRSR asset or behavior claim is included.

## Chronological continuation — bounded NMF header profile

**2026-10-21 15:47 EDT (19:47 UTC)**

- Private read-only REA/Ghidra review of the installed ModelViewer, scoped to WRSR build `23935965`, observed a 32-bit count at byte offset `0x08`, fixed-width `0x40`-byte records beginning at `0x14`, and a separate count at `0x0C` used with a `0xF0`-stride record array. The in-memory loader has no explicit input-length parameter. This is build-specific static evidence, not a safe parser contract.
- A private one-sample byte check found NUL-terminated printable-ASCII identifiers in the 64-byte records. Their set matched the sibling MTL `$SUBMATERIAL` identifier set, with order differing. Exact paths, sample identities, hashes, and REA/Ghidra outputs remain outside Git.
- Added an OpenRepublic-owned header-profile reader over caller-supplied bytes. It checks the 20-byte prefix, caps both count fields at 4096, bounds the fixed-width identifier table against the buffer, requires a NUL terminator and printable ASCII, preserves order, and leaves output unchanged on failure. It reports but does not parse object records; geometry, animation, material/texture semantics, and runtime model import remain out of scope.
- Independent review found that the first version rejected ASCII spaces despite describing its accepted range as printable ASCII. The profile now accepts the full printable ASCII range, and the synthetic valid fixture includes a space.
- **VERIFIED — source/build/test evidence:** the Release client and headless NMF test targets built successfully. Full client CTest passed 11/11 and full headless CTest passed 10/10. Running both suites simultaneously initially exposed a shared temporary-file collision in the existing terrain-reader client test; rerunning the suites sequentially passed both. `git diff --check` passed.
- **OBSERVED — local WRSR sample:** one private model's identifier set matched its sibling MTL identifier set. This single sample and the build-specific static loader review do not establish general NMF support, geometry layout, WRSR visual fidelity, or standalone-use rights.
- No WRSR game was launched; no original asset, save, Steam installation file, or Rival Republics data was modified or added. The REA evidence bundle remains in the private research area. The code and public evidence documentation are ready for final scoped diff review and publication; broader NMF support and M1/playable-alpha acceptance remain open.

**2026-10-21 15:48 EDT (19:48 UTC) — publication receipt**

- The reviewed header-profile reader, synthetic tests, and evidence-bounded documentation were committed as `2ec56a9` (`Add bounded NMF header profile reader`) and pushed to `origin/main`. Client Release CTest passed 11/11 and headless Release CTest passed 10/10. No proprietary file or private reverse-engineering output was added to the repository; broader NMF support and the playable-slice gates remain open.

## Chronological continuation — shared desktop simulation and typed Agent API

**2026-10-21 15:57 EDT (19:57 UTC)**

- The Wicked Engine app now constructs an `AgentApi` with a non-owning reference to its app-owned `sim::Simulation`. The API can also continue to own a synthetic instance for standalone hosts/tests; its move operations are disabled to protect the internal simulation reference.
- UI snapshot reads, fixed-tick steps, facility/storage/road construction, truck-plan assignment, road-state changes, Lua-rule application, and simulation save/load now go through the typed Agent API. The app consumes typed command events/results for UI feedback. The JSONL host continues to own its own separate synthetic world and does not expose in-process configuration/save/load operations.
- Added a permission-gated full-snapshot query for trusted in-process UI use, plus revision-checked road/rule configuration and save/load operations. Loading clears API-side command tracking from the replaced timeline and emits a world-loaded event. Denied full-snapshot reads return zeroed rule and endpoint fields.
- Added synthetic tests for app-owned simulation injection, shared state after API commands, stale-revision rejection, permission-gated configuration, save/load replacing command state, and denied snapshot zeroing. A follow-up review found that loading an older save can reuse a simulation-internal command ID; API command handles now remain monotonic across world loads, and tests verify the stale handle stays unknown while the new result is independently queryable.
- **VERIFIED — source/build/test evidence:** Release client build succeeded; full client CTest passed 11/11. Release headless build succeeded; full headless CTest passed 10/10. `git diff --check` passed. Independent read-only review is pending.
- **UNKNOWN — graphical acceptance:** the new app path has not been exercised in a window. These tests do not prove UI clicks, picking, rendering, or user-visible save/reload. No WRSR process was launched, and no WRSR asset, save, Steam installation file, or Rival Republics data was modified or added.
- The scoped source, tests, and docs are pending final review and publication. The JSONL host remains a separate simulation owner; persistent world identities across multiple clients, native-window acceptance, and broader WRSR content support remain open.

### Follow-up review — command handles across save/load

- An independent read-only review identified that the desktop event-drain loop could stop after a full event page and that loading an older save could reuse a simulation-internal command ID still held by a controller. The event loop now compares the returned cursor with its value before reading that page. API results/events now use monotonic opaque handles mapped to simulation IDs; `LoadWorld()` clears the replaced mapping but does not reset the API handle counter.
- Added regression coverage for post-load handle uniqueness and adopted results whose simulation IDs differ from their API handles. Query-denied APIs still leave untracked transferred results in simulation storage.
- **VERIFIED — client Release build and CTest:** build succeeded; full client CTest passed **11/11**.
- **VERIFIED — headless Release build and CTest:** build succeeded; full headless CTest passed **10/10**.
- **VERIFIED — diff hygiene:** `git diff --check` passed after the follow-up edits. **UNKNOWN — graphical acceptance:** this did not exercise the desktop UI in a window or validate rendered behavior. No WRSR game, save, Steam installation file, original asset, or Rival Republics data was used or modified. The independent reviewer has not re-reviewed the corrected diff.

## Follow-up — pending truck-plan command identity

**2026-10-21 16:07 EDT (20:07 UTC)**

- Independent review found that pending-plan UI text used simulation-internal command IDs, while submission notices used opaque Agent API handles. These IDs can differ after save/load, and a plan restored from a save has no handle from the replaced API timeline.
- Added `LookupApiCommandHandleForSimulationCommand()` for trusted in-process presentation. The desktop UI now labels tracked plans with their API command handle and untracked restored queue entries with their simulation command ID and the explicit `no API handle` qualifier. Updated the Agent API guide accordingly.
- **VERIFIED — Release source/build/test:** the client build succeeded with CTest 11/11; the headless build succeeded with CTest 10/10. `git diff --check` passed. **UNKNOWN — graphical acceptance:** the updated text has not been exercised in the desktop app. No WRSR game, save, Steam installation file, original asset, or Rival Republics data was used or modified.

## Follow-up — terrain orientations and bounded preview mapping

**2026-10-21 16:17 EDT (20:17 UTC)**

- The app now selects preview width and height from the source raster axes after applying the caller-selected orientation. This preserves detail when a rectangular raster is transposed.
- Added an explicit axis-swap classifier and synthetic checks across all eight orientations. Explicit-transform preview heights now use the selected horizontal display scale multiplied by 5, so downscaling very large extents preserves the advertised 5x exaggeration. Metadata previews describe their fixed vertical scale without claiming a constant exaggeration ratio.
- Strengthened the pending-plan API regression: it loads an initial world after a first submission, then proves the next simulation command ID differs from its opaque API handle. Saving and reloading that pending plan confirms the replaced timeline has no stale API handle.
- **VERIFIED — Release source/build/test:** client build succeeded and CTest passed **11/11**; headless build succeeded and CTest passed **10/10**. `git diff --check` passed.
- **UNKNOWN — graphical acceptance and WRSR compatibility:** the explicit-transform app path has not been visually exercised, and no map-matched WRSR transform/orientation is established. Synthetic fixtures verify OpenRepublic code behavior only. No WRSR process, save, Steam file, original asset, or Rival Republics data was used or modified.

## Chronological continuation — role-aware JSONL method discovery

**2026-10-21 16:23 EDT (20:23 UTC)**

- Added a `methods` table to JSONL `hello` and `capabilities`, listing each implemented method and the roles allowed to call it. The existing `actions` list remains a separate inventory of typed gameplay actions.
- Observer/controller role assertions cover command-result polling, controller-only acknowledgement, and controller-only gameplay mutations.
- **VERIFIED — Release source/build/test:** `OpenRepublicAgentHost` and `OpenRepublicAgentProtocolTests` built successfully; targeted protocol and external-controller CTest passed 2/2. `git diff --check` passed.
- **KNOWN API gap:** request `id` remains correlation only. Replaying the same mutation after a lost response can submit duplicate work; bounded idempotency is not implemented and must be addressed before using the protocol for remote or multi-client commands.
- No WRSR process, save, Steam installation file, original asset, or private Rival Republics data was accessed or modified for this protocol change. No compatibility behavior is claimed.

## Publication receipt — shared desktop simulation and terrain preview transforms

**2026-10-21 16:36 EDT (20:36 UTC)**

- The reviewed shared-simulation Agent API integration, opaque command-handle mapping, and eight-orientation terrain preview transform were committed as `569cdc3` (`Integrate desktop Agent API and terrain transforms`) and pushed to `origin/main`.
- **VERIFIED — Windows Release source/build/test:** the client rebuilt after the final source correction and full CTest passed **11/11**. The headless Release build succeeded and full CTest passed **10/10**; it does not compile the desktop-only terrain-summary code changed in the final correction. `git diff --check` passed before publication. Independent read-only API/app and terrain/render reviews found no actionable issues.
- **UNKNOWN — graphical acceptance:** the injected Agent API app path and explicit terrain transform preview were not launched or visually exercised in this turn. No WRSR game, save, Steam installation file, or original asset was accessed or modified; no compatibility or standalone-use-rights claim is made.
- The next content gate is a bounded static NMF geometry reader for one build-scoped profile, connected to the existing normalized-mesh render path. Object/geometry layout remains unknown and must be established from private read-only evidence before coding that format.

## Continuation — bounded static NMF geometry profile and preview

**2026-10-21 16:57 EDT (20:57 UTC)**

- Added an OpenRepublic-owned bounded reader for one build-scoped static type-0 NMF triangle profile. The initial parser version treated the node `+4` field as an object span; follow-up loader review found no support for using it as a general type-0 bound, and the current parser no longer does so. It validates count-driven indices and finite positions, checks recognized auxiliary streams and subset-index lists, and rejects unsupported object types and extra vertex sets. `--nmf-probe <file.nmf>` routes extracted positions/indices through the Wicked Engine mesh/material path and normalizes the mesh into a 10-unit local debug preview. Source attribute meaning, partition/material meaning, textures, source placement, and game units are not recovered.
- Static private review was scoped to the installed ModelViewer for build `23935965`. A private sample's count-driven geometry stream walk reached the declared file end; detailed sample identity and artifacts remain outside the public repository. This is one build and one sample, not a general NMF contract.
- Added distinct synthetic sentinel coverage for accepted 4-byte and 8-byte vertex streams, UV1–UV3, and both per-triangle arrays. Following-submesh checks verify cursor placement; truncation cases preserve caller output.
- The Release client rebuilt and CTest passed **12/12**; the headless Release build succeeded and CTest passed **11/11**. Both suites were run sequentially. The renderer now explicitly rejects simultaneous NMF and terrain preview paths with a visible notice.
- **UNKNOWN — NMF sample runtime and visual acceptance:** OpenRepublic was launched with a private sample path, but computer-use capture returned an unrelated Chrome page while the desktop had other active windows. No sample-specific success log or target-matched image was captured. The exact OpenRepublic process started for this check was closed cleanly. No original WRSR process was launched; no source asset or save was changed or added to Git.
- **PUBLICATION GATE — decision D-005:** the mesh reader and app path remain local and unpublished pending rights-holder clarification or appropriate legal review before release of an asset-reading runtime. The earlier shared-simulation Agent API integration remains published at `caeba3a`.
- Next: obtain a target-matched OpenRepublic capture without disturbing active desktop use, verify whether the local sample reaches the render path, then compare geometry/orientation and assess a separately supported material path. Broader variants and standalone-use rights remain open.

## Follow-up — single-LOD NMF integration and render upload

**2026-10-21 17:19 EDT (21:19 UTC)**

- Revised the local geometry reader to preserve type-0 object nodes and their raw metadata with one nested LOD each. The inspected build selects one LOD per mesh draw, so the preview rejects multiple LODs rather than combining them. The parser accepts only the one-sample root/no-child and identity-matrix profile; those are conservative sample gates, not general hierarchy or transform semantics.
- The unproven node `+4` field remains metadata and is not used to bound type-0 payloads. The parser advances through count/flag-driven streams, checks the declared file byte count, and requires exact sequential end-of-input. LOD fields `+0x10`/`+0x18` and vertex flag `0x8` retain neutral labels where the meaning is only inferred. Build `23935965` has no byte-consumption branch for flag `0x1` (meaning unknown); flag `0x40000` consumes two separate per-triangle arrays, 16 bytes then 24 bytes each, which the reader bounds-checks independently. The flagged vec3 attribute is preserved but not rendered; the pinned Wicked Engine mesh path generates smooth normals.
- A fresh read-only render review identified that `Scene::Entity_CreateMeshFromData` does not create GPU render data. Direct inspection of the pinned `wiScene.cpp` confirmed it computes normals and returns without uploading buffers. The NMF preview now calls `MeshComponent::CreateRenderData()` after its CPU-side triangle data passes validation, matching the existing terrain preview path.
- The reader profile also caps object nodes at 4096 and absolute position components at 1,000,000, 65,536 subset records, and 12 million aggregate subset indices; these implementation limits do not establish game-unit bounds. Expanded synthetic coverage for preserved raw node metadata, single-LOD enforcement, nonidentity matrices, hierarchy gates, all recognized auxiliary-stream branches, subset-index validation, invalid triangle/index counts, excessive object/vertex counts, truncation, nonfinite values, exact EOF, and unchanged output on failure. Independent parser and renderer reviews found no remaining actionable issue after the upload correction.
- **VERIFIED — Windows Release client:** build succeeded; full CTest passed **12/12**. **Headless Release:** build succeeded; full CTest passed **11/11**. The suites were run sequentially. `git diff --check` passed.
- **UNKNOWN — original-mesh visual acceptance:** no target-matched OpenRepublic image or visual geometry comparison was obtained. Parser/build/test results do not show that the private sample renders recognizably or preserves source shading, materials, placement, orientation, or game units. Standalone-use rights remain unknown under D-005; keep the asset-reading runtime local and unpublished pending rights-holder clarification or appropriate legal review.
- No WRSR process was launched; no installation file, save, original asset, private research payload, or Rival Republics data was modified or added to the repository. This local increment protects the engineering work but does not close the visual or release gates.

## Follow-up — NMF subset draw mapping and preview materials

**2026-10-21 17:31 EDT (21:31 UTC)**

- Extended the bounded type-0 parser to retain the fixed-width submaterial identifiers, each subset's observed draw offset/count and table index, and its auxiliary 16-bit index list. Draw ranges are bounded against the current LOD index array, table references against the retained table, and auxiliary values against the vertex count. The auxiliary list remains opaque.
- Wired the local preview to emit each retained range as a separate Wicked Engine mesh subset and assign deterministic diagnostic colors by explicitly referenced table index. Objects without subset records keep a separate neutral material; no table entry is inferred for them. This preview does not recover original MTL color/shading or texture bindings.
- A subset beginning at index 1 is retained by a synthetic fixture. The range-alignment recommendation from independent review was not adopted: build-scoped static review observed each subset submitted in a separate indexed triangle draw using its own start and `indexCount / 3`, so requiring alignment against the concatenated LOD list would impose an unsupported restriction. The independent reviewer agreed with this after follow-up. Review also caught and corrected the unpartitioned-object fallback that initially inferred submaterial 0.
- **VERIFIED — Windows Release source/build/test:** client build succeeded and full CTest passed **12/12**. Headless Release build succeeded and full CTest passed **11/11**. The suites were run sequentially. `git diff --check` passed.
- Updated the README, project plan, research summary, and evidence ledger to describe subset range/table mapping and diagnostic colors while keeping MTL values, texture semantics, auxiliary-index meaning, target-matched visual recognition, broad compatibility, and standalone-use rights explicitly unresolved.
- **UNKNOWN — original-mesh visual acceptance:** no target-matched OpenRepublic capture or recognizable-mesh comparison was obtained in this continuation. No WRSR game was launched; no original asset, save, Steam installation file, private research payload, or Rival Republics data was modified or added to Git.
- **PUBLICATION GATE — decision D-005:** keep these mesh-reader/render-path changes local pending rights-holder clarification or appropriate legal review before publishing an asset-reading runtime. The earlier shared Agent API integration remains published at `caeba3a`; this follow-up protects the local NMF work and is not pushed.

## Continuation — app-owned NMF capture and visual inspection

**2026-10-21 18:05 EDT (22:05 UTC)**

- Added and exercised an opt-in application-owned `--capture-preview` path. It hides the OpenRepublic window, skips synthetic scene geometry and simulation presentation, allows three renderer warm-up frames, centers two diagnostic camera views on the normalized NMF bounds, and saves the Wicked Engine 3D render target through `RenderPath3D::PostRender()` and `saveTextureToFile()`.
- A private research copy within the current bounded type-0 profile loaded through OpenRepublic and produced two valid 1260x748 PNGs: front and three-quarter views. The foreground window remained unchanged. The frames show the imported mesh, establishing a direct NMF upload/draw/readback path for this sample.
- **UNKNOWN — same-model visual acceptance:** the mesh appears dark and elongated and cannot be called a recognizable building from these views. The copy has no sibling MTL in its research directory; its original game build/hash is unknown. ModelViewer was not already open, so no same-model reference comparison was made and no source-axis, winding, reflection, normal, or material correction was guessed. Submesh/material boundaries, exact dimensions, and node placement remain unverified.
- The application-owned captures stay in private visualization storage. No original file was modified; no WRSR game or ModelViewer process was launched; no source asset, MTL, screenshot, or private analysis output entered Git.
- **VERIFIED — Windows Release client:** final build succeeded and CTest passed **14/14**. **Headless Release:** final build succeeded and CTest passed **13/13**. Both complete build/test sequences ran sequentially after the source changes.
- The parser, Wicked upload/capture route, and material compatibility received three independent LUNA reviews. The structural review's unsupported auxiliary-index range assumption was removed; source normals are applied before upload only for build-scoped flag `0x8` evidence; unmatched/invalid MTL data remains diagnostic; diffuse-to-PBR conversion stays explicitly approximate.
- **PUBLICATION GATE — decision D-005:** keep the asset-reading runtime local; do not push the three existing NMF commits or this new increment until rights-holder clarification or legal review. The previously published Agent API integration remains on `origin/main`.
- Next: establish a trusted same-model viewer reference when it can be captured without disturbing the owner's active desktop. Verify axes, winding, dimensions, connectivity, subset boundaries, and shading before changing the importer; then continue the evidence-backed original MTL/texture path.
## Continuation — NMF flag retention and paired render capture

**2026-10-21 18:30 EDT (22:30 UTC)**

- For installed build `23935965`, static loader review found no vertex-element or input-stream-consumption branch for flag `0x2`. The bounded parser now preserves that raw bit without assigning semantics; synthetic coverage confirms flags `0x1` and `0x2` do not advance the next-object cursor, while unsupported bit `0x4` remains rejected.
- A private NMF/MTL pair associated with that installed build now passes the Release `--nmf-probe` path and produces two nonblank, application-owned 1260x748 render-target captures from opposing diagnostic diagonal views. The inspected images show dark, stepped, low-poly building-like geometry. This establishes import/render/readback for this bounded sample, not recognition, fidelity, or same-camera comparison.
- The capture window now shows nonactivating, and capture begins after a bounded 60 complete-frame startup wait. The wait is a sequencing precaution; per-mesh PSO readiness is not logged or established. Inputs and images remain in private storage.
- **VERIFIED — Windows Release source/build/test:** client build succeeded and full CTest passed **14/14**; headless Release build succeeded and full CTest passed **13/13**. Build and test sequences ran sequentially after the source changes. `git diff --check` passed before documentation updates.
- **UNKNOWN — visual acceptance:** a matched-camera ModelViewer image is not available, and the current renders are not sufficient to claim the first recognizable WRSR object. Axes, winding, dimensions, connectivity, detailed material boundaries, texture behavior, and visual equivalence remain open.
- **PUBLICATION GATE — decision D-005:** keep the asset-reading runtime and this local mesh increment unpublished pending rights-holder clarification or appropriate legal review. The shared Agent API integration remains published at `caeba3a`. No WRSR game was launched or modified; no source asset, save, private payload, path, hash, or capture was added to Git.
- Next: obtain a same-model viewer image at matched axes and framing without disturbing active desktop use, then make any orientation, normal, material, or texture change only from the comparison evidence.

## Continuation — build-scoped NMF texture-coordinate adapter

**21 October 2026**

- Static loader review for build `23935965` labels flags `0x100`/`0x200` `_TC0`/`_TC1` and confirms eight-byte per-vertex consumption. OpenRepublic now retains the two payloads with checked bounds and finite components. Reading each as two floats and adapting to Wicked UV sets 0/1 remains **INFERRED**; texture-slot semantics remain **UNKNOWN**.
- Added a bounded, transactional content helper that combines per-object streams in vertex order and zero-fills missing-object vertices when a set is present elsewhere. This alignment behavior is an OpenRepublic preview policy, not a recovered source default. Synthetic tests cover either set independently, both sets, both object orders, output alignment, malformed values/lengths, and unchanged output on failure.
- The app assigns converted Wicked UV data before `MeshComponent::CreateRenderData()`. A fresh two-angle run through the rebuilt client exited successfully and wrote both application-owned 1260x748 render-target captures outside Git. Visual inspection again shows dark, elongated, stepped geometry. No target-matched ModelViewer image was available; recognition, orientation, winding, dimensions, connectivity, material boundaries, and visual fidelity remain **UNKNOWN**.
- Three actual LUNA reviews covered parser/submesh structure, Wicked rendering/capture, and material compatibility. They found no concrete blocker in the parser, subset bounds, mesh/material ownership, or upload order. They confirmed the float-pair/UV adapter remains inferred and that MTL texture-slot use remains unknown; the mixed-object aggregation test seam was added from their coverage recommendation.
- **VERIFIED — Windows Release client:** build succeeded and full CTest passed **14/14**. **Headless Release:** build succeeded and full CTest passed **13/13**. The build/test sequences ran sequentially.
- **PUBLICATION GATE — D-005:** the NMF asset-reading runtime and this increment remain local. The current local commits include the gated NMF work as a coherent integration; no safe separable publication subset was identified, so nothing was pushed. The shared Agent API integration remains published at `caeba3a`.
- No original game or ModelViewer process was launched, and no installation file or save was modified. The NMF input and captures remain private and outside Git. Next evidence step is a same-model reference view at matched axes/framing; only after geometry comparison should orientation, winding, normals, or materials be changed.

## Continuation — verified preview geometry handoff and bounded reference attempt

**21 October 2026**

- Extracted the existing CPU transform/flatten path into `BuildNmfPreviewGeometry` so the app's geometry handoff can be checked independently from Wicked Engine. The helper preserves the prior positive uniform normalization, X/Z recentering, minimum-Y offset, object/index order, index rebasing, subset offsets and per-subset triangle-tail behavior. It also records source extents, rendered/zero-area triangle counts, source-normal length summaries, and a per-vertex source-normal mask for mixed-object fallback.
- Synthetic tests now cover the transform and framing, multi-object index offsets, non-triangle-aligned subset ranges, mixed source-normal presence, structural diagnostics, malformed ranges/indices and transactional failure behavior. An independent read-only review found no concrete parity or correctness issue in the extraction.
- Rebuilt the Windows client Release target and ran its full CTest suite: **14/14 passed**. Then rebuilt the headless Release target and ran its full CTest suite: **13/13 passed**. The suites ran sequentially after the final source change.
- The rebuilt OpenRepublic probe on one installed building NMF/MTL pair exited with code 0 and emitted two application-owned render-target captures outside Git. They visibly contain mesh geometry, but it remains dark, stepped and not sufficiently recognizable for a fidelity claim. This confirms bounded import/render/readback for that sample only.
- Made the time-boxed read-only ModelViewer reference attempt. The preinstalled viewer opened, but the available UI capture bridge rejected native capture as unavailable on this host, and native CUA access is disabled. The empty viewer was closed. No same-model reference image or comparison was obtained; source axes, winding, dimensions, topology fidelity, normal convention, and visual equivalence remain **UNKNOWN**.
- **PUBLICATION GATE — D-005:** these local NMF runtime changes remain unpublished; no push was made. The shared Agent API baseline remains available on `origin/main`. No WRSR game was launched, no original files or saves were modified, and no source assets, private paths/fingerprints, captures, or analysis payloads were added to Git.
- Next: secure an independent same-model reference through an available capture workflow, then use matched views to determine whether the remaining visible defects arise from geometry, orientation/winding, normals, or shading before changing the renderer. Continue verification and integration; do not expand NMF field discovery absent a concrete dependency.
- Added a capture-only `--nmf-unlit-preview` diagnostic that requires an NMF input and capture output, uses a constant neutral base color with Wicked's global force-unlit path, and bypasses sibling MTL parsing/color adaptation, UV aggregation, and source-normal application. **VERIFIED:** the client Release build and full test suite pass with the option; the rebuilt app exits 0 and produces two additional application-owned captures. **OBSERVED:** paired neutral and regular captures have similar approximate silhouette coverage, while the neutral views are visibly brighter. These captures do not isolate the cause of the earlier dark PBR output because multiple inputs change together, and common fog/post-processing may remain. They verify only that the current preview geometry reaches the renderer's unlit diagnostic path; original topology, orientation, recognition and fidelity remain **UNKNOWN**.
- Runtime execution left Wicked-compiled shader blobs/metadata under root `shaders/hlsl6/` and an app `log.txt`. Narrow `.gitignore` entries now keep these generated outputs out of the source change set without deleting the local files; no assets, captures or private analysis data were staged.
- **VERIFIED — diagnostic log integration:** pinned Wicked's `wilog` formatter uses a fixed-size buffer that truncated the earlier verbose preview record. OpenRepublic now emits separate compact geometry and compatibility-limit records, and a fresh Release run confirmed both records remain complete. Geometry counts are CPU-side parser/helper results, including expected complete draw-range indices/triangles; they are not GPU submission, PSO, or rasterization telemetry.
- **UNKNOWN — independent geometry reference:** private byte comparisons verified that the captured local NMF and MTL copies match installed source files. The latest Windows UI bridge invocation did not return a targetable ModelViewer window, so no same-model reference capture or viewer-side LOD/material selection was observed. The secondary community Blender workflow is not trusted acceptance evidence because importer provenance, transformation conventions, and topology correspondence are unresolved. No orientation, winding, shading, or material correction was made from these observations.
- No original WRSR executable was launched; the copied sample remained outside the repository and the local installation remained read-only. No private path or exact file hash was added to Git.

## Continuation — isolated NMF depth-coverage verification

**21 October 2026**

- Tightened `--capture-preview` so the capture scene cannot be combined with terrain, scene-probe or GLB-probe content. In neutral-unlit mode, the existing app-owned capture path reads the single-sample main depth buffer after rendering, validates the expected descriptor and depth values, and fails if readback is invalid or no pixels have depth coverage.
- A fresh Windows Release run on the private local sample exited successfully and reported nonzero depth coverage for both diagnostic views. This is **VERIFIED OpenRepublic raster-path evidence**: the isolated preview wrote some pixels to Wicked's main depth prepass. It does not establish correct surfaces, complete topology, axes, winding, source shading, recognition or original-game fidelity.
- The existing capture mechanism remains in use; inputs and captures remain private and outside Git. No original game was launched and the installation was not modified.
- **VERIFIED — Windows Release source/build/test:** client build succeeded and CTest passed **14/14**; headless Release build succeeded and CTest passed **13/13**. The build/test sequences ran sequentially after the source changes.
- **UNKNOWN — model acceptance:** there is still no same-model matched ModelViewer capture. The nonzero depth result does not change the geometry, orientation, material or texture fidelity verdict. Obtain a trusted reference and compare front, side and three-quarter views before renderer changes.
- A read-only assessment confirmed that the separate community Blender importer does not provide a validated full-model reference. Its parsing alignment and topology correspondence remain unresolved, so its renders and aggregate bounds are not used as acceptance evidence. No private importer details, source paths or sample fingerprints were added to the repository.
- **PUBLICATION GATE — D-005:** keep the asset-reading runtime and this local increment unpublished pending rights-holder clarification or appropriate legal review. The shared Agent API integration remains published at `caeba3a`.

## Continuation — three-view capture for visual verification (21 October 2026)

- Extended the existing developer-only `--capture-preview` option from two to three distinct PNG destinations. The application-owned render-target path now uses fixed camera directions for diagonal A, a level +X diagnostic side, and diagonal B, all aimed at the normalized preview bounds. These labels are OpenRepublic camera directions; the original model's front/side axes remain unknown.
- On the same private NMF/MTL pair, both regular and neutral-unlit Release runs exited 0 and produced three 1260x748 captures each outside Git. The capture path now requires nonzero main-depth coverage in both modes, and both runs accepted all three views. Visual inspection shows a dark, stepped regular render and a brighter solid silhouette in neutral mode; the model is not recognizable enough for geometry acceptance. An intervening regular-only rerun produced three identical black PNGs despite exit 0. Code review found regular mode skipped depth validation; that earlier color result remains unexplained.
- No geometry, axis, winding, normal, or material correction was made because a target-matched same-model reference remains unavailable. The latest isolated ModelViewer attempt failed before loading the model with an incomplete relative resource tree; it did not modify the Steam installation. The earlier independent UI observation of ModelViewer rendering a building with its MTL does not substitute for a retained matched-view reference image.
- **VERIFIED — Windows Release client:** build succeeded and full CTest passed **14/14**. **Headless Release:** build succeeded and full CTest passed **13/13**. Build and test sequences ran sequentially after the final source edit.
- **UNKNOWN — visual acceptance:** topology, source axes, winding, dimensions, recognition, and material fidelity remain unverified. **PUBLICATION GATE — D-005:** the asset-reading runtime remains local and unpublished; no source assets or captures were added to Git.

## Continuation — private reference and capture integration (21 October 2026)

- A private same-input ModelViewer reference is available for qualitative inspection. It is not camera- or bounds-matched to OpenRepublic, so geometry, orientation, and material fidelity remain **UNKNOWN**.
- Developer-only capture controls now support up to eleven views with optional camera zoom and elevation. With no overrides, the existing camera directions remain unchanged. Neutral and material-preserving unlit diagnostics remain capture-only.
- A fresh lower-pitch comparison completed in neutral and material-preserving unlit modes with depth validation passing for each requested view. The captures support capture-path operation only; they remain unmatched to the reference and do not establish fidelity.
- The geometry audit now distinguishes regular, neutral-unlit, and material-unlit modes; a fresh capture confirmed the label and nonzero depth record.
- **VERIFIED — Windows Release source/build/test:** the client build succeeded and CTest passed **15/15**; the headless build succeeded and CTest passed **14/14**, sequentially after the capture-path changes. Captures and model-derived data remain private and outside Git.
- The local texture preview remains an experiment: slot-to-map and slot-to-UV relationships are inferred, not accepted source behavior. No geometry or material correction is justified by the unmatched comparison.
- **PUBLICATION GATE — D-005:** keep the asset-reading runtime local and unpublished pending rights-holder clarification or appropriate legal review. No original game file or save was modified.
- Next: use the adjustable capture view for a more comparable reference frame, then make only render-path changes supported by that comparison.

## Continuation — full-model reference framing (21 October 2026)

- The user zoomed out the same-input ModelViewer reference so the complete school is visible in a retained private three-quarter image. It shows the multi-storey mass, repeated windows and entrance; this removes the earlier cropping obstacle and supports a whole-model qualitative comparison.
- OpenRepublic's existing 11-view application-owned captures include the same private input, with optional zoom/elevation. Some views show a strongly stepped/ribbed facade silhouette, while the ModelViewer reference shows a continuous windowed facade. This is an **OBSERVED** difference between unmatched views; exact camera orientation, bounds, source LOD and viewer transforms are not established.
- No geometry or material correction is justified by this comparison alone. Source axes, world dimensions, topology completeness, winding, transform interpretation and texture-slot/UV semantics remain **UNKNOWN** or **INFERRED** as previously documented.
- A synthetic mixed-normal regression now explicitly checks that both the flattened normal vector and source-normal mask match the flattened vertex count, and that only vertices with source normals are marked for source-normal application.
- **PUBLICATION GATE — D-005:** keep the asset-reading runtime local and unpublished. The shared Agent API and synthetic integration remain on the published baseline; the 11 local commits are scoped to the NMF preview path and are not a safe publishable subset while D-005 is unresolved.

## Takeover increment — winding fix, ambient fill and textured school (21 October 2026)

- **Root cause of the "stepped/ribbed, dark" facade:** NMF triangles are wound opposite to Wicked's CCW front face, so every outward wall was culled and interiors were visible. The ribs are real window-reveal geometry. `BuildNmfPreviewGeometry` now exchanges indices 1 and 2 per triangle (data-driven: 418/418 school and 3791/3791 second-model triangles agree with stored normals). The NMF preview also adds a fill-light weather (ambient/horizon/zenith) so lit captures are legible.
- **Independent geometry evidence:** a Python mirror of the parser reproduces every object's declared bounds exactly, and a software rasteriser with `_TC0` and slot-0 DDS (no V flip) gives a coherent textured facade. See the ledger entries dated 21 October 2026 (Claude takeover). A ModelViewer matched-view comparison is still not retained; that acceptance item remains UNKNOWN.
- Lit and material-unlit Wicked captures of the school (zoom 2.0, elevation 25) show a recognizable textured building with doors, windows and entrance stairs. Captures live in the private research `scratch\claude-captures` folder, outside Git.
- **VERIFIED:** client Release build, CTest 15/15; headless Release build, CTest 14/14 (run after the final source change).
- **UNKNOWN:** slot 1/2 (specular/bump) semantics, `_TC1`, alpha/colour space, orientation against the game world frame. **D-005:** NMF runtime stays local and unpublished.
- Next: second structurally different model, then specular/bump loader trace; keep unmatched-view caveats.

## Second-model verification — residential brick block (21 October 2026)

- Geometry/bounds/winding checks repeat cleanly on `residental_brick_3` (all three LODs; different vertex flags and two submaterials); the app imports LOD0 and renders a closed building with default framing. Details and limits are in the ledger section "Second model".
- Material binding is the open gap: this model's MTL name (`lambert1`) matches neither NMF submaterial, so the app keeps diagnostic colours. A corpus survey shows name mismatches are common (124/259), `$TEXTURE_MTL` is a separate directive, and `buildingtypes.ini` ties MODEL/MODEL_LOD/MATERIAL/MATERIALEMISSIVE together. No fallback was invented.
- Software-render evidence reconfirms unflipped `_TC0` UVs. Capture zoom 2.0 fails depth coverage on this elongated model; default framing works.
- No source change in this increment beyond the earlier winding commit; tests unchanged (client 15/15, headless 14/14 from the previous run).
- Next: trace the loader's submaterial-to-MTL matching and `$TEXTURE_MTL` path base; parse `buildingtypes.ini` blocks as data.

## Material loader trace and binding (21 October 2026)

- Static Ghidra review of C3DDLL32.dll (build `23935965`, private outputs in the research folder `ghidra-claude`) established: submaterial selection is single-section → that section, else first exact name, else first section; and `$TEXTURE_MTL` = MTL directory + `/` + name. See the ledger section "Material loader trace".
- Implemented `SelectMtlSubmaterialsLikeLoader` and `ApplyMtlDirectoryToRelativeTextures` (plus app wiring and tests). The residential brick block now loads its two slot-0 textures and renders textured; the school is unchanged (6/6). The old exact-name binder remains for its own tests but is no longer used by the app.
- **VERIFIED:** client Release CTest 15/15, headless Release CTest 14/14 after the final source change.
- **UNKNOWN:** 64-bit loader parity, texture slots 1/2, emissive materials, `buildingtypes.ini` model/LOD selection. **D-005:** NMF runtime stays local and unpublished.
- Next: parse `buildingtypes.ini` blocks as data (MODEL/MODEL_LOD/MATERIAL) so a building type, not a file pair, drives the preview.

## Building types drive the preview (21 October 2026)

- Added `ParseBuildingTypes` / `ResolveBuildingTypeAssets` (`building_types.h/.cpp`) and CLI `--content-root`, `--building-type`, `--building-lod`. The type's model and explicit MATERIAL feed the existing preview; roots are contained and traversal/drive paths rejected. Parser, rejection and containment cases are covered by `OpenRepublicBuildingTypesTests`.
- A throwaway harness (outside the repo) confirmed the real file parses (489 types) and every model, material and LOD reference resolves. Captures of `residental_brick_3` (LOD0/1) and `panelak_kvp1` show textured buildings.
- **VERIFIED:** client Release CTest 16/16, headless Release CTest 15/15 after the final source change. **UNKNOWN:** loader semantics of the ini (comment prefix, LOD numbers, emissive use). **D-005:** runtime stays local and unpublished.
- Next: either trace the ini loader in SOVIET64 for LOD/emissive meaning, or start placing a type at a terrain position.

## Historical pause status — 21 October 2026 (superseded by the current checkpoint)

- At the project owner's request, development is paused with the master playable-alpha goal still incomplete. No further code work is planned in this checkpoint.
- **VERIFIED — Windows Release build:** the full configured client build completed successfully after the mixed-normal test assertion was added. This confirms the changed test source compiles; the client CTest suite was not rerun afterward.
- **TEST STATUS:** the last client/headless CTest receipts remain 15/15 and 14/14 from before that assertion. Neither suite was rerun in this pause turn; the headless build was not rebuilt after the assertion.
- **OBSERVED — visual reference:** the user zoomed out the same-input ModelViewer view to show the complete school. The existing OpenRepublic views remain unmatched for camera and bounds, so the visible facade difference does not justify a geometry or shading correction.
- **PUBLICATION STATUS AT THAT HISTORICAL CHECKPOINT:** branch `main` was 11 commits ahead of `origin/main` at `4cc14aa220f59ab46907e5ea03cda3a363aeb496`. The NMF reader/preview work had not yet been pushed. The owner later authorized OpenRepublic-authored source publication; see the current checkpoint above and D-005.
- **WORKTREE:** the latest documentation reconciliation and mixed-normal regression assertion are uncommitted in `README.md`, `docs/DEVELOPMENT_CHECKPOINT.md`, `docs/PROJECT_PLAN.md`, `docs/REVERSE_ENGINEERING_LEDGER.md`, and `tests/content/nmf_geometry_tests.cpp`. No original game payload, capture, or private research data was added.
- **Resume sequence:** rerun the client and headless CTest suites, then perform a camera/bounds-matched comparison and model-space geometry check before proposing a render correction. Reassess D-005 before any publication.

## Historical development pause and goal-status clarification — 21 October 2026 EDT / 22 October UTC (superseded)

- **VERIFIED — repository state:** `main` and `origin/main` point to `0545eda87c4cfc89b63db31a45cae561982d3400`. The local branch has no unpublished commits.
- **VERIFIED — public source scope:** the owner authorized publishing OpenRepublic-authored source. The public repository contains no WRSR assets, saves, private captures, or decompiled research. No separate binary/content release has been published; D-005 remains open for distribution of original content and binaries that use it.
- **TEST RECEIPTS:** latest recorded client Release CTest is 26/26 after road import; latest recorded headless Release CTest is 29/29 after Lua economy rules. Neither receipt is a fresh run after the later autopilot script update. No build or tests were run for the current uncommitted `--map-lod` change.
- **OBSERVED — reference:** the owner zoomed out the same-input ModelViewer view so the full school is visible. Views remain unmatched for camera and bounds; model and material fidelity remain **UNKNOWN**.
- **WORKTREE:** uncommitted source files are `src/app/main.cpp`, `src/app/ContentPreview.h`, `src/app/ContentPreview.cpp`, `src/app/OpenRepublicApplication.h`, and `src/app/OpenRepublicApplication.cpp`. Documentation status updates in this pause are also uncommitted. No other changes were observed before this documentation update.
- **DEVELOPMENT:** this historical pause was superseded by the 22 October active development checkpoint at the top of this document and the continuation receipt in `docs/PROGRESS_LOG.md`.
- **PROMPT CLARIFICATION:** the prior `docs/AUTONOMOUS_GOAL_PROMPT.md` said to treat new messages as steering and then resume development unless the owner canceled or replaced the goal. It did not preserve a development-only pause across continuations. The assistant also incorrectly treated the owner's development pause as a persistent-goal pause. The prompt now says a development-only pause keeps the goal active and remains in effect until the owner asks to resume; the goal itself is paused only on an explicit goal-pause or cancellation request.

## Chronological continuation — session-bound economy bridge hardening

**2026-10-22 00:53 EDT (04:53 UTC)**

- Continued the map-region economy controls from the previous report review. The next architecture target remains an app-owned `economy::World` with source placement IDs preserved through region import; current controls still reach a separate process and retain an approximate type/coordinate binding.
- Fixed a bridge error-envelope defect: a non-empty HTTP error body could previously be stored as a world snapshot. The overlay now validates `ok`, object `result`, and session identity before publishing each snapshot; protocol/bridge failures clear cached status and require a fresh handshake.
- Added a per-`Dispatcher` session marker to economy-host response envelopes and `hello` results. Requests can supply this marker; the host returns `stale_session` before method dispatch when it does not match. The overlay binds catalog, world, action, and command-poll requests so a replaced host cannot accept a stale queued step or reuse an old command handle.
- Removed per-cycle hello traffic. The overlay handshakes on connection/recovery and reads session identity from every response. It polls and rotates one pending command handle per one-second world poll, preserving bounded latency while avoiding polling all 32 handles every cycle. The economy host retains a finite 10,000,000-request cap for long-lived use.
- **VERIFIED — Windows Release build:** `OpenRepublic`, `OpenRepublicEconomyHost`, and `OpenRepublicEconomyHostTests` built successfully in `build/openrepublic-vs2022`.
- **VERIFIED — host regression:** the economy-host test checks that hello returns a stable session ID, protocol errors carry it, and a step carrying a different session ID is rejected without advancing the world tick.
- **VERIFIED — client CTest:** 29/29 passed. **VERIFIED — headless Release build and CTest:** build succeeded and 29/29 passed.
- **UNKNOWN — graphical acceptance:** no live map-region button flow or host-replacement UI behavior was exercised. Tests do not directly cover the overlay's asynchronous HTTP thread. The type/coordinate building association remains approximate, and package identity is not checked by the current center/radius validation.
- No original WRSR process was launched; no game installation file or save was changed. The source delta remains an uncommitted local working-tree increment over `da8f460`.

## Chronological continuation — app-owned map-region economy and placement identity

**2026-10-22 01:03 EDT (05:03 UTC)**

- Re-read the current project plan, decisions, evidence ledger, economy API/playbook, progress log, and user-supplied master goal. The report's next goal was to move the map-region economy into the app and carry placement identity through import; the active implementation now reaches that source/build/test milestone. The full playable-alpha goal remains active and incomplete.
- Added `sourcePlacementKey` to region placement refs and a `placedSourceBuildings` result mapping each accepted source key to its imported base type and economy building ID. Existing `placedBuildings` remains available. Duplicate explicitly supplied keys fail transactionally, including duplicates encountered after an earlier item would otherwise be skipped; the caller's prior world remains unchanged.
- The map preview records each rendered instance's index in the exact `allPlacements` vector supplied to import. The app-owned integration uses that key to map selection to the imported building ID. This is exact for that load and source vector; it is not a stable identity across package edits or revisions.
- Added the default in-process path: load the app's bounded content catalog, import region buildings and decoded roads into an owned `economy::World`, construct a typed `EconomyApi`, and use it for inspect/toggle/haul/24-tick-step actions. Snapshots update building status presentation and truck marker entities. A resource is selected only when the source type's recipe outputs intersect the destination type's recipe inputs. Direct pending command handles are bounded at 64.
- Kept `--economy-url` as the optional external-host mode. That mode still uses the session-bound bridge, its independent host-owned world, and approximate type/position association; it is not sharing the default app-owned world.
- **VERIFIED — Windows Release build:** `OpenRepublic`, `OpenRepublicEconomyHostTests`, and `OpenRepublicRegionImportTests` built successfully in `build/openrepublic-vs2022`.
- **VERIFIED — focused identity regression:** `CompatRegionImport` passed 1/1, covering identity mapping, deterministic repeated import, and duplicate-key transactional failure.
- **VERIFIED — client Release suite:** CTest passed 29/29. **VERIFIED — headless Release suite:** build succeeded and CTest passed 29/29.
- **VERIFIED — source review:** the default app path calls the typed economy API and maps loaded placement keys to imported IDs; a focused review corrected haul resource selection to require a producer-output/consumer-input match.
- **UNKNOWN — graphical runtime:** no map-region UI session exercised picking, direct command outcomes, status colors, truck markers, or save/load. The test suites do not drive this graphical path. No WRSR process, installation file, save, proprietary asset, private fingerprint, capture, or research output was touched or added to Git.
- **INFERRED / approximate model inputs:** catalog rates, footprints, and vehicle properties remain authored/derived compat values, not demonstrated WRSR formulas; decoded roads are imported through the existing raster/chord path and economy cells are quantized. This increment proves interface wiring and identity mapping, not WRSR compatibility or gameplay parity.
- **Next target:** version a trusted-local controller endpoint into the same app-owned `World` with shared revision/session/command semantics, then capture a synthetic/open-fixture graphical acceptance run. Keep the separate host bridge clearly designated as a separate-world mode until that endpoint exists.
- **Repository receipt:** branch `main`, synchronized public base `da8f460`; 21 tracked files are locally modified and uncommitted. No commit, push, or release publication was made.

## Chronological continuation — matched-camera map-region graphics showcase

**2026-10-22 01:31 EDT (05:31 UTC)**

- Re-read the supplied master development goal and current checkpoint, project plan, decisions, ledger, and chronological progress log. The immediate graphics target was a genuine OpenRepublic map-region render pair with fixed scene/camera inputs; the broader active goal remains the playable independent WRSR modernization.
- Added CLI options for one-shot `--showcase-capture`, baseline/enhanced look selection, and yaw/elevation/radius-relative camera controls. Validation requires `--map-package` with `--map-region`, an absolute non-overwriting PNG destination, and no conflicting probe mode. The dedicated render path waits for renderer startup and temporal settling, records the fixed camera, checks main-depth readback/coverage, saves the 3D render target without GUI, and exits with capture status.
- Added the enhanced scene look: directional shadow casting, warm key light, ambient fill, a graded sky, volumetric clouds, height fog, SSAO, FXAA, bloom, sharpening, and restrained terrain/road material values. The imported meshes, placement matrices, terrain and road geometry remain the actual same inputs across both modes. Baseline preserves the existing map-region default.
- The private read-only map sample from WRSR build `23935965` resolved to an automatically selected center placement. Both captures logged 21 building instances from 11 model types, 99 selected source placements, 1 unresolved type, zero failed model loads, zero LOD fallbacks, and 70 road links drawn as chords. Camera target `(0, 8.260, 0)`, distance 375 m, yaw 45°, elevation 24.5°. Both Release runs exited 0, captured 1260×748, and reported identical 62.310% main-depth coverage.
- **VERIFIED — Windows Release source build:** the `OpenRepublic` target rebuilt successfully after the showcase implementation. **VERIFIED — app render-target capture:** baseline and enhanced runs succeeded with equal dimensions and matching scene/camera summaries. **OBSERVED — visual comparison:** the enhanced frame shows graded sky/clouds, atmospheric haze, and altered lighting/material response; the baseline remains the previous flat-sky/flat-ground view.
- A first enhanced tuning pass using Wicked's realistic-sky LUT rendered a black lower sky. Replacing that with the graded procedural horizon/zenith plus height fog produced the inspected enhanced frame. This implementation does not import terrain textures, vegetation, native vehicles, rail, or water, and does not decode curved-road polylines; the showcase is a bounded renderer increment, not a full fidelity claim.
- Exact local input paths, five source-file hashes, commands, logs, and output hashes are recorded in the private graphics capture receipt. No WRSR process was launched. Original input files remained read-only; no proprietary source, capture, path, or fingerprint was added to Git.
- **NOT RUN:** CTest and performance benchmarking were not run for this increment. **Repository receipt:** public base remains `da8f460`; previous dirty integration edits were preserved, showcase source/docs remain uncommitted, and no commit or push was made.
- **Next goal target:** implement a versioned trusted-local controller endpoint on the app-owned `economy::World`, then capture a synthetic/open-fixture graphical interaction. In the graphics stream, add supported terrain materials and authentic vegetation/vehicle rendering before treating a showcase as complete.

## Chronological continuation — app-owned trusted-local controller endpoint

**2026-10-22 01:49 EDT (05:49 UTC)**

- Re-read the latest project plan, decisions, development checkpoint, API/playbook, progress log, evidence ledger, and the user-supplied master goal. The immediate target was to expose the app-owned map-region `economy::World` through a versioned local controller endpoint without creating a second world. The broader playable-alpha goal remains active and incomplete.
- Added an opt-in `--economy-pipe-name <name>` Windows named-pipe mode. It requires `--map-package` and `--map-region`; conflicts with `--economy-url`, probe, and capture modes; and accepts 1–64 ASCII letters, digits, period, underscore, or dash. The pipe is `\\.\pipe\<name>`. Omitting the option leaves the endpoint disabled.
- Added a borrowed-API `Dispatcher` mode. The controller shares the exact app `EconomyApi`, world revision, command handles, event journal, and queue. Embedded requests require the session returned by `hello`/`capabilities` except those handshake methods; embedded shutdown is disabled. `submit` keeps the protocol's expected-revision check. The endpoint remains protocol v0 and is not a WRSR interface.
- Added bounded byte-stream JSONL transport. Its worker performs framing and request/response handoff only; the app update thread calls `Dispatcher::HandleLine`. Input is capped at 64 KiB, responses are bounded below 1 MiB, the pending queue is bounded, remote named-pipe clients are rejected, and the process token's default security descriptor is used. One synchronous client is served at a time. An idle client that leaves an incomplete line open retains the slot until it disconnects or the app closes; no idle-read timeout or per-client authentication is implemented. Shutdown cancels synchronous pipe I/O and joins before the dispatcher/API/world are destroyed.
- **VERIFIED — Windows Release source build:** `cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic` completed and produced the Release executable after the integration changes.
- **VERIFIED — focused dispatcher tests:** the Release `OpenRepublicEconomyHostTests` target built and its executable reported `Dispatcher tests passed`. Added regressions cover a borrowed API sharing world and command state, required session binding, and embedded shutdown denial. Full client/headless CTest was not rerun for this endpoint increment.
- **VERIFIED — live in-app controller smoke on local read-only WRSR build 23935965:** `hello` returned protocol v0, controller role, session ID, and region description; `capabilities` omitted `shutdown`; catalog and world queries succeeded with four imported buildings. A stale-session `step` was rejected and did not advance tick 6. A revision-checked `set_building_enabled` submission returned no API error, `step` advanced to tick 7, `poll` returned `accepted`, and the next world query showed the same building disabled. This demonstrates that the local controller path reaches the app-owned world instance; it does not establish WRSR compatibility or a synthetic-fixture acceptance.
- **VERIFIED — endpoint shutdown path:** while a pipe client remained connected and idle after `hello`, a window-close message caused the app to exit with status 0 and no forced process cleanup. This exercises cancellation of the worker's blocked pipe read before endpoint/API destruction.
- **VERIFIED — showcase output safety follow-up:** capture destinations under either `--content-root` or `--map-package` are rejected before directory creation; existing output paths are rejected, and the capture writer promotes a same-directory temporary file with atomic no-replace semantics. The region-preview failure path now reports a mode-specific error and exits with failure. Runtime checks confirmed source-root rejection created no file and an existing capture's hash remained unchanged. Fresh 1260×748 captures were recaptured after this hardening; exact paths, hashes, and logs remain in private research storage.
- **UNKNOWN — graphical acceptance:** no synthetic/open map-region fixture was used for this smoke. Scene picking, button interactions, status colors, haul UI, and truck marker behavior on such a fixture remain unverified. The local WRSR package was read-only; no original-game process was launched and no WRSR file or save was changed.
- **Repository receipt:** changes remain uncommitted over `da8f460`; no staging, commit, push, or release occurred. Existing dirty integration changes were preserved. Full app CTest and hardware performance measurement were not run for this increment.
- **Next goal target:** exercise selection, inspection, toggle, haul, stepping, status, and vehicle presentation through the graphical app on a synthetic/open fixture. Keep the private imported-region runtime result separate from that fixture acceptance. In the graphics stream, terrain materials and supported vegetation/vehicle content remain subsequent targets.
