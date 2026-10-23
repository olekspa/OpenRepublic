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
- `buildings`: array of `{ "type": string, "x": int, "z": int }`; `type` is a built-in catalog name (`farm`, `mill`, `bakery`, or `market` in these fixtures).
- `score`: currently `{ "metric": "stored_non_raw_units" }`.

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
the requested main-step ticks. Scenario setup consumes one tick before that run;
autopilot placement attempts may advance additional one-tick steps. The output
`tick` is the actual final simulation tick. The scorer emits score JSON with
`scoreMicroUnits` and `scoreUnits`. `scenarioHash` hashes normalized scenario
JSON; `stateHash` identifies the final host state; `scoreHash` hashes the
canonical score record. These hashes
should repeat for the same normalized scenario, tick count, and
host/autopilot/catalog build; changing any of those may change them. They are
reproducibility receipts for this OpenRepublic run, not evidence of parity with
the original game.

## Metric and evidence boundary

`stored_non_raw_units` is the final stored inventory total for resources the
host's synthetic catalog classifies as non-raw, reported in whole units. It is
a point-in-time inventory score; it does not directly measure throughput,
profit, unmet demand, or quality of play. Scenario outcomes depend on the
OpenRepublic catalog, simulation rules, autopilot, and requested tick horizon.

**VERIFIED (fixture authorship and declared schema):** these files are
OpenRepublic-authored synthetic layouts, and their intended road/building
arrangements are described above. The scenario suite does not encode extracted
or proprietary game material.

**UNKNOWN:** whether these layouts, rule-based behavior, resource quantities,
score values, or any resulting hash match WRSR. No WRSR parity or compatibility
claim is made.

## LLM controllers and tournament baseline

The same fixtures can seed `tools/llm_republic.py` and the tournament harness.
Use the exact scenario ID (the shipped filename stem) or an existing JSON path.
Pass `--scenario <id|path>` to `llm_republic.py`; setup consumes one tick, then
controller runs for `rounds × ticks` ticks and emits a final `scenarioScore`
record with the same integer `stored_non_raw_units` metric as this scorer.

For a tournament comparison, add `--scenario <id|path>` to
`republic_tournament.py`. Each manifest controller runs on a fresh fixture host,
and a separate rule-based autopilot baseline is added to the same table. All
rows use `scoreMicroUnits` as the exact value and `scoreUnits` for display.
World `host_args` are disallowed in scenario mode so region/checkpoint inputs
cannot silently alter the fixture. Reported final ticks can differ because the
autopilot may advance extra ticks while attempting placements. Local tests use
the scripted backend only; network and installed CLI backends are not invoked.
