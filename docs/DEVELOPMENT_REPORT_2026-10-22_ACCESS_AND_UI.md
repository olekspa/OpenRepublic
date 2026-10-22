# Development continuation: building access and region UI

**Date:** 22 October 2026 (America/New_York)
**Later follow-up:** the owner released the foreground and the remaining bounded GUI checks ran. The [replanning report](DEVELOPMENT_REPORT_2026-10-22_REPLANNING.md) records single-tick truck movement, readable inventory, mill production and overflow interactions, plus the latest client/headless CTest 44/44. Pending-coordination statements below describe the earlier interrupted pass and are superseded by this receipt.
**Source base:** `d409d91`, retaining the local haul-retarget and save-integrity increments.
**Status:** access implementation and synthetic verification complete; graphical presentation follow-up described separately below. The broader playable-alpha goal remains active and incomplete. Changes remain local and uncommitted.

## Building placement and loading

The authoritative economy now validates footprint, access and road reservations together. A building's access is still the cell below its minimum-Z footprint edge, at `origin.x + (width - 1) / 2`. Placement can be disconnected; a road is permitted at that access cell and is not required to place a building.

The validation checks, in order:

1. Footprint bounds: `OutOfBounds`.
2. Overlap with an existing building footprint: `BuildingCollision`.
3. Access outside the grid, access inside another footprint, or a proposed footprint covering another building's access: `AccessCellUnavailable`.
4. A proposed footprint covering an existing road endpoint, whether the road is enabled or disabled: `RoadOnBuilding`.

Rectangle/point comparisons use widened arithmetic. The placement and building-load paths share the same validator. Because roads appear after buildings in the save, road-load validation checks their endpoints against all loaded footprints before committing the temporary world. Failed loads leave the target unchanged and identify access or road/footprint conflicts.

This also fixes a runtime/load inconsistency: previously a new building could be placed over an existing road, creating a native save whose road section would subsequently fail to load. Disabled road cells remain reserved so reopening cannot introduce that conflict.

The rule is symmetric under which building is placed or loaded first. Within a queued batch, the first accepted placement reserves its footprint and access, and later conflicts reject deterministically. Removing a building releases its access reservation. No automatic relocation or repair of old conflicting layouts was added: v1/v2 saves with newly-invalid access conflicts reject transactionally; valid layouts remain supported.

## Region import

`RegionImportReport::skippedAccessConflict` distinguishes access rejection from footprint overlap. Accepted source placement keys still map to their exact created building IDs; rejected placements receive no mapping. Source row order is preserved and determines which conflicting placement wins. `DescribeRegionImport` reports the separate count.

`RegionImportOptions::maxDiagnosticNotes` defaults to 32 and caps retained notes while counters remain exact. Synthetic tests exercise both directional conflicts, accepted-only identity mapping, deterministic repeats, bounded/zero diagnostic budgets, and duplicate-key transactionality. The existing road-connector regression still passes with roads at building access cells.

Imported footprints and quantized positions remain provisional OpenRepublic catalog policy. These checks do not establish authentic WRSR footprints, access points, road topology, or economy behavior.

## Verification receipt

**VERIFIED — Windows Release builds/tests:** full client and headless builds succeeded. Both CTest suites pass **41/41**. The new `EconomyBuildingAccess` entry covers 1x1/2x2/3x2 centering, bounds and maximum grid edges, both access-conflict directions, same-tick command ordering, road-at-access permission, active/disabled road reservations, removal, access-only invalid save layouts under both canonical ID orientations, v1/v2 compatibility boundaries, queued replay, and successful haul delivery after a road-covering placement is rejected.

**VERIFIED — separate controller process:** a session-bound Python controller launched the headless host with synthetic content and received the expected `access_cell_unavailable`, `road_on_building`, `out_of_bounds`, and `building_collision` terminal results. At tick 2, the original one building and one road remained; the process shut down with exit 0 and no stderr.

```powershell
cmake --build build/headless-host --config Release --parallel 6
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
ctest --test-dir build/headless-host -C Release --output-on-failure
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
```

## Graphical fixture and presentation follow-up

**OBSERVED — existing synthetic fixture, live Windows run:** the ground, road, and Farm/Mill/Bakery primitives rendered at the default approximately 900x560 client size. The previous black-viewport report did not reproduce. Picking selected the farm and mill with the correct economy identities. A GUI toggle plus 24-tick step disabled the farm at tick 26 and changed its color; re-enabling and stepping reached tick 50 with full grain storage. Setting the farm as source, selecting the mill, creating a haul, and stepping to tick 74 produced one truck and a mill reporting full flour storage. These observations concern the app-owned synthetic economy only.

That run exposed clipped final status lines/right-edge inventory text, inappropriate million-unit conversion for synthetic amounts, and an oversized/off-road truck cube. The local presentation increment adds a one-tick step control through the same API/bridge path as the retained 24-tick button, wraps/sizes the status label to the viewport with scrollable overflow, splits inventory into lines, displays synthetic quantities as native integers, and uses a smaller marker aligned to the authored fixture road. Genuine imported-region coordinates retain their earlier half-cell policy.

Source review caught an input-routing interaction introduced by scrollable overflow: the old passive-label exception allowed wheel zoom and world picking through the status scrollbar. The app now retains GUI focus while the overflow panel is hovered or its scrollbar is grabbed. The rebuilt code preserves passive-label pass-through when scrolling is not needed; direct overflow scroll/camera-isolation acceptance remains part of the pending graphical pass.

**VERIFIED — presentation source/build:** the revised client builds and its full CTest suite passes 41/41. A fresh launch displays all five region buttons and the initial status panel within the window. Post-change selected inventory, overflow interaction, single-step truck movement and multi-stage production still require the final graphical pass. Desktop input contention interrupted that pass; no additional clicks were issued while waiting for foreground coordination.

No original game process was launched, no installation/save was modified, and no proprietary input was used for these tests. Live pipe retarget transport, authentic map visual fidelity, cross-platform replay, and graphics performance remain outside this receipt.

## Next targets

Finish the bounded graphical presentation pass, then add controller replanning that uses retargeting rather than cancelling loaded orders. Native economy save/load controls, larger-world persistence and catalog identity binding remain useful subsequent features. Preserve the separate evidence gates for genuine-content graphics and WRSR behavior.

**Completed follow-up:** owner-authorized foreground checks observed farm grain 20/20 at tick 26, small marker movement between single ticks 27/28, and mill grain/flour 8/8 at tick 52. A short-window run exercised overflow wheel scrolling and scrollbar dragging with selection preserved on panel clicks and no apparent scene zoom. Task-owned fixtures were closed. The controller replanning increment is also complete locally; see the later report above. No pending foreground question remains.
