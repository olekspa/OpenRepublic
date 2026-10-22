# Development continuation: economy save integrity

**Later continuation:** [building-access validation and region UI](DEVELOPMENT_REPORT_2026-10-22_ACCESS_AND_UI.md) are now implemented locally, with both Release suites passing **41/41**. The 40/40 receipt and building-access follow-up below describe this earlier checkpoint.

**Date:** 22 October 2026 (America/New_York)
**Source base:** `d409d91`, with the preceding local haul-retarget increment preserved.
**Status:** local implementation and Windows Release verification complete for this increment. The broader playable-alpha goal remains active and incomplete. Changes are uncommitted and unpublished.

## Problem and resulting behavior

The economy loader trusted several numeric casts, ID counters, entity ordering, and queued-command/result relationships. It could accept a world that later reused a building/order ID or applied a queue inconsistently. Separately, runtime command histories could exceed 1,024 records while the loader rejected any section above that count, making an otherwise valid native save unloadable.

The reader now validates the persisted state before replacing the target world:

- Numeric tokens use `from_chars` into their actual destination types. Negative unsigned values, plus prefixes, overflow, partial numbers, invalid boolean domains, and narrowing of `NEXT` counters are rejected.
- Grid dimensions are explicitly `1..INT32_MAX`. The `World` constructor enforces the same contract with `std::invalid_argument`, keeping coordinate-neighbor arithmetic within its representation.
- Building, road, order, vehicle, pending-command, and result records must follow native ID ordering. Entity IDs are nonzero and below their corresponding `NEXT` counters. Holes from removal/cancellation remain legal. Road endpoint ordering and duplicate haul source/destination/resource combinations are validated.
- Successful result references must be below the relevant next-building/road/order counter even when the entity has since been removed. Command results retain the native complete sequence `1..nextCommand-1`; pending commands match the queued suffix exactly. Unknown states, inconsistent state/failure pairs, and invalid output-ID combinations reject.
- Pending payloads remain deferred: a nonexistent type, invalid geometry, or a target that an earlier queued command will create can still be saved. These commands accept or reject through the normal tick-boundary execution path after loading.
- Inventory, recipe progress, completion-count/tick bounds, vehicle capacity/cell/direction, revision history, and pending execution budget are checked without modifying the caller's world on failure.

Save sections are now bounded by the input byte budget rather than the unrelated 1,024-definition catalog limit. `MaxWorldSaveBytes` remains **1 MiB**. `SaveWorld` returns an empty string explicitly when output would exceed that budget, checking during serialization; it no longer returns oversized text that its reader refuses. Callers must check for an empty result before writing a file. This is a bounded native text format, not an arbitrary-world-size persistence guarantee. A streaming or larger-world format remains future work.

## Counter boundaries

Entity creation rejects exhausted ID space before wrapping. Queue admission returns command ID `0` without mutation when command IDs, ticks, or the revision budget cannot support execution. It reserves one tick plus every pending command application. `Step` rejects an overflowing batch before advancing, and catalog replacement cannot consume revision space reserved for queued work.

The typed API returns `CommandQueueExhausted` for failed world admission, and the host serializes `command_queue_exhausted`. This is distinct from API command-handle exhaustion. The existing save version remains v2; valid native v1 saves still migrate. No binary or original-game save format was changed.

## Verification receipt

**VERIFIED — build:** full Windows client and headless Release builds succeeded, including `OpenRepublic`, both controller hosts, libraries and verification targets.

**VERIFIED — tests:** client **40/40**, headless **40/40** CTests passed after final source changes. The new `EconomySaveIntegrity` entry checks canonical v2 and retained v1 fixtures, transactional malformed-state rejection, ID/counter/history rules, strict token and boolean parsing, pending dependencies and delayed invalid-command rejection, removed/cancelled IDs, cargo replay, constructor limits and a road at the maximum grid edge.

The regression also creates **1,101 retained command results**, reloads and continues that world with equal canonical saves, queues **24,000 commands** to verify explicit oversized-save failure, and exercises tick/revision exhaustion, exact queue reservation boundaries, and rule-reload reservation. The existing API suite checks explicit world-admission failure with no queued event or world mutation. All prior haul-retarget, content-reader, Lua, import, protocol, and external-controller regressions also pass.

```powershell
cmake --build build/headless-host --config Release --parallel 6
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
ctest --test-dir build/headless-host -C Release --output-on-failure
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
```

Independent source review found and resolved the pending-counter reservation gap, then found no further introduced correctness issue in its focused audit. `git diff --check` passed. All earlier dirty source and documentation work was retained.

## Remaining boundaries and next development

This validates internal consistency of a bounded OpenRepublic save; it is not authentication or proof of the input world's history. The format does not retain a separate history of retired vehicle IDs. It also does not bind the save to a catalog fingerprint, prove cross-build/platform replay, or validate every possible production/vehicle state combination. Building access-cell overlap remains a separate known layout issue.

The next useful milestone is **building-access invariants across placement, loading and region import**, followed by **synthetic graphical economy acceptance** and **controller replanning using retargeting**. Larger-world persistence needs a separate bounded format/caller design. A pre-existing theoretical API handle-exhaustion case also remains: wrappers enqueue before checking the API's own maximum handle, which should be centralized before admission if that path is expanded or given practical limits.

No GUI, live named-pipe request, original-game comparison, graphics performance test, or WRSR process was exercised here. No original installation file/save was changed and no proprietary data or dependency was added.
