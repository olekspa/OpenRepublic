# Synthetic heating and water

**Status:** OpenRepublic-authored utility policy. **WRSR parity: UNKNOWN.**

Catalog authors can declare `utility heat` and `utility water`. Producers publish
finite units per tick through ordinary utility-output recipes. Consumers declare
season-independent per-tick requirements with ordinary utility-input recipes.
For example, a housing type can include `recipe retain_heat every 1 in heat:2`,
and a brewery can include `recipe brew every 1 in water:3 out beer:1`. Boilers,
water towers, pumps, housing, and factories are ordinary authored building types;
their names have no engine semantics. Connect producer and consumer buildings
with explicit `connect_utility` links (pipes are represented by these links).

The existing utility phase supplies producers before consumers, allocates whole
recipe demand deterministically, and reports generated, requested, consumed,
unused, and starved recipe values per resource component. The host's
`utility_diagnostics` method and `summary.utilityNetworks` expose the latest
measurement. Stall reasons distinguish missing heat and missing water. Utility
units are synthetic catalog units, with no physical conversion or capacity,
loss, pressure, or seasonal model.

Housing that declares capacity and has an enabled recipe consuming `heat` or
`water` retains residents only when its required utility recipe is supplied in
the tick. A shortage sets its residents to zero; the regular synthetic housing
fill and food rules can fill it again when supply returns. Housing without such
a recipe keeps the existing behavior. This is a simple policy hook, not a
migration or habitability model.

`data/scenarios/heated_housing.json` and
`data/scenarios/water_for_brewery.json` are deterministic synthetic fixtures.
Their scorer receipts include flow by utility resource. Run them with
`python tools/scenario_score.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --scenario heated_housing --ticks 16` (replace the ID for the water fixture).

**VERIFIED:** these rules are implemented in OpenRepublic source and exercised
by synthetic scenario runs.

**UNKNOWN:** the original game's `heating.bin` and `waterpipes.bin` use shared
network-file layout, but their gameplay semantics are unknown. No format,
physical behavior, or parity is inferred here.
