# Economy scalability measurements

**Status: VERIFIED for the measured synthetic and local-import workloads below; UNKNOWN for original-game parity.**

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
40,000-building state. The follow-up below adds a bounded page path and measures
both the typed API and JSON host protocol.

## Follow-up: bounded queries and imported-region workload

**OBSERVED, 23 October 2026:** `OpenRepublicEconomyScaleBench` loaded a local
save package through the same catalog, placement, region, and road compatibility
import functions used by the host. It used the base content aggregate catalog
(488 parseable definitions found, 88 catalog building types and 54 resources).
The save contained 736 source placements; 55 were recognizable by that
catalog, 32 were placed in the selected 1,000 m radius, and 275 unknown-type
placements were skipped. The imported grid was 437 x 437 cells. The road
importer considered 1,732 links and accepted 4,988 road segments.
All these counts describe importer output, not the source game's semantics.
The opt-in benchmark accepts `<content-root> <map-directory> [radius-meters]`
as arguments and does not print those paths.

| Local import/API phase | Measured time |
|---|---:|
| Catalog parse and import | 53.40–56.22 ms |
| Placement file load | 45.56–48.42 ms |
| Center candidate scan (up to 256 candidates) | 24.00–24.21 ms |
| Region building placement | 0.19–0.26 ms |
| Road decode, rasterization, and import | 154.86 ms |
| Full import workload | 278.33–283.92 ms |
| Economy API step, 10-step average | 0.0028–0.0037 ms/tick |
| Adviser JSON request | 4.51–4.64 ms |
| Host JSON world query | 51.46–52.94 ms, 379,846–379,847 bytes |
| Host JSON buildings page (32) | 0.404–0.479 ms, 9,800–9,801 bytes |
| Host JSON step request | 0.034–0.131 ms |

The pre-tick hash was `3800139225844556675` and the hash after ten ticks was
`12087323593452597463`. The host full-world and page queries left the latter
hash unchanged. A separate host launch measurement, from process start through
the first hello response, took 175.1, 177.5, and 214.4 ms in three runs; this
includes process startup, import, and hello serialization, so it is not an
isolated import phase.

**OBSERVED:** Six available DLC2 terrain packages were checked against the
same base aggregate catalog; none of their placements resolved to the 88
economy catalog building types. The save import above was therefore the
largest useful real-content workload found in this pass. This is a local
importer compatibility observation, not a statement about DLC2 content or
game behavior generally.

The 40,000-building synthetic query exposed a larger serialization cost:

| Query path | Time | Response size |
|---|---:|---:|
| Existing full typed `QueryWorld()` | 17.34–17.39 ms | all collections |
| Typed buildings page (32) | 0.0067–0.0070 ms | 32 buildings |
| Host full-world JSON | 526–569 ms | about 9.74 MB |
| Host buildings-page JSON (32) | 0.347–0.362 ms | about 7.85 KB |

The full and page results intentionally contain different amounts of data.
The page path avoids materializing and converting unrelated collections and
has deterministic ID ordering inherited from the world vectors. Its revision
argument rejects stale reads, while the existing no-argument full query stays
available and unchanged. On this workload, the full host JSON request is the
next scale limit: callers that request the complete world still pay for all
40,000 buildings and JSON encoding.

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
- `World::SnapshotPage` copies only the requested ordered collection slice.
  The paged API validates the requested revision and converts only those
  entities, while the original full `QueryWorld()` method remains available.

The optimized 40,000-building tick was about 3.1 to 4.7 times faster across the
tested thread settings. The 1,500-order tick was about 11.4 times faster, and
the API step about 309 times faster in this event-capture-heavy scenario. The
full typed API query still takes about 17 ms because it materializes the entire
world; bounded pages are measured separately above.

## Scope and limits

The synthetic scenarios do not combine 20,000 buildings, 5,000 orders, and
200,000 road cells into one world. The local save import uses an aggregate base
catalog and the compatibility importer; skipped types and imported roads are
not proof of complete content interpretation. The measurements do not cover
large-map road mutation under haul traffic, memory use, or production and cargo
loads on the imported region. The existing full JSON query remains expensive
for large worlds. Local content was read-only and the original game was not
launched. No WRSR simulation, format, or performance compatibility claim
follows from these results.
