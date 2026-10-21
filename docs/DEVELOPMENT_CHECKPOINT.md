# OpenRepublic development checkpoint

**Checkpoint:** 2026-10-21 13:53:36 EDT (17:53:36 UTC)
**Repository:** `D:\.CODEX_Projects\OpenRepublic`
**Base revision:** `4b686ac` (`Verify long-horizon simulation replay`)
**Branch:** `main`, tracking `origin/main`
**Goal status:** Active; the playable-alpha objective is not complete.

## Current milestone

M0, the existing interactive logistics loop, has targeted code fixes and passing source/build checks. Its required full UI scenario is still **not runtime-accepted**. The next M0 action is to run the documented paused storage-placement, road, selection, route, delivery, full-destination, and isolated save/reload sequence through a working native-window automation surface. M1 model/material compatibility follows M0; this checkpoint does not claim it has started or passed.

## Work completed in this checkpoint

- Traced the previous repeat-placement failure to storage placement remaining active after its first accepted queue request. Successful single-storage queueing now exits placement mode; queue failure leaves the mode available for retry.
- Traced swallowed world clicks to Wicked Engine `GUI::HasFocus()` treating every visible widget hitbox as focus. The large, passive OpenRepublic status label now allows world input to pass through its hitbox while the other GUI focus checks remain in place.
- Reconciled placed-storage render entities by their ordered stable `OR-<id>` identities, not only by count. Selection is restored by stable ID across a rebuild if that entity still exists.
- Applied the same stable-ID selection preservation to dynamically placed Farms and Mills when their scene entities are rebuilt after the loaded facility set changes. A selected facility is cleared if its ID no longer exists.
- Retained the “Next storage” selector as a fallback selection path. It is useful for deterministic entity selection, but it does not substitute for spatial picking acceptance.
- Added simulation regression assertions that accepted Farm, Mill, and Storage IDs resolve to the intended entities and that an accepted plan keeps those exact IDs after save/load.

## Validation evidence

- **VERIFIED — Release build:** OpenRepublic client and its simulation, terrain-reader, GLB-container, and Lua-rules test targets built with the VS 2022/MSVC toolchain.
- **VERIFIED — client CTest:** 4/4 tests passed.
- **VERIFIED — headless CTest:** 3/3 tests passed.
- **VERIFIED — diff check:** `git diff --check` reported no whitespace errors. Git emitted a CRLF-to-LF normalization warning for `tests/sim/simulation_tests.cpp`.
- **OBSERVED — previous runtime only:** the 21 October project-plan trace opened the OpenRepublic client, paused it, entered/canceled storage placement, selected built-in storage `OR-1003`, read its inventory, and changed the draft truck-plan storage endpoint. That run did not place a storage, run a changed route, deliver cargo through the UI, or save.
- **UNKNOWN — current UI fixes:** there is no post-fix runtime confirmation of passive-label click-through, automatic placement-mode exit, spatial picking of a placed storage, facility-selection restoration after load, or the complete UI logistics sequence.
- The client was launched for this checkpoint, but the available CUA runtime exposed no native-app API and reported no native app windows. REA native UI capture reported `target_unavailable` (“No app is open”) for the running Windows process. Thus the build is runnable but this session could not interact with its window through the available UI automation interface.
- The existing `build/openrepublic-vs2022/OpenRepublic-demo.save` was observed at 205 bytes with last-write time 2026-10-21 11:56:52 AM before the runtime attempt. No save/load button was used in this checkpoint. No WRSR executable, save, or Steam installation file was changed, and no Rival Republics files were modified.

## Exact M0 runtime items still open

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
