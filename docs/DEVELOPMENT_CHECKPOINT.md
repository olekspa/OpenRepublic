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
