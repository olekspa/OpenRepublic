# Development continuation: cargo-preserving haul replanning

**Later continuation:** the [save-integrity milestone](DEVELOPMENT_REPORT_2026-10-22_SAVE_INTEGRITY.md) is now implemented, with both full Release suites passing **40/40**. The 39/39 receipts and save-integrity follow-up listed below describe this earlier logistics checkpoint. Building access and graphical economy acceptance are the next targets.

**Date:** 22 October 2026 (America/New_York)
**Scope:** independent OpenRepublic economy and controller interfaces.
**Source base:** `d409d91` on `main`; the locally recorded `origin/main` matched at task start.
**Status:** implemented and verified in local Windows Release builds. Changes remain uncommitted and unpublished. The broader playable-alpha project is incomplete.

## Project review and selected goal

Reviewed the README/contributor rules, project plan and decisions, development checkpoint, latest handoff and progress log, graphics/DLSS status, economy API/playbook, compatibility catalog, dependency manifest, relevant evidence-ledger entries, CMake targets, and authoritative economy/API/host sources and tests. The handoff and current Git history supersede the older 03:10 pause/revision/benchmark-edit notes. Five documentation files were already dirty at task start; those edits were preserved. The benchmark file itself was clean.

The project already has a data-driven C++ economy, local controller interfaces, rule tuning, batch haul commands, import adapters, and graphics experiments. The bounded feature selected here is replanning a haul already in progress without deleting the truck or losing its cargo. Correctness takes priority over speculative haul parallelism: orders share inventories and the existing production benchmark did not establish a threading speedup.

## Implemented behavior

- `World::RetargetHaulOrder` and `EconomyApi::RetargetHaulOrder` preserve the existing source/resource/type, truck ID, order ID, and cargo while changing the destination at a tick boundary. The controller advertises and accepts `retarget_haul_order` with `orderId` and `destination`.
- Destination validation uses the state after earlier queued commands. Unknown orders, missing/same-source destinations, absent resource storage, and duplicate source/destination/resource orders reject without rewriting the order. Selecting the current destination is an accepted no-op.
- Loaded trucks route from their actual cell; empty trucks return to their source before loading. Disconnected or full destinations retain cargo and expose a waiting/route failure. A subsequent retarget can redirect a truck waiting at full storage.
- Loaded cancellation now rejects with `cargo_in_transit`. Previously it deleted the vehicle and its cargo. Empty cancellation still removes the order and vehicle.
- Fixed the related return-leg defect: an empty route-blocked truck could reload source inventory while still away from the source. Loading now requires being at the source access cell.
- Economy world saves now emit v2 and read v1. The record layout is retained; new command/failure enum values are v2-only. Pending retargets and active cargo survive save/reload. Added order cargo/cell/direction validation required by this path.
- Rule reloads reject recipe resource rewiring even when input/output list lengths match. Amount/interval tuning remains accepted. Vehicle capacity reductions below current cargo reject transactionally, preserving saveability and inventory bounds.

No new dependency, original-game data, SDK binary, or private research output was introduced.

## Verification receipt

**VERIFIED — source/build:** full `Release` builds completed for both configured trees, including the Windows `OpenRepublic` client and headless economy host.

```powershell
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
cmake --build build/headless-host --config Release --parallel 6
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
ctest --test-dir build/headless-host -C Release --output-on-failure
```

**VERIFIED — regression suites:** client **39/39**, headless **39/39**. Three new CTest entries are `EconomyHaulRetarget`, `EconomyCatalogReload`, and `EconomyHaulRetargetProtocol`. They cover loaded/empty/disconnected/full-storage retargets, cargo conservation and capacity bounds per tick, connectivity recovery, blocked empty-return recovery, invalid and duplicate destinations, same-tick command ordering, no-op destinations, v1 migration/v2 saves, pending/active deterministic replay, transactional malformed cargo/cell rejection, capability discovery, strict JSON fields/types, observer denial, stale revisions, queue/apply/poll results, and transactional rule rewiring/capacity rejection.

**VERIFIED — separate process:** a Python controller launched the rebuilt headless host with no game inputs, bound requests to its `hello` session, built a synthetic farm and two disabled mills, produced and froze twelve grain units, created one loaded haul, observed cancellation rejected as `cargo_in_transit`, retargeted that same order/truck, and stepped through delivery. At tick **32**, the original destination held **0**, the redirected destination **4**, and stored plus onboard grain remained **12** at every checked tick. The host shut down cleanly with exit 0 and no stderr.

**UNKNOWN / not exercised here:** desktop picking/buttons, live named-pipe retarget requests, visible truck movement, original-game economic comparison, cross-platform replay, and performance impact. The embedded dispatcher regression exercises the borrowed API used by the app, but it does not prove pipe transport or GUI acceptance. No original game process was launched and no installation/save was modified. Existing graphics claims retain their own evidence boundaries in `GRAPHICS.md`.

## Ranked next useful development goals

1. **Save-integrity validation.** Reject invalid or reused `NEXT` counters, zero/duplicate queued command IDs, queue/result inconsistencies, invalid boolean domains, and noncanonical entity ordering transactionally. Add adversarial fixtures and verify a valid world continues with fresh unique IDs after load. The focused cargo checks in this increment are not a complete save audit.
2. **Building access invariants.** Prevent new footprints from covering existing access cells, and prevent new access cells from falling inside footprints. Placement and save-load validation must agree and behave independently of record ordering. Extend region-import tests before tightening imported layout acceptance.
3. **Graphical economy acceptance with synthetic content.** Reproduce the earlier black-viewport/status-clipping report against the current client, then verify selection, inspect/toggle, haul creation, retarget via shared authority, stepping, status refresh, and visible truck movement. The later real-content showcase does not close this separate interactive-fixture gate.
4. **Autonomous logistics replanning.** Teach the adviser/controller to prefer retargeting stalled loaded orders toward reachable compatible free storage; compare inventory outcomes with an identical idle/reference run. Preserve deterministic command order and separate planner heuristics from simulation rules.
5. **Profile before scaling.** Measure API snapshots/event capture, route search, linear lookups, and sorting with haul-heavy synthetic workloads. Preserve serial reference hashes before any parallel haul design; shared endpoint inventory writes need explicit conflict ordering.

Graphics alternatives remain the optional public DLSS build path and temporal/resize/fallback acceptance, or build-specific terrain/water/electro semantic research. They carry separate renderer/dependency/evidence gates; this logistics milestone makes no claim to complete them or WRSR compatibility.
