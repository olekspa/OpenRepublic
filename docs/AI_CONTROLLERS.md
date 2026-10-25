# AI controllers

OpenRepublic exposes the synthetic economy to local controller processes through `OpenRepublicEconomyHost` and protocol v0 JSON Lines (JSONL). The C++ economy remains authoritative: controllers observe snapshots, submit revision-bound requests, advance ticks, and poll command outcomes. This is an OpenRepublic API, not a WRSR interface. All synthetic rules and WRSR parity remain **UNKNOWN**.

## Start the host

Build `OpenRepublicEconomyHost` with the optional JSON dependency available, then start a 64 by 64 empty synthetic world as a controller:

```powershell
build\headless\src\economy_host\Release\OpenRepublicEconomyHost.exe --role controller
```

By default, the host starts as an observer. Optional arguments include `--catalog <file>` for catalog text, `--rules <economy_rules.lua>` for bounded Lua tuning, or `--region <map-dir> --content-root <dir> [--center N] [--radius M]` to seed a bounded local-content region. These modes still run the OpenRepublic simulation. The host communicates over stdin/stdout only; keep it trusted and local. Role selection is a permission profile, not authentication.

Send one UTF-8 JSON object per input line and read one response line. Each request has a non-empty string `id`, integer `protocolVersion: 0`, and string `method`. Responses echo the ID and contain `ok` plus `result`, or `ok: false` and an error object. Unknown fields, duplicate JSON keys, malformed values, and unsupported protocol versions are rejected. Every uint64 field is encoded as a decimal string; bounded IDs and coordinates are JSON integers.

Start with `hello` or `capabilities`; both return available methods/actions, limits, role, API version, and a process-local `sessionId`. For session-bound dispatchers, include that ID in every request except `hello` and `capabilities`. Read `world` to get the current decimal-string `revision`; submit mutations against that revision, then `step` and `poll` to inspect the terminal result. A queued submission is not yet proof that the requested action applied.

## Protocol methods

`hello` / `capabilities` advertise the role-aware contract. Observer and controller roles can call:

- `catalog`: resources, building types, recipes, storage, and vehicle types.
- `world`: tick, revision, hash, buildings, roads, rails, vehicles, haul orders, utility links, and command-history metadata.
- `summary`: compact totals such as stock, stalls, vehicles, and enabled population/workforce fields.
- `utility_diagnostics`: measured utility network flows after a tick has produced measurements.
- `explain`: current stall reason and explanation for a building.
- `advise`: read-only suggestions about inputs, surplus, and eligible haul retargets.
- `poll`: outcome for a command handle.
- `events`: bounded event-journal page.
- `rules_history`: accepted Lua economy rule reload history when enabled.
- `begin_checkpoint` / `read_checkpoint`: bounded chunked checkpoint export.

Controller roles can also call:

- `submit`: queue one revision-checked command.
- `create_haul_orders`: submit a batch of 1 to 64 haul orders against one revision; individual entries receive results.
- `step`: advance up to the advertised maximum ticks.
- `compact_history`: explicitly compact retained command results.
- `reload_rules`: apply a validated Lua economy tuning bundle when this host was built with Lua rules enabled.
- `trade_export` / `trade_import`: queue an explicit priced exchange at a trade post.
- `shutdown`: available only when that host was configured to allow it; the embedded desktop pipe does not expose it.

The `submit.command.kind` values and fields in protocol v0 are:

| Command kind | Required fields after `kind` | Notes |
|---|---|---|
| `place_building` | `type`, `x`, `z` | Places a completed building. |
| `place_construction_site` | `type`, `x`, `z` | Requires declared construction materials/work. |
| `remove_building` | `buildingId` | Removes the building subject to world references. |
| `build_road` | `ax`, `az`, `bx`, `bz` | Adds a valid orthogonal road segment. |
| `build_rail` | `ax`, `az`, `bx`, `bz` | Adds one adjacent rail edge. |
| `set_road_enabled` | `roadId`, `enabled` | Toggles an existing road. |
| `set_building_enabled` | `buildingId`, `enabled` | Toggles production/staffing eligibility. |
| `create_haul_order` | `source`, `destination`, `resource`, `vehicleType`; optional `mode` | `mode` is `road` (default) or `rail`. |
| `dispatch_truck` | `vehicle`, `source`, `destination`, `resource`; optional `amount` | Uses an idle depot-owned truck; omitted/zero amount requests up to capacity. |
| `cancel_haul_order` | `orderId` | Cancels an order when allowed by its state. |
| `retarget_haul_order` | `orderId`, `destination` | Changes a compatible order destination. |
| `connect_utility` / `disconnect_utility` | `resource`, `a`, `b` | Connects two live buildings for a declared utility resource. |

Trade commands use the dedicated method with `revision`, `buildingId`, `resource`, `amount`, and positive `rateCap`. They are explicit exchanges; they do not create recurring import/export orders. Read [Economy API v0](ECONOMY_API.md) for the full request/response contract, limits, session details, and checkpoint framing.

## Rule-based autopilot

`tools/republic_autopilot.py` is a deterministic controller that reads the host's catalog/world/advice, proposes supported placements, roads, hauls, trades, utility links, and depot dispatches, steps the world, and polls command results. It is useful as a synthetic baseline and scenario runner; it is not an optimal planner and does not reproduce WRSR decision-making.

Score a fixture with:

```powershell
python tools/scenario_score.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --scenario data/scenarios/coal_to_heat.json --ticks 600 --exact-ticks
```

`--scenario` accepts a shipped scenario ID/file stem or JSON path in the LLM/tournament tools. Scenario setup advances ticks, and autopilot planning may add its own command/step ticks; use the reported actual final `tick`. Construction supply fixtures often need repeated planning rounds (`--rounds`) because material stock can appear over time. See [scenarios](SCENARIOS.md) for the full fixture table and score meanings.

## LLM harness

`tools/llm_republic.py` wraps a language-model planner around the same host contract. For example:

```powershell
python tools/llm_republic.py --host build/headless/src/economy_host/Release/OpenRepublicEconomyHost.exe --backend scripted --scripted-responses C:\temp\llm-responses.json --rounds 8 --ticks 100 --scenario coal_to_heat
```

`llm-responses.json` must contain a JSON array of scripted backend responses, for example `[ { "actions": [], "notes": "inspect the current snapshot" } ]`.

Available backend choices in the documented harness are `codex`, `openai-compatible`, `claude`, and `scripted`. A model proposes JSON actions, but the Python harness validates command fields, names, IDs, coordinates, and action limits before submission. The model has no direct shell or host tools. Host validation remains final. `--fallback-advise` can use deterministic suggestions for a failed model round. `--transcript <file>` records rounds; transcripts from imported regions may include private map-derived information and should remain outside Git.

`--self-test` validates the harness locally. `--replay <jsonl>` replays recorded validated actions against a fresh host; matching results require matching initial inputs, catalog, host build, action ordering, and tick schedule. See [LLM republic](LLM_REPUBLIC.md) for backend setup and [tournament instructions](LLM_REPUBLIC.md#multiple-republics-and-tournaments).

## Scores and interpretation

Scenario metrics describe authored OpenRepublic quantities:

- `stored_non_raw_units`: final stored amount of catalog resources classified as non-raw.
- `utility_multi_objective`: reports utility throughput, unmet eligible demand, infrastructure-count proxy, and relevant utility details separately.
- `completed_construction_sites`: count of authored sites in the completed state.
- `heat_units`: stored synthetic heat for the depot fixture.
- `combined_multi_objective`: reports requested fields independently, such as fed residents, completed sites, treasury, throughput, unmet demand, and stored non-raw units.
- `rail_vs_road`: dedicated separate-route arrival-tick comparison for that fixture.

Scenario, final state, and score hashes support repeatability checks for a matching OpenRepublic build and setup. They are not WRSR fingerprints. Baseline values depend on catalog, controller, fixture, and horizon. Tournament means and sample spread describe only the measured rows/repeats; they do not establish broad controller advantage or gameplay parity. Avoid comparing unlike units or treating one metric as overall play quality.
