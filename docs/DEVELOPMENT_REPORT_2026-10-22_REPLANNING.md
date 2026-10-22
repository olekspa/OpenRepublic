# Development continuation: autonomous cargo replanning

**Date:** 22 October 2026 (America/New_York)
**Later increment:** [native checkpoints](DEVELOPMENT_REPORT_2026-10-22_CHECKPOINTS.md) add catalog-bound export/restart and bring client/headless CTest to 49/49. The remaining-goals section below records the targets identified at this report's checkpoint.
**Source base:** `d409d91`, retaining the local logistics, save-integrity and access/UI increments.
**Status:** implementation and synthetic verification complete for this increment. Changes remain local and uncommitted. The broader playable-alpha goal remains active and incomplete.

The latest project reports identified controller replanning as a useful background task while the final synthetic GUI pass awaited foreground coordination. The core already preserves truck identity and cargo when changing an existing haul destination. This increment gives the read-only adviser and baseline controller a way to use that command when a loaded truck waits at full storage. The owner subsequently released the foreground and the graphical follow-up ran as recorded below.

## Adviser behavior

`advise` retains its existing missing-input and surplus fields and adds `retargets` and `retargetsTruncated`. Its retarget planner processes waiting, loaded `destination_full` orders by stable order ID. It checks actual free resource storage, subtracts other loaded inbound cargo, searches the enabled road graph from the current truck cell, and chooses the shortest reachable destination with building ID as the tie-breaker. It excludes the original source/destination, incompatible storage and existing or newly suggested duplicate haul tuples.

Capacity arithmetic saturates reservation sums and guards subtraction. Earlier suggestions reserve room for later suggestions. Reservations remain conservative at the old destinations as well: proposed diversions do not release an existing inbound reservation. A full-storage failure that became stale after a capacity change is skipped when the current destination can accommodate the truck after reserving other inbound cargo.

The output is capped at 32 suggestions. The planner continues past infeasible orders, so early failures do not permanently hide later useful work. `retargetsTruncated` means additional eligible orders remain unexamined after 32 suggestions; it does not guarantee those remaining orders have feasible alternatives. Sparse road adjacency and per-start-cell BFS caches avoid grid-sized allocations. No scale or latency result is claimed.

The original missing-input heuristic remains separate and unchanged: first-recipe inputs, Manhattan-ranked producers, without route validation for new haul links. The new route and capacity checks apply specifically to the retarget suggestions.

## Controller behavior

The Python baseline controller consumes the additive suggestions before creating new links, uses the advice revision, steps and polls command handles, and distinguishes queued work from terminal acceptance/rejection. It replans after placement attempts advance the world, including rejected placements, and refreshes stale advice at most once per round. Already queued orders are excluded from that retry and retain their duplicate-tuple reservations. Failed creates are no longer remembered indefinitely in a local `ordered` cache. Older hosts without `retargets` retain their previous new-link workflow. The standalone client binds requests to the session returned by its initial handshake; argument checks reject zero/negative rounds or ticks outside 1..100000.

The controller remains a simple heuristic. It can choose storage that fills before the truck arrives; advice is a snapshot and queued commands do not advance the revision. The authoritative C++ simulation continues to enforce validation and preserve cargo while waiting. No cancellation of loaded orders is used.

## Verification receipt

**VERIFIED — Windows Release source/build/tests:** full client and headless builds succeeded. Both full CTest suites pass **44/44**, including the three added entries `EconomyHaulAdviser`, `OpenRepublicAutopilotTests` and `OpenRepublicReplanningExternalController`.

The C++ adviser tests cover read-only canonical saves/hashes, observer access, deterministic output, whole-cargo capacity boundaries, enabled/disabled/disconnected paths, width-derived access, road-distance versus Manhattan ranking, ID ties, zero-edge routes, loaded-inbound reservations, existing/suggested duplicate tuples, stale full-storage failure, non-waiting exclusion, maximum-value reservation saturation, the 32-output/truncation policy and a feasible order after 32 infeasible ones. Precise state fixtures are OpenRepublic-authored canonical saves with synthetic field changes; they establish planner policy, not naturally reached economic histories.

The Python controller entry runs **10** unit cases for command lifecycle counts, terminal rejection, stale advice, conflicting create suppression only after queue admission, duplicate-free refresh, old-host compatibility, successful/rejected placement refresh, session binding and generated-power scoring.

**VERIFIED — separate controller processes with a matched idle control:** two hosts receive identical synthetic definitions, placements, roads and commands and reach equal starting hashes. Two trucks load before their shared four-unit depot fills; the farther truck reaches a loaded `destination_full` stall. The baseline controller retargets that order to empty reachable storage, polls acceptance and retains the original truck/order. At tick **23**, the replanning run has delivered **4** units to the alternative versus **0** for the idle control. Stored plus onboard grain remains **24** at every checked step. The original depot holds four in both runs. The hosts shut down with exit 0.

**OBSERVED — graphical follow-up after foreground permission:** a fresh synthetic fixture displayed selected farm grain 2/20 at tick 2 and 20/20 at tick 26. Queuing a farm-to-mill haul and stepping once produced the small road-aligned truck marker at tick 27; a further single step visibly moved it along the road at tick 28. A 24-tick step reached tick 52 with mill grain 8/8 and flour 8/8. Inventory text and final action lines fit the default window. A second approximately 904x182 window exercised overflow: wheel input revealed the final inventory/selection lines, panel clicks retained the mill selection, and scrollbar dragging moved the text. No camera zoom was apparent in the remaining visible scene during scrolling; this is bounded visual evidence, not instrumented camera/input verification. Both task-owned fixtures were closed.

A concurrent full-suite rerun exposed an existing terrain-test collision: both build trees wrote the same global temporary DDS path. `TestFileDdsReader` now reserves a separate directory with atomic `create_directory` before writing and removes only its own file/empty directory. After this fixture isolation fix, simultaneous full client/headless reruns each passed 44/44. The failure was in test-fixture ownership; no terrain decoder behavior changed.

```powershell
cmake --build build/headless-host --config Release --parallel 6
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
ctest --test-dir build/headless-host -C Release --output-on-failure
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
python -B tests/economy_host/replanning_controller.py build/headless-host/src/economy_host/Release/OpenRepublicEconomyHost.exe
```

`git diff --check` passes. Source changes are in `src/economy_host/economy_protocol.cpp`, `tools/republic_autopilot.py`, focused tests/registration in `tests/economy_host`, and temporary-fixture isolation in `tests/content/terrain_reader_tests.cpp`. Public API/playbook/status docs and the evidence/progress ledgers are updated. Existing unrelated dirty work is preserved; no commit, push or publication was performed.

## Remaining goals

Native economy persistence controls, catalog identity binding and larger-world save capacity remain useful development targets. Route/event/snapshot profiling needs matched synthetic inputs before scaling changes. The graphical pass did not exercise pipe retarget transport, multi-stage bakery/market production, graphics performance, or original content. Original-game economic comparison, cross-platform replay and genuine-content graphical fidelity retain their separate evidence gates.

No original game process is launched for this increment. Synthetic fixtures contain only OpenRepublic-authored definitions and state; no proprietary assets, save payloads or private analysis are added to Git.
