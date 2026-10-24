# Synthetic rail transport

Rail is an opt-in OpenRepublic economy rule. A catalog may declare a vehicle
named `train` with its own positive cargo capacity and cells-per-tick speed, and
mark a building type as a `station`. A train haul must start and end at station
access cells and follow connected declared rail edges. Roads and rails are
separate networks. Truck orders continue to use road edges.

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
stock is produced. It is a comparison of these authored synthetic rules, not a
general performance claim.

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
