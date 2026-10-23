# Workforce reliability, scale and next parity goals

Prepared 23 October 2026 (America/New_York).

**Status:** bounded development increment implemented and verified locally.
Full WRSR feature parity remains incomplete. The working source is
`D:\.CODEX_Projects\OpenRepublic`; the supplied WRSR-WICKED-ENGINE directory is
an older non-Git snapshot. Initial source base was `833fc84`; another active
development stream advanced shared main to `bbbbed4` during this work. Its
header/road/graphics edits were preserved. This increment is uncommitted.

## Implemented

- Stable worker assignment now takes one linear pass after totaling residents,
  rather than rescanning the world per workplace and recipe. Production threads
  read the same immutable assignment vector. Snapshots use the same allocation
  rule. No persistent assignment cache or save/hash format was introduced.
- Disabled workplaces release residents to later enabled workplaces. Installed
  worker requirements still include disabled buildings; assigned totals count
  enabled allocations. This deliberately corrects prior disabled-site behavior.
- Native v6 loads reject recipe progress above its interval, matching earlier
  versions. Previously v6 bypassed that bound.
- Catalog reload rejects housing reductions below current residents and changes
  to population opt-in presence or food-resource binding before mutation.
  Same-structure numeric tuning remains supported.
- Lua economy tuning preserves food, housing, worker requirements, trade prices
  and trade-post designation, alongside existing utility metadata. Reloading
  rules no longer silently drops those declarations.
- The [parity roadmap](PARITY_ROADMAP.md) now records unassessed major systems
  and three sequenced milestones. [Construction evidence](CONSTRUCTION_EVIDENCE.md)
  establishes ordered loader operations without inventing work/material units
  or geometry-derived automatic costs.

These are **VERIFIED OpenRepublic source/test results**, not original-game
population, staffing or economic parity.

## Validation receipt

- Full Windows MSVC Release headless build succeeded;
  `ctest --test-dir build/openrepublic-headless-vs2022 -C Release --output-on-failure`:
  **78/78 passed** (8.67 seconds in the final run).
- Full Windows MSVC Release client build succeeded;
  `ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure`:
  **79/79 passed** (11.62 seconds).
- Regression coverage: disabled-site release/re-enable; partial staffing with
  multiple recipes; 16,384-building serial/parallel and batched-step agreement;
  uint64 saturation and non-duplicated allocation; utility generator/consumer
  staffing; small-world save/reload continuation; malformed v6 rejection;
  transactional unsafe catalog reload; live resident/food state through Lua
  tuning and canonical save/reload.
- Independent review found no remaining concrete correctness or threading
  defect in the reviewed diff. Parent review strengthened initial worker tests
  to avoid oversized checkpoints and actually exercise placed, populated worlds.
- `git diff --check` passed. No game launch, game/save modification, graphics
  capture, publication, or hardware acceptance was part of this increment.

## Synthetic scale measurement

**OBSERVED, one run per point:** standalone MSVC Release, Ryzen 7 7800X3D,
baseline `833fc84` versus the candidate economy source. One dorm provides one
resident for every enabled one-worker producer. Each producer has two
every-tick recipes producing one ore and one goods unit. Setup and one warm-up
tick are untimed; timed sections are 20 ticks and 10 full snapshots, ending at
tick 22. Candidate source matched the final economy source byte-for-byte.

| Workplaces | Baseline ms/tick | Candidate ms/tick | Baseline ms/snapshot | Candidate ms/snapshot | Matching final state hash |
|---:|---:|---:|---:|---:|---:|
| 1,000 | 1.87433 | 0.02274 | 1.20247 | 0.31262 | 4330332918177738232 |
| 4,000 | 49.0748 | 0.15039 | 27.8486 | 1.94046 | 5129557936606716032 |

Both variants produced 22,000 ore and goods at 1,000 workplaces, and 88,000 of
each at 4,000. These timings are single samples with baseline first, not a
variance estimate or a whole-engine performance claim. No-op assignment worlds
and mixed mobility/utility workloads need separate profiling.

The maintained authored driver is `tests/economy/workforce_bench.cpp`, built as
`OpenRepublicWorkforceBench` and intentionally excluded from CTest timing gates:

```powershell
cmake --build build/openrepublic-headless-vs2022 --config Release --target OpenRepublicWorkforceBench
.\build\openrepublic-headless-vs2022\tests\economy\Release\OpenRepublicWorkforceBench.exe 4000
```

The maintained driver adds argument and result validation to the measured
driver; a smoke run produced the same final hash and totals (0.13325 ms/tick,
1.40705 ms/snapshot). It accepts 1..20,000 workplaces; malformed input was
checked to exit with diagnostic/code 2. Exact measurement source copies,
driver and run notes remain in ignored `build/workforce-benchmark-20261023/`.

## Acceptance boundaries and next increment

The 16,384-building test proves in-memory deterministic execution; it does not
claim persistence at that size. Native world payloads remain capped at 1 MiB;
checkpoint continuation is tested separately on smaller worlds. Existing
staffing scheduling and saturated aggregate reporting are authored policies.
They do not establish commuting, citizen needs or service behavior.

The next gameplay milestone is a fully specified, opt-in native construction
site lifecycle: ordered authored phases, materials delivered by existing
logistics, explicit work admission, inactive production until completion, shared
API explanations, and save/replay tests. Original-definition cost import must
wait for numeric consumer/geometry evidence; `$COST_RESOURCE_AUTO` cannot be
flattened into a final material amount. Broader passenger/service, utility and
transport parity remains on the roadmap with separate evidence gates.
