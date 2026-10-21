# OpenRepublic Agent API v0

**Status:** The typed C++ API and local JSONL stdio host are implemented. Current evidence is source, Release builds, CTest, and a synthetic external-controller run only.

`AgentApi` is a typed gateway over the existing authoritative `sim::Simulation`. It can own a synthetic simulation for standalone hosts/tests or attach a non-owning reference to a simulation owned by another component. The Wicked Engine desktop app now attaches to its own simulation, so UI actions, Agent API callers, and rendered state share one authoritative world. The API does not expose Wicked Engine entity handles as gameplay IDs or define a WRSR protocol.

## Contract

- The C++ structs, enums, and method signatures in `src/agent_api/include/openrepublic/agent_api/agent_api.h` are the v0 internal schema. `ApiVersion` is `0` and describes this OpenRepublic interface only.
- The typed C++ API is in-process. `OpenRepublicAgentHost` exposes it to a separate local process through newline-delimited JSON on stdin/stdout. Its protocol has a separate `protocolVersion` (`0`); it is not a WRSR protocol. The host calls the same `AgentApi` and authoritative `sim::Simulation`, without creating a second gameplay model.
- The host uses nlohmann/json 3.12.0 from the pinned Wicked Engine submodule's `Editor/json.hpp` (SPDX MIT). CMake builds the host and its tests when that header is present. Initialize the submodule for this target; a headless checkout without it still builds the simulation/content libraries but omits the host and its tests.
- The host is local stdio only. It has no listener, network transport, MCP adapter, C# client, or authentication. Each process owns its own in-memory synthetic demo world. `observer` is the default permission profile; `--role controller` enables the simulation step/construction permissions. This role flag is not caller authentication or a security boundary: anyone able to launch the local process can select controller. Keep it trusted-local and off the network.
- `Permissions` controls queries, fixed-tick advancement, construction, transport-plan assignment, and trusted in-process simulation configuration. `DiscoverCapabilities()` exposes the typed API's available actions; `QueryWorld` permission also gates snapshots, saves, entity pages, and event reads. The app's injected API grants these permissions locally; this is not an authentication boundary.
- `QuerySnapshot()` is the full-snapshot convenience read for trusted in-process clients such as the app. `QueryWorld()` and bounded pages remain the narrower controller-facing data surface.
- `SaveWorld()`, `LoadWorld()`, `SetRoadEnabled()`, and `SetSimulationRules()` are typed in-process operations. `LoadWorld()` requires the current revision and clears command tracking from the replaced timeline while preserving the API's command-handle counter. The local JSONL adapter does not expose these operations; its `world` method remains read-only for rules.
- `LookupApiCommandHandleForSimulationCommand()` is a trusted in-process presentation helper that maps a simulation-internal command ID to its opaque API handle while the API is tracking it. It returns no handle for a queued command restored from a save, because that command belongs to the loaded simulation timeline rather than a live API submission.
- The class contains no synchronization. Its owner must serialize calls with simulation work.

## Local JSONL process adapter

Launch `OpenRepublicAgentHost` with no arguments to start as an observer, or pass `--role controller` to enable commands. Each input line must be one UTF-8 JSON object with a non-empty string `id` of at most 128 bytes, integer `protocolVersion: 0`, a string `method`, and only fields supported by that method. The response echoes the ID and contains either `ok: true` with `result`, or `ok: false` with a structured `error` code/message. Diagnostics and command-line usage go to stderr; stdout is reserved for protocol responses.

Implemented methods are `hello`, `capabilities`, `world`, `entity_page`, `events`, `step`, `place_facility`, `place_storage`, `add_road_segment`, `assign_transport_plan`, `command_result`, and `acknowledge`. Both `hello` and `capabilities` return a `methods` array whose entries name a method and list the roles that may call it. Observers can query and poll command results; controllers can also advance simulation, submit gameplay commands, and acknowledge completed results. The separate `actions` array describes typed gameplay actions and does not replace the method list. Mutations still use revision checks, simulation validation, tick-boundary execution, and explicit queued/applied/rejected results. The Python example in `examples/agent_api/drive_logistics.py` exercises the protocol through a child process, including default observer denial and delivery to a player-placed Storage.

The JSON adapter caps a request line at 64 KiB, a process at 8,192 requests, nesting at 16 levels, and parsed structure at 4,096 nodes. An overlong line returns `request_too_large` and terminates the host rather than draining unbounded input. Step, entity-page, and event-page bounds are also reported by `hello`. All `uint64_t` values on the wire use decimal strings; bounded IDs and coordinates use JSON integers. Unknown request fields, duplicate keys, unsupported protocol versions, malformed JSON, and invalid types are rejected before gameplay API calls.

Build with the pinned submodule initialized, then run `ctest`; if Python 3 is available at configure time, CTest also starts the external-controller acceptance. For a direct manual run from the repository root:

```powershell
python examples/agent_api/drive_logistics.py build/openrepublic-vs2022/src/agent_host/Release/OpenRepublicAgentHost.exe
```

The example is a local synthetic demonstration, not an unattended service or a production multi-client boundary.

## Queries and revisions

`QueryWorld()` returns an explicit `ErrorCode` and, when allowed, a snapshot with the current process-local `revision`, tick, entities with stable simulation IDs and inventories, the current truck plan/state, road views, the active `SimulationRules`, and the simulation's cumulative `deliveredFlourTotal`. These production fields come from the same authoritative simulation snapshot as the rest of the world view. They expose OpenRepublic's current synthetic rules and outcome counters; they do not claim to reproduce WRSR formulas. A denied query returns `PermissionDenied` and a zero/default view, including zeroed production telemetry.

The JSONL `world` result groups these fields under `production`. `deliveredFlourTotal` is a decimal string because it is a `uint64_t`; the three bounded rule values are JSON integers:

```json
{"production":{"deliveredFlourTotal":"0","rules":{"farmGrainPerHarvest":2,"farmHarvestIntervalTicks":2,"millGrainPerTick":1}}}
```

The Python controller validates the decimal-string representation and checks that cumulative delivery advances after its route runs. Controllers can observe active rule values but cannot change them through the local JSONL Agent API v0; rule updates are currently limited to the trusted in-process configuration method.

`QueryEntityPage(expectedRevision, offset, limit)` returns entities in ascending stable-ID order. A page limit must be from 1 through `MaxEntityPageSize` (32). The result carries the snapshot revision, starting offset, `nextOffset`, and `hasMore`; callers can reuse the returned revision for the next page. A changed world returns `StaleRevision`. The query uses one simulation snapshot, so its revision and page contents agree.

The revision is monotonic within the live simulation instance and is not serialized in save v7. It changes when simulation state, queued work, or result acknowledgements change. Loading a save advances the receiving instance's revision. A controller should query again after a stale-revision error.

The full-world convenience query currently returns all road views. Large-world road paging is not implemented yet.

## Command lifecycle

Submission checks the action permission and expected revision, validates the request shape, then queues the same typed simulation command used by the rest of OpenRepublic. A successful `Submission` means the command was admitted to the simulation queue and has state `Queued`; it does not mean construction or transport succeeded.

The simulation evaluates queued work at a tick boundary. `GetCommandResult(commandId)` then reports `Applied` or `Rejected`, including an entity ID or a concrete simulation failure reason when available. `commandId` is an opaque API handle, independent of the simulation's internal command ID. Handles increase for the lifetime of one `AgentApi` instance and are not reset by `LoadWorld()`, so a stale handle from a replaced timeline cannot name a later command even when the loaded simulation reuses an internal ID. Results are retained until `AcknowledgeCommandResult` is called. `MaxTrackedCommands` is 64; when full, new submissions fail with `CommandTrackingFull`. If the 64-bit API handle space is exhausted, a submission fails before it reaches the simulation with `CommandIdExhausted`. A queued command cannot be acknowledged; acknowledging an unknown or incomplete result returns an explicit error.

The API caps one `Step(ticks)` request at `MaxStepTicks` (10,000). Zero ticks is a no-op; a larger request returns `StepLimitExceeded` without changing simulation state. Split longer runs so a controller can observe state and outcomes between batches.

## Event journal

`ReadEvents(afterSequence, limit)` reads the bounded process-local journal. Sequences increase monotonically for one `AgentApi` instance. The cursor is exclusive: pass the previous page's `nextCursor` to request later events. Pages are limited to `MaxEventPageSize` (32), and the journal retains the latest `MaxEventJournalEntries` (128) events. If the cursor predates the retained window, `cursorFellBehind` is true and the page reports the available sequence range. Future cursors and invalid limits return explicit errors. A new process/API instance starts a new journal; events are not saved.

Events cover successful command submission, command application or rejection, simulation advancement, in-process configuration, and successful world loads. For `Step(n)` where `n > 1`, the API advances and journals the first tick, reports command outcomes produced on that tick, then advances and journals the remaining `n - 1` ticks as one event. Thus command outcomes are ordered at their actual first-tick boundary instead of appearing after the entire batch. Event records are notifications, not entity diffs; query a fresh world snapshot when details change.

## Validation evidence and limits

At the Agent API v0 publication checkpoint on 21 October 2026, the Release client build passed CTest **10/10** and the headless Release build passed **9/9**. The current full repository suites also include the bounded NMF header-profile test and pass **11/11** client and **10/10** headless. The shared-simulation increment adds source/build/test evidence that the desktop app injects its authoritative `Simulation` into the API, routes UI simulation commands and snapshot reads through that API, and uses typed API calls for rules, road state, and save/load. Tests verify injected ownership, state changes visible through both the API and owner, revision checks, permission checks, save/load replacement, and configuration. Follow-up coverage verifies API command handles remain opaque across adoption and are not reused when loading a save whose simulation command IDs can recur. The remaining suites cover capability discovery, stable-ID queries, production telemetry, pagination, event ordering and retention, command lifecycle, bounded stepping, overflow-safe road validation, legacy-result adoption, and Farm→Mill→Storage delivery. The process-level controller proves the wire protocol can build roads and player Storage, assign the plan, inspect results/events, observe an increasing cumulative delivery count, and receive flour by tick 17.

These are OpenRepublic source/build/test results. No WRSR runtime, save, binary, asset, formula, or compatibility behavior is verified by this API. The local role flag is not authentication, and the host must not be exposed over a network. Any future multi-client transport needs explicit caller identity, ownership, schema validation, resource controls, and lifecycle tests.
