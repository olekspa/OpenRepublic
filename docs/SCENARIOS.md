# Synthetic economy scenarios

These fixtures exercise the OpenRepublic economy host and the rule-based
controller in `tools/republic_autopilot.py`. They are OpenRepublic-authored
synthetic inputs using the host's built-in synthetic catalog. They do not
contain game data.

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
- `buildings`: array of `{ "type": string, "x": int, "z": int }`; built-in fixtures use `farm`, `mill`, `bakery`, or `market`.
- Optional `catalog`: bounded synthetic catalog DSL text. Utility fixtures use this to define their own power generator and consumer recipes; it contains no game-derived data.
- Optional `utilityConnections`: array of `{ "resource": "power", "aBuilding": int, "bBuilding": int }`, where endpoints are zero-based indexes into the initial `buildings` array.
- Trade catalogs may declare per-resource prices and trade-post storage. Trade commands run through the baseline controller; treasury appears in utility score breakdowns only when a catalog declares trade prices.
- `score`: `{ "metric": "stored_non_raw_units" }` for inventory fixtures, or `{ "metric": "utility_multi_objective" }` for utility fixtures.

The loader and scorer are expected to reject malformed or unsupported scenario
input with a diagnostic rather than silently substituting values.

## Run a deterministic score

Build the headless economy host, then invoke the scorer with a positive tick
count and the requested scenario path. For example:

```powershell
python tools/scenario_score.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --scenario data/scenarios/full_chain.json --ticks 1000
```

On Windows generators that place executables directly under the build tree,
use the host executable's actual path. The scorer drives a fresh synthetic host
instance, applies the scenario setup, then runs the rule-based autopilot for
the requested main-step ticks. Initial buildings and roads consume one setup
tick; utility links, when present, consume a second. Autopilot placement and
trade attempts may advance additional one-tick steps. The output `tick` is the actual final
simulation tick. Inventory scenarios emit score JSON with `scoreMicroUnits` and
`scoreUnits`. Utility scenarios emit `scoreBreakdown` with measured power
throughput, unmet eligible demand, and an infrastructure-count cost proxy.
Any scenario whose catalog declares trade prices adds final `treasury` to
`scoreBreakdown`; non-trade scenarios do not get a treasury field.
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
`throughputUnits` is power consumed during the final tick and
`unmetDemandUnits` is eligible power demand minus consumption. These are catalog
units as reported by the live host; they are not micro-scaled. `costUnits` is
final buildings + roads + utility links. Cost is an explicit
infrastructure-count proxy; the current economy has no construction-price
model. Utility diagnostics measure this independent model's per-tick flows and
do not establish physical wattage, original-game topology, or parity. Neither
score is a comprehensive play-quality or profit measure. Results depend on the
authored catalog, simulation, autopilot, and tick horizon.

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

The inventory fixtures can seed `tools/llm_republic.py` and the tournament harness.
Use the exact scenario ID (the shipped filename stem) or an existing JSON path.
Pass `--scenario <id|path>` to `llm_republic.py`; setup consumes one tick, then
controller runs for `rounds × ticks` ticks and emits a final `scenarioScore`
record using the scenario's declared score format. Trade scenarios include
treasury in the breakdown only when the synthetic catalog declares prices.

The utility fixtures currently use `scenario_score.py`'s direct synthetic-catalog
runner; LLM/tournament controller hosting is not wired for custom catalogs yet.

For a tournament comparison of inventory fixtures, add `--scenario <id|path>` to
`republic_tournament.py`. Each manifest controller runs on a fresh fixture host,
and a separate rule-based autopilot baseline is added to the same table. All
rows use `scoreMicroUnits` as the exact value and `scoreUnits` for display.
World `host_args` are disallowed in scenario mode so region/checkpoint inputs
cannot silently alter the fixture. Reported final ticks can differ because the
autopilot may advance extra ticks while attempting placements or trade. Local tests use
the scripted backend only; network and installed CLI backends are not invoked.
