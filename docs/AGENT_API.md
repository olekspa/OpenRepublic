# OpenRepublic Agent API v0

**Status:** Implemented as an in-process C++ interface; synthetic source/build/test evidence only.

`AgentApi` is a typed gateway over the existing authoritative `sim::Simulation`. It is the first contract for structured controllers and future agent access. It does not create a second simulation, expose Wicked Engine entity handles as gameplay IDs, or define a WRSR protocol.

## Contract

- The C++ structs, enums, and method signatures in `src/agent_api/include/openrepublic/agent_api/agent_api.h` are the v0 internal schema. `ApiVersion` is `0` and describes this OpenRepublic interface only.
- Calls are in-process. There is no JSON schema, wire encoding, MCP server, local IPC, C# client, or network transport yet. A future adapter must define its own versioned serialization and preserve the same simulation validation and command lifecycle.
- `Permissions` controls queries, fixed-tick advancement, construction, and transport-plan assignment. `DiscoverCapabilities()` exposes the available actions; `QueryWorld` permission also gates entity pages and event reads.
- The class contains no synchronization. Its owner must serialize calls with simulation work.

## Queries and revisions

`QueryWorld()` returns an explicit `ErrorCode` and, when allowed, a snapshot with the current process-local `revision`, tick, entities with stable simulation IDs and inventories, the current truck plan/state, and road views. A denied query returns `PermissionDenied` and a zero/default view.

`QueryEntityPage(expectedRevision, offset, limit)` returns entities in ascending stable-ID order. A page limit must be from 1 through `MaxEntityPageSize` (32). The result carries the snapshot revision, starting offset, `nextOffset`, and `hasMore`; callers can reuse the returned revision for the next page. A changed world returns `StaleRevision`. The query uses one simulation snapshot, so its revision and page contents agree.

The revision is monotonic within the live simulation instance and is not serialized in save v7. It changes when simulation state, queued work, or result acknowledgements change. Loading a save advances the receiving instance's revision. A controller should query again after a stale-revision error.

The full-world convenience query currently returns all road views. Large-world road paging is not implemented yet.

## Command lifecycle

Submission checks the action permission and expected revision, validates the request shape, then queues the same typed simulation command used by the rest of OpenRepublic. A successful `Submission` means the command was admitted to the simulation queue and has state `Queued`; it does not mean construction or transport succeeded.

The simulation evaluates queued work at a tick boundary. `GetCommandResult(commandId)` then reports `Applied` or `Rejected`, including an entity ID or a concrete simulation failure reason when available. Results are retained until `AcknowledgeCommandResult` is called. `MaxTrackedCommands` is 64; when full, new submissions fail with `CommandTrackingFull`. A queued command cannot be acknowledged; acknowledging an unknown or incomplete result returns an explicit error.

The API caps one `Step(ticks)` request at `MaxStepTicks` (10,000). Zero ticks is a no-op; a larger request returns `StepLimitExceeded` without changing simulation state. Split longer runs so a controller can observe state and outcomes between batches.

## Event journal

`ReadEvents(afterSequence, limit)` reads the bounded process-local journal. Sequences increase monotonically for one `AgentApi` instance. The cursor is exclusive: pass the previous page's `nextCursor` to request later events. Pages are limited to `MaxEventPageSize` (32), and the journal retains the latest `MaxEventJournalEntries` (128) events. If the cursor predates the retained window, `cursorFellBehind` is true and the page reports the available sequence range. Future cursors and invalid limits return explicit errors. A new process/API instance starts a new journal; events are not saved.

Events cover successful command submission, command application or rejection, and simulation advancement. For `Step(n)` where `n > 1`, the API advances and journals the first tick, reports command outcomes produced on that tick, then advances and journals the remaining `n - 1` ticks as one event. Thus command outcomes are ordered at their actual first-tick boundary instead of appearing after the entire batch. Event records are notifications, not entity diffs; query a fresh world snapshot when details change.

## Validation evidence and limits

On 21 October 2026, the Release client build passed CTest **7/7** and the headless Release build passed **6/6**. The Agent API controller test exercises capability discovery, stable-ID queries, pagination, stale revisions, permission denial, event ordering and cursor retention, command submission/application/rejection/acknowledgement, bounded stepping, overflow-safe road validation, legacy-result adoption, save-v7 compatibility, and a synthetic Farm→Mill→Storage delivery.

These are OpenRepublic source/build/test results. No WRSR runtime, save, binary, asset, formula, or compatibility behavior is verified by this API. The API is not yet a production multi-client security boundary; future transport adapters need explicit caller identity, ownership, schema validation, and lifecycle tests.
