# Economy scalability measurements

**Status: VERIFIED for the synthetic workloads below; UNKNOWN for large imported regions and original-game parity.**

Measurements were taken on 23 October 2026 in the Windows Release headless build
(MSVC 19.44, VS 2022, Windows SDK 10.0.26100). Baseline and updated runs used
the same machine, benchmark source, build configuration, and deterministic
scenarios. Timings are local microbenchmarks, not a cross-platform performance
guarantee.

## Before and after

| Synthetic workload | Baseline | Updated | Baseline hash | Updated hash |
|---|---:|---:|---:|---:|
| 40,000 buildings, 1 production thread | 1.059 ms/tick | 0.345 ms/tick | `798402018939142678` | `798402018939142678` |
| 40,000 buildings, 2 production threads | 1.022 ms/tick | 0.262 ms/tick | `798402018939142678` | `798402018939142678` |
| 40,000 buildings, 4 production threads | 1.027 ms/tick | 0.232 ms/tick | `798402018939142678` | `798402018939142678` |
| 40,000 buildings, 8 production threads | 1.097 ms/tick | 0.324 ms/tick | `798402018939142678` | `798402018939142678` |
| 1,500 active haul orders | 5.918 ms/tick | 0.521 ms/tick | `11567758099352643217` | `11567758099352643217` |
| 5,000 active haul orders | 6.389 ms/tick | 0.524 ms/tick | `444432331517164768` | `444432331517164768` |
| API step, 40,000 buildings | 604.373 ms/tick | 1.955 ms/tick | `17407156440371658646` | `17407156440371658646` |
| API `QueryWorld`, 40,000 buildings | 18.042 ms/query | 17.385 ms/query | n/a | n/a |

The baseline values were measured from the tracked source before the changes;
the same benchmark printed hashes for both revisions. A second updated run
reproduced all hashes exactly. API stepping was timed after the production
workload's initial 101 ticks; `QueryWorld` was called five times on the same
40,000-building state. Query serialization is still a full-world operation,
so this change should not be read as a paged-query optimization.

## Changes behind the measurements

- Road traversal now builds a derived enabled-road adjacency map on demand and
  invalidates it when roads are added or their enabled state changes. BFS keeps
  the existing left, right, down, up neighbor order, but uses a keyed visited /
  predecessor map instead of linearly rescanning the visited cells. The cache
  is neither serialized nor included in the authoritative state hash.
- Vehicle IDs are monotonically allocated; removals preserve vector order.
  Vehicle lookup now uses binary search. Building and order vectors likewise
  retain their ID order through append and erase, so per-tick sorting was
  unnecessary and has been removed.
- The API event journal now captures only building stall records, order cargo,
  and the tick before and after advancing. It no longer constructs two complete
  world snapshots, computes their hashes, or converts the after-snapshot into a
  `WorldView` just to find stall and delivery transitions.

The optimized 40,000-building tick was about 3.1 to 4.7 times faster across the
tested thread settings. The 1,500-order tick was about 11.4 times faster, and
the API step about 309 times faster in this event-capture-heavy scenario. API
query time remained approximately 17 to 18 ms because that query still materializes
the entire world.

## Scope and limits

These are synthetic OpenRepublic worlds. The haul cases use a straight road
row with 40,000 unit road segments and up to 5,000 orders; the production case
uses 40,000 buildings. They do not combine 20,000 buildings, 5,000 orders, and
200,000 road cells into one scenario. No shipped terrain or save was imported
for this measurement, and no original-game files were read. Region import cost,
road rasterization/import, large-map road mutation cost, JSON world serialization
at imported-region scale, and memory use remain unmeasured. No WRSR simulation,
format, or performance compatibility claim follows from these results.
