# Synthetic economy scenarios

These fixtures exercise the OpenRepublic economy host and the rule-based
controller in `tools/republic_autopilot.py`. They are OpenRepublic-authored
synthetic inputs using the built-in catalog or a fixture-owned custom catalog.
They do not contain game data.

## Fixtures

| ID | Layout | Intended condition |
| --- | --- | --- |
| `grain_shortage` | Mill, bakery, and market on one connected road; no farm | The chain has no local grain source. |
| `road_gap` | Farm and mill have access to separate road fragments | Their access cells are disconnected. |
| `flour_oversupply` | Connected farm and mill; no bakery or downstream flour consumer | Flour has no downstream consumer in this layout. |
| `full_chain` | Connected farm, mill, bakery, and market | Complete synthetic grain-to-bread chain. |
| `power_deficit` | One generator linked through two powered mines | Demand exceeds finite connected power supply. |
| `unconnected_consumer` | Generator and powered mine with no utility link | The consumer cannot use supply in the generator's separate component. |
| `overbuilt_generator` | High output generator linked to one powered mine | Generator supply exceeds demand; unused supply is discarded. |
| `powered_chain_link_gap` | Generator, powered mine, and smelter on connected roads; only generator and mine are power-linked | The baseline cannot add the power link needed by the smelter. |
| `powered_chain_road_and_link_gap` | Generator, powered mine, and smelter on separate road fragments; only generator and mine are power-linked | The chain needs both a road connection and a power link to the smelter. |
| `import_missing_input` | Grain export post and coal-import mill | Exports earn treasury funds, then imports coal for a stalled input recipe. |
| `export_surplus_for_power` | Grain export post and coal-fueled generator | Grain surplus funds coal imports; final-tick power flow is scored. |
| `understaffed_mine` | Five-capacity housing and a ten-worker mine | OpenRepublic-authored synthetic population supplies half the mine's staffing requirement. |
| `build_a_mill` | One mill construction site with no material source | The site requires planks, but the baseline has no stocked source to haul from. |
| `build_a_mill_with_supply` | Farm, sawmill and one mill construction site on one road | The farm produces timber, the sawmill makes planks, and the autopilot serves and completes the site. |
| `build_two_mills_with_supply` | Farm, sawmill and two mill construction sites on one road | The same authored planks chain supplies two sites; score reports completed-site count and each site's final state. |
| `growth_town` | Four-capacity home, farm→mill→bakery chain, grain exporter and market site | Residents consume bakery bread; grain exports fund imported planks and bricks, which the autopilot hauls to the market site. The receipt reports fed residents, completed sites and treasury separately. |
| `power_and_build` | Generator, powered mine, smelter and timber-fed `power_line` site | The site produces wire after construction; the smelter requires wire, ore and connected power. The receipt reports site completion and final-tick utility throughput/deficit. The power links are explicit fixture connections; the `power_line` site is a synthetic production prerequisite. |
| `austerity` | Grain trade post, local farm/mill/bakery chain and second mill site | Limited exports fund imports for both the trade post's flour recipe and site materials. At short horizons, the baseline can spend on inputs while the second mill remains waiting for materials; longer runs may eventually complete it. |
| `rail_vs_road` | Independent 20-edge road and rail routes, two producer/destination pairs, and one train | The scorer moves 1,000 grain units by truck and train, records destination arrival ticks, and requires the rail run to finish first. See [RAIL.md](RAIL.md) for synthetic-rule boundaries and the explicit-transfer limitation. |

The building coordinates are origins on the host grid. Each building's access
cell is one cell north (`x`, `z - 1`); the fixtures place that cell on a road.
Road entries are adjacent unit segments with integer endpoint coordinates. The
connected layouts form one continuous route. `road_gap` deliberately has no
road edge between its two components.

## Scenario JSON schema

A scenario is one JSON object with these required fields:

- `schemaVersion`: currently `1`.
- `id`: stable lowercase identifier; the supplied IDs match their filenames.
- `name` and `description`: human-readable fixture metadata.
- `roads`: array of `{ "a": {"x": int, "z": int}, "b": {"x": int, "z": int} }` unit road segments.
- Optional `rails`: array of the same adjacent-cell shape for declared rail edges. Rail fixtures use the opt-in `build_rail` host command.
- `buildings`: array of `{ "type": string, "x": int, "z": int }`; built-in fixtures use `farm`, `mill`, `bakery`, or `market`.
- Optional `catalog`: bounded synthetic catalog DSL text. Utility fixtures use this to define their own power generator and consumer recipes; it contains no game-derived data. Population fixtures may declare `housing`, `workers`, and optional `food` rules as documented in [POPULATION.md](POPULATION.md).
- Optional `utilityConnections`: array of `{ "resource": "<declared utility>", "aBuilding": int, "bBuilding": int }`, where endpoints are zero-based indexes into the initial `buildings` array.
- Trade catalogs may declare per-resource prices and trade-post storage. Trade commands run through the baseline controller; treasury appears in utility score breakdowns only when a catalog declares trade prices.
- `score`: `{ "metric": "stored_non_raw_units" }` for inventory fixtures, `{ "metric": "utility_multi_objective" }` for utility fixtures, `{ "metric": "completed_construction_sites" }` for construction fixtures, `{ "metric": "combined_multi_objective", "objectives": [...] }` for combined fixtures, or the dedicated `rail_vs_road` comparison fields in its fixture. Supported combined fields are `fedResidents`, `completedSites`, `treasury`, `throughputUnits`, `unmetDemandUnits`, and `storedNonRawUnits`; each remains a separate objective in the score receipt.

The loader and scorer are expected to reject malformed or unsupported scenario
input with a diagnostic rather than silently substituting values.

## Run a deterministic score

Build the headless economy host, then invoke the scorer with a positive tick
count and the requested scenario path. For example:

```powershell
python tools/scenario_score.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --scenario data/scenarios/full_chain.json --ticks 1000
```

Construction supply scenarios need repeated planning as production generates
material stock. For a fixed one-tick planning cadence, run for example:

```powershell
python tools/scenario_score.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --scenario data/scenarios/build_a_mill_with_supply.json --ticks 64 --rounds 64 --exact-ticks
```

On Windows generators that place executables directly under the build tree,
use the host executable's actual path. The scorer drives a fresh synthetic host
instance, applies the scenario setup, then runs the rule-based autopilot for
the requested main-step ticks. Initial buildings and roads consume one setup
tick; utility links, when present, consume a second. Autopilot placement and
trade attempts may advance additional one-tick steps. The output `tick` is the actual final
simulation tick; `--exact-ticks` caps each planning round at its scheduled tick.
Inventory scenarios emit score JSON with `scoreMicroUnits` and
`scoreUnits`. Utility scenarios emit `scoreBreakdown` with measured throughput
and unmet eligible demand for the primary utility, per-resource `utilityFlows`,
and an infrastructure-count cost proxy. Power remains the primary utility when
present; otherwise the first reported utility is primary. Resource-specific
flows are retained separately so unlike utility units are not added together.
Any scenario whose catalog declares trade prices adds final `treasury` to
`scoreBreakdown`; non-trade scenarios do not get a treasury field.
Construction scenarios score one unit per site whose final state is `done` and
include `completedSites`, `totalSites`, and a `siteStates` list in
`scoreBreakdown`. Their deterministic hashes include the normalized scenario,
final host state, and site-state score receipt.
`scenarioHash` hashes normalized scenario
JSON; `stateHash` identifies the final host state; `scoreHash` hashes the
canonical score record. These hashes
should repeat for the same normalized scenario, tick count, and
host/autopilot/catalog build; changing any of those may change them. They are
reproducibility receipts for this OpenRepublic run, not evidence of parity with
the original game.

## Metric and evidence boundary

`stored_non_raw_units` is the final stored inventory total for resources the
host's synthetic catalog classifies as non-raw, reported in whole units, with
the exact value retained in `scoreMicroUnits`. For utility scenarios,
`throughputUnits` is primary-utility consumption during the final tick and
`unmetDemandUnits` is primary-utility eligible demand minus consumption. Each
`utilityFlows` entry reports the generated, requested, consumed, and unmet
catalog units for one resource. These are reported by the live host and are not
micro-scaled. `costUnits` is
final buildings + roads + utility links. Cost is an explicit
infrastructure-count proxy; the current economy has no construction-price
model. Utility diagnostics measure this independent model's per-tick flows and
do not establish physical wattage, original-game topology, or parity. Neither
score is a comprehensive play-quality or profit measure. Results depend on the
authored catalog, simulation, autopilot, and tick horizon.
`completed_construction_sites` counts completed authored synthetic sites; it is
not a cost, profit, or construction-material valuation. In combined receipts,
`fedResidents` is the final synthetic population total from the host summary,
`treasury` is the final authored balance, `throughputUnits` and
`unmetDemandUnits` are final-tick utility values, and `storedNonRawUnits` is the
final stored non-raw inventory total. The objectives are reported independently;
the receipt does not collapse unlike units into an invented aggregate score.

**VERIFIED (fixture authorship and declared schema):** these files are
OpenRepublic-authored synthetic layouts, and their intended road/building
arrangements are described above. The scenario suite does not encode extracted
or proprietary game material.

**UNKNOWN:** whether these layouts, rule-based behavior, resource quantities,
score values, or any resulting hash match WRSR. No WRSR parity or compatibility
claim is made.

## Baseline scores across the fixture suite

The following **OBSERVED** results came from the rule-based baseline autopilot
in this checkout, using the headless Release host and 16 requested autopilot
ticks per scenario. The output score is the inventory total in whole units, or
`throughputUnits / unmetDemandUnits` with the infrastructure-count proxy for
utility scenarios. Inventory totals can be fractional because the exact host
inventory is stored in micro-units. Final ticks vary when the autopilot advances
extra ticks while attempting orders or placements.

| Scenario | Metric | Baseline score | Final tick |
| --- | --- | ---: | ---: |
| `flour_oversupply` | Stored non-raw units | 0.000006 (6 micro-units) | 17 |
| `full_chain` | Stored non-raw units | 0.000004 (4 micro-units) | 17 |
| `grain_shortage` | Stored non-raw units | 0.000009 (9 micro-units) | 20 |
| `road_gap` | Stored non-raw units | 0 (0 micro-units) | 17 |
| `power_deficit` | Throughput / unmet; cost | 2 / 2; cost 5 | 18 |
| `unconnected_consumer` | Throughput / unmet; cost | 0 / 2; cost 2 | 17 |
| `overbuilt_generator` | Throughput / unmet; cost | 2 / 0; cost 3 | 18 |
| `powered_chain_link_gap` | Throughput / unmet; cost | 2 / 2; cost 12 | 18 |
| `powered_chain_road_and_link_gap` | Throughput / unmet; cost | 2 / 0; cost 7 | 18 |
| `import_missing_input` | Stored non-raw; treasury | 0.000018 (18 micro-units); treasury 0 | 19 |
| `export_surplus_for_power` | Throughput / unmet; cost; treasury | 1 / 0; cost 10; treasury 1 | 20 |
| `growth_town` | Fed residents; completed sites; treasury | 2; 1/1; 28 | 17 |
| `power_and_build` | Completed sites; power throughput / unmet | 1/1; 4 / 0 | 18 |
| `austerity` | Completed sites; treasury; stored non-raw units | 0/1; 6; 0.000027 | 17 |

The combined baseline results used 16 requested controller ticks,
`--exact-ticks`, and 16 planning rounds. **OBSERVED (synthetic host, 24 October
2026):** growth_town completed its market and exported grain to fund
trade-material imports, but the final population total was 2 of the four
authored housing places. The power_and_build baseline completed its power-line
production prerequisite and ended with power throughput 4 and no eligible
unmet demand. The austerity baseline queued trade and haul actions but left the
second mill waiting for materials at tick 17. A separate 64-tick run completed
the site, so this is a short-horizon budget stall rather than a permanent
deadlock. Repeated 64-tick runs of each combined ID produced identical
`scoreHash` and `stateHash` values. These observations apply only to the current
OpenRepublic host, authored catalogs, rule-based controller and horizons.

The `rail_vs_road` receipt is produced by its dedicated deterministic scorer
path, not by the baseline autopilot. It records elapsed ticks until each
destination contains the declared 1,000 grain units and the final world hash.
This is a synthetic fixture result; it is not a speed, capacity, route, or
parity claim about WRSR. Run it with the same command form as other scenarios,
for example `--scenario rail_vs_road --ticks 5000`.

The last two scenarios leave their initial utility-link configuration unchanged;
the road-and-link fixture also retains its three disconnected road segments.
The autopilot creates one ore-hauling order in each. In the connected-road
fixture, eligible smelter demand remains unmet because the autopilot does not
add a power link. In the road-and-link fixture, the ore remains unreachable
across disconnected roads, so the smelter has no eligible power demand yet.
These utility scores report last-tick power flow rather than full chain output.
The fixtures show infrastructure gaps that the current autopilot does not
repair, not general controller competence.

## LLM controllers and tournament baseline

The inventory and combined fixtures can seed `tools/llm_republic.py` and the tournament harness.
Use the exact scenario ID (the shipped filename stem) or an existing JSON path.
Pass `--scenario <id|path>` to `llm_republic.py`; setup consumes one tick, then
controller runs for `rounds × ticks` ticks and emits a final `scenarioScore`
record using the scenario's declared score format. Trade scenarios include
treasury in the breakdown only when the synthetic catalog declares prices.

For a tournament comparison of inventory or combined fixtures, add `--scenario <id|path>` to
`republic_tournament.py`. Each manifest controller runs on a fresh fixture host,
and a separate rule-based autopilot baseline is added to the same table. All
inventory rows use `scoreMicroUnits` as the exact value and `scoreUnits` for
display; combined rows retain their separate `scoreBreakdown` objectives and do
not collapse unlike units into one scalar.
World `host_args` are disallowed in scenario mode so region/checkpoint inputs
cannot silently alter the fixture. Reported final ticks can differ because the
autopilot may advance extra ticks while attempting placements or trade. Local tests use
the scripted backend only; network and installed CLI backends are not invoked.

# Construction site scenario baseline

The scorer now makes up to 64 planning rounds by default for scenarios with
construction sites (fixtures without a site retain their one-round default).
Repeated planning is required because the first pass can correctly
haul timber to a sawmill before that production chain has made the planks a site
needs. The autopilot then plans later site deliveries when planks are stocked.
This is an OpenRepublic controller policy; it does not imply WRSR behavior.

**VERIFIED (synthetic host, 24 October 2026):** both supply scenarios complete
within a 64-controller-tick budget using the default scorer planning cadence:
`build_a_mill_with_supply` completes 1/1 sites at tick 10 and
`build_two_mills_with_supply` completes 2/2 sites at tick 16.
`scoreBreakdown.completionTick` records the first planning-round boundary at
which all fixture sites are done; with the documented 64-tick budget, each
round advances one tick, so these are the observed completion ticks. Repeated
runs produce identical state and score hashes. The 4,000-tick default CLI run
also completes both scenarios; it first observes both sites complete at tick
161, the end of its first 80-tick planning interval. That long-run receipt value
is an observation boundary, not the exact completion tick.

Reproduce the bounded baseline:

```powershell
python tools/scenario_score.py --host <OpenRepublicEconomyHost.exe> --scenario data/scenarios/build_a_mill_with_supply.json --ticks 64 --exact-ticks
python tools/scenario_score.py --host <OpenRepublicEconomyHost.exe> --scenario data/scenarios/build_two_mills_with_supply.json --ticks 64 --exact-ticks
```
