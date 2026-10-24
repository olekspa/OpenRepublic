# Synthetic rail transport

**VERIFIED (OpenRepublic core/API source and headless tests):** catalog station
and rail-only vehicle declarations, rail construction, rail-mode hauling,
conditional world persistence, and typed API queries are implemented. Protocol
v0 dispatch and JSON fields are added in source, but the host target could not
be built in this worktree because `nlohmann/json.hpp` is unavailable. The core
comparison receipt below measures the synthetic rules only.

Rail is an opt-in OpenRepublic economy rule. A catalog may declare a vehicle
named `train` with its own positive cargo capacity and cells-per-tick speed, and
mark a building type as a `station`. A train haul must start and end at station
access cells and follow connected declared rail edges. Roads and rails are
separate networks. Truck orders continue to use road edges.

The catalog marks a rail-only vehicle with the optional `mode rail` suffix,
for example `vehicle train capacity 100 speed 3 mode rail`; these are
OpenRepublic-authored values, not original-game capacity or speed claims.

`build_rail` adds one orthogonally adjacent rail edge between cells, using the
same integer grid coordinate convention as `build_road`. Train movement and
route selection are deterministic. A `mode: "rail"` haul order selects rail
transport; `mode: "road"` selects road transport, and omitted mode keeps the
protocol's road default for existing clients. Rail haul does not automatically
move cargo between a station and a road vehicle. Any road/rail transfer leg
must be represented by a separate explicit haul order and appropriate
intermediate inventory/building. This is a documented model limitation.

The bounded comparison fixture is
[`rail_vs_road.json`](../data/scenarios/rail_vs_road.json). Its scorer receipt
records how many simulation ticks each separate route needs to put 1,000 grain
units into its destination inventory and asserts that rail completes first.
The fixture uses independent source/destination pairs, fixed declared vehicle
capacities and speeds, and disables the source recipes after their initial
stock is produced. Its `comparisonTicks` score field gives the scorer enough
simulation time to measure the slow truck route; the generic `--ticks` budget
still applies to other scenario metrics. It is a comparison of these authored
synthetic rules, not a general performance claim.

The fixture uses a 100-unit train at 3 cells per tick and a 50-unit truck at
1 cell per tick. Both routes span 20 edges. The capacity ratio is 2:1 and the
speed ratio is 3:1, so a roughly sixfold rail advantage is the expected scale
before route-endpoint and discrete-tick effects. This deliberately avoids a
single full-load train trip against hundreds of small truck trips. These are
fixture tuning values for a bounded synthetic comparison, not estimates of
real vehicle capacities or speeds.

**OBSERVED (OpenRepublic Windows Release host, scenario/economy sources matching revision
`5dbdcedc7cddc9066676698ba8a5b72fb1d72321`, 24 October 2026):** the refreshed
receipt records 1,000 grain arriving by train in 123 ticks and by truck in 760
ticks, a 6.2× rail advantage. The scenario, score, and state hashes in
[`RAIL_VS_ROAD_RECEIPT.json`](RAIL_VS_ROAD_RECEIPT.json) identify this fixture
and host run only; none is a WRSR state fingerprint.

## WRSR evidence boundary

`rail.bin` and `rail_secondary.bin` share an observed network layout with
three leading counts, fixed node blocks, and variable node arrays in sampled
packages. Their structural evidence is recorded in
[`REVERSE_ENGINEERING_LEDGER.md`](REVERSE_ENGINEERING_LEDGER.md). The layouts
are useful evidence for geometry investigation only. Rail family semantics
beyond geometry, including serialized array meanings, gameplay movement,
vehicle capacities and speeds, station behavior, and save/runtime coupling,
remain **UNKNOWN**. The OpenRepublic rail rules, catalog syntax, protocol
fields, and score are synthetic; they make no WRSR compatibility or parity
claim.
