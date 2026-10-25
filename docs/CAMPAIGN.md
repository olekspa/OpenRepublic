# Synthetic campaign

`republic_rising` is a five-mission, OpenRepublic-authored campaign that uses
the native economy, scenario fixtures, and snapshot-based objectives. The
campaign is an opt-in preset. Its mission scenarios use synthetic catalogs and
scenario rules; **WRSR parity is UNKNOWN**.

## Missions

The ordered campaign lives under `data/campaign/republic_rising/`. Its
`campaign.txt` manifest points to one briefing, one strict `scenario_score.py`
scenario JSON file, and one native objective JSON file per mission. All three
references are basenames beside the manifest. The manifest's par score is the
baseline controller tick budget used by the smoke harness. Starting treasury
is recorded per mission; the current fixtures declare zero.

| Mission | Focus | Baseline tick budget |
| --- | --- | ---: |
| `coal_to_heat` | Move coal through a border trade and depot chain to provide heat. | 1,500 |
| `build_the_chain` | Build a mill chain using active construction sites and delivered materials. | 64 |
| `growing_town` | Grow a town with residents, food, and housing services. | 500 |
| `power_and_water` | Connect and supply synthetic power and water utilities. | 100 |
| `trade_and_logistics` | Use trade, depots, and haul logistics to meet the authored targets. | 1,500 |

The campaign loader reads metadata only; objective evaluation remains the
existing pure snapshot evaluator. Starting balances, building placements,
catalog rules, and par budgets are fixture choices, not inferred original-game
values.

## Run the campaign

The desktop build can list available campaigns or start one with:

```powershell
OpenRepublic.exe --campaign list
OpenRepublic.exe --campaign republic_rising
```

The `--campaign` option selects the native economy preset, applies the first
mission's authored setup, and displays its briefing and objectives. Completing
a mission records progress in the user's local data directory, outside the
repository and Steam installation. The default progress file is
`%LOCALAPPDATA%/OpenRepublic/saves/campaign-progress.dat` on Windows and
`$HOME/.local/share/OpenRepublic/saves/campaign-progress.dat` on Linux. The
runtime writes progress through a temporary sibling file and atomically
replaces the prior file. This is OpenRepublic campaign progress, not a WRSR
save format.

## Headless baseline smoke

Build the headless economy host, then run:

```powershell
python tools/campaign_smoke.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe
```

The harness reads the ordered manifest, runs each mission in a fresh synthetic
host with the shared baseline autopilot for its par tick budget, and prints a
JSON receipt containing objective IDs, final values, targets, status, scenario
score breakdown, and deterministic hashes. `reachable` means the final
snapshot value meets that objective's target at the authored tick budget.
`not_reached` identifies a measured objective that missed its target;
`not_measured` identifies an objective metric that could not be read from the
scenario snapshot. The command exits nonzero when an objective is missed or
unmeasured. To run a shorter or longer common horizon for diagnosis, pass
`--ticks N` (1 through 100,000); that replaces each mission's authored par
budget in the run but does not alter the manifest.

The native evaluator treats `delivered_at_least` as current destination stock,
since there is no durable delivery history. The smoke follows the same proxy.
This measures current synthetic state only. C++ unit results or headless
autopilot receipts do not verify desktop presentation, human playability, or
original-game compatibility. No campaign progress file is committed to Git.
