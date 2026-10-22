# OpenRepublic Economy API v0

`OpenRepublicEconomyApi` exposes the deterministic `economy::World` and immutable
`economy::Catalog` through typed calls. `OpenRepublicEconomyHost` provides a
trusted-local JSONL adapter. The API and protocol versions are both `0` and are
OpenRepublic interfaces; they make no compatibility claim about WRSR.

The host reuses `third_party/WickedEngine/Editor/json.hpp`. CMake exposes the
containing directory as the `OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR` cache path.
It defaults to this checkout's initialized Wicked Engine submodule. Set the cache
path explicitly when the header is stored elsewhere.

## Desktop app-owned map-region mode

When `OpenRepublic.exe` is launched with `--map-region` and without `--economy-url`,
the app imports its bounded catalog, accepted region buildings, and decoded roads
into an app-owned `economy::World`. Human inspection and mutations call the same
typed `EconomyApi` in process. Rendered instances carry the index of their source
placement in the exact loaded region vector; region-import results map that key to
the created economy building ID. This mapping is exact for one load but is not a
durable identity across package revisions. Skipped/unsupported placements do not
receive a building mapping.

The app has an opt-in Windows named-pipe controller endpoint for this same
in-process instance.
`OpenRepublicEconomyHost` remains a standalone host with its own world. Passing
`--economy-url` selects the legacy desktop bridge to that separate world; it does
not attach an external controller to the app-owned world. The endpoint's
source/build and local runtime wiring have been exercised. A later synthetic/open
GUI run showed visible geometry, picking, toggle/step and Farm-to-Mill production.
The final presentation follow-up remains open; see the fixture path below and
the [current access/UI report](DEVELOPMENT_REPORT_2026-10-22_ACCESS_AND_UI.md).

### Synthetic/open graphical-acceptance fixture

Run `OpenRepublic.exe --synthetic-region-fixture` for a self-contained
OpenRepublic-authored Farm/Mill/Bakery scene, a visible primitive road, and a
matching economy input. It accepts no WRSR paths and cannot be combined with map,
probe, capture, or economy-endpoint options. The scene exists to exercise the same
app-owned region selection and economy controls; it is not a WRSR-content showcase
or a compatibility claim. A later live run did not reproduce the earlier black
viewport: selection, disabling/re-enabling with stepping, haul creation and mill
production were observed. New `Step 1 tick` and retained `Step 24 ticks` controls
use the same API. Wrapped status text, native synthetic integer quantities and
smaller road-aligned markers have live synthetic GUI observations: farm grain
20/20 at tick 26, truck movement at single ticks 27/28, mill grain/flour 8/8
at tick 52, and overflow wheel/scrollbar interaction in a short window without
losing selection. No apparent camera zoom occurred from panel scrolling; this
is a bounded visual observation, not instrumented input/renderer verification.

### In-app trusted-local controller endpoint

On Windows, add `--economy-pipe-name <name>` to a map-region launch to create a
named pipe at `\\.\pipe\<name>` inside the desktop process:

```text
OpenRepublic.exe --content-root <content-dir> --map-package <map-dir> --map-region --map-radius <meters> --economy-pipe-name OpenRepublicController
```

The name must be 1–64 ASCII letters, digits, period, underscore, or dash. The
option requires `--map-package` and `--map-region`; it cannot be combined with
`--economy-url`, a content probe, or a capture mode. Without the option, the
endpoint is not created. Its embedded `Dispatcher` borrows the app's existing
`EconomyApi`, so the UI and the pipe share the same catalog, world revision,
command handles, event journal, and queued commands. Pipe requests are framed
on a worker and dispatched serially from the application's update thread. The
app does not expose embedded `shutdown`; closing the app owns endpoint lifetime.

The pipe uses the process token's default security descriptor and requests
`PIPE_REJECT_REMOTE_CLIENTS`. It does not authenticate individual clients:
any local process with access under that descriptor can submit controller
commands. The service handles one synchronous client at a time. A connected
client that leaves an incomplete line open occupies that connection until it
disconnects or the app closes; there is no idle-read timeout in this increment.
Request lines are limited to 64 KiB and transport responses to less than 1 MiB.
The protocol is OpenRepublic JSONL v0, not a WRSR interface.

After `hello`, use the returned `sessionId` on every follow-up request except
`hello` and `capabilities`; the embedded dispatcher rejects missing and stale
session IDs. `submit` requires the decimal-string `revision` from a recent `world`
query. `step` is controller-only and session-bound but advances directly without
a revision argument, matching protocol v0. See the methods and examples below.

## Host and framing

The host reads one UTF-8 JSON object per line from stdin and writes one JSON
response per line to stdout. Start without arguments as an observer, or use
`--role controller`; an optional `--catalog <file>` loads strict catalog v1
text instead of `MakeSyntheticCatalog()`. The process owns a 64 by 64 empty
world. Role selection is not authentication and this stdio process must remain
trusted and local.

Every request has `id` (1 to 128 bytes), `protocolVersion: 0`, and `method`.
Responses echo `id`, include `protocolVersion` and the process-local `sessionId`,
and contain `ok` with `result`, or `ok: false` and an `error` code/message.
After `hello`, clients may include that `sessionId` on each request; a mismatch
returns `stale_session` before the request is dispatched. This binds mutations
and command-handle polls to the session that produced the client snapshot. The
field is a replacement marker, not authentication; clients that omit it keep
the unbound protocol-v0 behavior. Unknown fields, duplicate keys,
invalid types, malformed JSON, and unsupported versions are rejected before
calling the API. Request lines are limited to 64 KiB; overlong input returns
`request_too_large` and exits with status 5. The process accepts at most
10,000,000 requests, JSON nesting is limited to 16 and parsed structure to
4096 nodes. The larger finite request budget supports long-lived loopback UI
sessions that poll world state; it does not remove the per-request bounds.
Every `uint64_t` is encoded as a decimal string; bounded IDs and coordinates
are JSON integers.

`hello` and `capabilities` return the process-local `sessionId` in both the
response envelope and result, role-aware `methods`, available `actions`, limits,
and the uint64 encoding. The session ID stays stable for one dispatcher
instance and changes when a new host instance starts. Every response carries
the session ID. Observers may call `hello`, `capabilities`,
`catalog`, `world`, `explain`, `advise`, `rules_history`, `poll`, `events`,
`begin_checkpoint`, and `read_checkpoint`. Controllers may also call
`submit`, `step`, `compact_history`, and `shutdown`. `world` currently returns all four sorted
collections; the typed API provides revision-checked pages of up to 32 items
for `buildings`, `roads`, `vehicles`, or `orders`.

## Methods

- `catalog`: returns resource names, building footprints/storage/recipes, and
  vehicle capacity and speed. Recipe and storage amounts are decimal strings.
- `world`: returns revision, tick, state hash, buildings with inventories and
  stall text, roads, vehicles, haul orders and versioned command-history metadata.
- `explain`: requires integer `buildingId`; returns current stall reason and
  explanation text.
- `advise`: read-only for observers and controllers. Returns the snapshot's
  decimal-string `revision` and `tick`, the existing `unservedInputs` and
  `unconsumedSurplus` lists, and additive `retargets` / `retargetsTruncated`
  fields. The input-link heuristic still reads the first recipe and ranks
  producers by Manhattan distance; it does not validate those new-link routes.
  The separate retarget planner considers waiting, loaded `destination_full`
  orders. It searches enabled unit-road edges from the truck's actual cell to
  compatible storage access cells, ranks by route length then building ID,
  and excludes the original source/destination and duplicate haul tuples.
  Each candidate must fit the whole cargo after current inventory, all loaded
  inbound cargo, and earlier suggested cargo are reserved. Reservations are
  conservative: cargo remains reserved at its current destination even if
  that order is also advised to retarget. A stale full-storage failure is
  skipped when the current destination has room after other inbound cargo.
  Up to 32 suggestions are emitted in order-ID order. `retargetsTruncated`
  means later eligible orders were left unexamined after that output limit;
  those orders may have no feasible candidate. An infeasible early order does
  not consume the output budget.

  A suggestion contains integer `orderId`, `vehicleId`, `destination`,
  `distanceCells`, string `resource`, decimal-string `cargo`, and a ready
  `command`, for example
  `{"kind":"retarget_haul_order","orderId":1,"destination":4}`.
  Submit against the advice revision, step, and poll every handle. Routes and
  room are snapshot estimates: production, queued commands and later road
  changes can still make a truck wait. The adviser does not mutate the world
  or reserve capacity in the simulation, and a retarget submission retains
  the core's existing endpoint validation rather than enforcing this heuristic.
- `submit`: requires decimal-string `revision` and a tagged `command` object.
  Kinds are `place_building` (`type`, `x`, `z`), `remove_building`
  (`buildingId`), `build_road` (`ax`, `az`, `bx`, `bz`), `set_road_enabled`
  (`roadId`, `enabled`), `set_building_enabled` (`buildingId`, `enabled`),
  `create_haul_order` (`source`, `destination`, `resource`, `vehicleType`), and
  `cancel_haul_order` (`orderId`), and `retarget_haul_order` (`orderId`,
  `destination`). Names resolve exactly through the catalog.
  A successful submission returns an opaque decimal-string `handle` and
  `queued` state; it does not guarantee eventual command acceptance.
- `poll`: requires decimal-string `handle`; reports command state, failure,
  message, and any resulting building, road, or order ID.
- `step`: controller-only, with integer `ticks` from 0 through 100000.
- `events`: requires decimal-string `afterSequence` and integer `limit` from 1
  through 32. The 128-entry journal reports `cursorFellBehind` if retention
  overtakes the requested cursor.
- `shutdown`: controller-only graceful process exit.

## Example session

```json
{"id":"1","protocolVersion":0,"method":"hello"}
{"id":"2","protocolVersion":0,"method":"catalog"}
{"id":"3","protocolVersion":0,"method":"world"}
{"id":"4","protocolVersion":0,"method":"submit","revision":"0","command":{"kind":"place_building","type":"farm","x":2,"z":2}}
{"id":"5","protocolVersion":0,"method":"step","ticks":1}
{"id":"6","protocolVersion":0,"method":"poll","handle":"1"}
```

The simulation is synthetic OpenRepublic content. Its recipes and outcomes are
not evidence of original-game formulas or behavior.

## Cargo-preserving haul replanning

Controllers can change an existing order's destination without replacing its
order ID, truck ID, source, resource, vehicle type, or onboard cargo:

```json
{"id":"redirect-1","protocolVersion":0,"method":"submit","revision":"42","command":{"kind":"retarget_haul_order","orderId":1,"destination":3}}
```

Use the current revision from `world`, then `step` and `poll` the returned handle.
Retargeting is applied in submission order at the tick boundary. A loaded truck
routes from its current cell to the new destination. An empty truck first returns
to its original source before loading. Retargeting to the current destination is
an accepted no-op. Unknown orders, missing or same-source destinations, absent
resource storage, and duplicate source/destination/resource orders are rejected.
An unreachable destination is accepted and reported as `no_route`; cargo remains
aboard while the order retries connectivity. A full destination leaves the truck
waiting with its remaining cargo, which can be retargeted again.

Cancelling an order with onboard cargo now produces a terminal rejected command
with failure `cargo_in_transit`. Deliver or redirect the cargo before cancelling.
Empty orders, including trucks returning to their source, can be cancelled.
Queue admission is distinct from command acceptance; inspect the poll result.

The economy world writer now emits `OPENREPUBLIC_ECONOMY_WORLD 2`, retaining the
existing record layout and adding the retarget command kind and cargo failure.
The reader accepts v1 saves and gates the new enum values to v2. Pending retargets
and loaded retargeted trucks preserve state hashes through save/reload on the
same build/platform. The separate legacy `src/sim` save schema is unchanged.
Persisted order cargo must fit its vehicle capacity, truck cells must be in the
grid, and loaded trucks must be on a destination leg. The subsequent integrity
increment validates typed counters, ID ordering/history, boolean domains,
queued-result associations and execution budget transactionally; see below.

Catalog replacement retains the ordered resource IDs of every recipe input and
output. Amounts and intervals remain tunable. Reducing storage below its inventory
or vehicle capacity below onboard cargo rejects the entire swap transactionally.

## Native economy save integrity and limits

`SaveWorld` emits v2 while full command history is retained and v3 after explicit
compaction, up to `MaxWorldSaveBytes` (1 MiB). It returns an empty string when
output would exceed that limit. Check the result before writing.
The reader accepts valid v1/v2/v3 state within that byte limit, including native
sections with more than 1,024 records. This contract belongs to the C++ economy;
the host does not expose save/load methods in this increment.

World dimensions must be 1..`INT32_MAX`; the constructor throws
`std::invalid_argument` otherwise. Load validates typed numeric fields,
canonical entity ID ordering, nonzero/monotone counters, retained command
history, queued-result suffix coherence, inventory/progress/cargo bounds, and
tick/revision budgets. Failed loads preserve the target world. Valid pending
commands keep their application-time semantics, including forward references
and payloads that will be rejected at the next tick.

Queue admission reserves the next tick plus all pending command applications.
If that cannot fit, command methods return `0` without changing state; the typed
API and JSONL submit result report `command_queue_exhausted` with no handle or
queue event. `step` and rule reloads cannot wrap counters or consume execution
space reserved for pending work. Entity-ID exhaustion rejects at execution.
See the [save-integrity report](DEVELOPMENT_REPORT_2026-10-22_SAVE_INTEGRITY.md)
for evidence and remaining catalog/history/large-world boundaries.

## Building access and road reservations

Placement distinguishes footprint bounds (`out_of_bounds`), footprint overlap
(`building_collision`), unavailable/covered access (`access_cell_unavailable`),
and footprints covering existing roads (`road_on_building`). Disabled roads keep
their cell reservations. An access cell may contain a road; disconnected building
placement remains permitted. Runtime placement and native save loading enforce
the same layout rules, with transactional rejection of conflicting saved layouts.

Region import preserves source-row order and accepted-only source keys. Its
`skippedAccessConflict` count is separate from footprint overlap, and the
`maxDiagnosticNotes` option defaults to 32 while counters remain exact. These
are native grid rules over provisional catalog footprints, not recovered WRSR
access/footprint semantics.


## Native catalog-bound checkpoints

The typed `ExportCheckpoint(expectedRevision)` is query-only and leaves world state,
queued work, handles and events unchanged. It returns `stale_revision` or
`persistence_failed` with a diagnostic on failure. `economy/checkpoint.h` also
provides transactional owned `RestoreCheckpoint` and exact expected-catalog
`LoadCheckpoint`; the latter rejects any canonical catalog mismatch.

Checkpoint v1 embeds canonical catalog bytes followed by the native world save:

```text
OPENREPUBLIC_ECONOMY_CHECKPOINT 1
CATALOG_BYTES <N>
WORLD_BYTES <M>
```

The three header lines end in LF; N and M are positive canonical decimal byte
counts. Exactly N catalog bytes and M world bytes follow, with no extra delimiter
or trailing data. Each section is at most 1 MiB; total size is at most
2,097,280 bytes (2 MiB + 128). Catalog text must parse and resave identically.
Native world v1/v2/v3 parsing and conditional v2/v3 writing remain available separately. Legacy
raw world saves still rely on the supplied catalog; the checkpoint wrapper
carries catalog identity. Neither format authenticates data or its history.

The JSONL export methods work for observers and controllers:

- `begin_checkpoint`: decimal-string `revision` required. Returns `errorCode`,
  opaque `checkpointId`, integer `totalBytes`, frozen decimal-string `revision`
  and `tick`, and integer `maxChunkBytes` (16384).
- `read_checkpoint`: string `checkpointId`, integer raw-byte `offset` from 0
  through totalBytes, and integer `limit` from 1 through 16384. Returns
  `errorCode`, `checkpointId`, `offset`, `nextOffset`, `totalBytes`, `hasMore`,
  `dataBase64` and frozen revision/tick. Decode each RFC 4648 padded Base64 slice
  and concatenate bytes, not strings. Reading at EOF returns an empty slice.

Each dispatcher retains one immutable successful export. World steps, queued
commands and rule reloads do not alter it. A successful new begin invalidates the
old ID; a failed begin preserves it. An unknown/invalidated ID returns
`invalid_argument`; malformed fields/bounds are protocol `invalid_request`
errors. Hello advertises the methods, `export_checkpoint` query action and
`maxCheckpointBytes` / `maxCheckpointChunkBytes`. Existing session rules apply.
The protocol never accepts a filesystem path for checkpoint export.

The stdlib `tools/economy_checkpoint.py` client exports over `--url` (HTTP bridge)
or `--pipe` (Windows economy pipe), validates chunks, and publishes to `--output`.
Default publication is exclusive and preserves an existing file; `--force`
requests atomic replacement. The parent directory must exist. The default
exclusive hard-link operation requires filesystem support. The native checkpoint
is an opaque file, not JSON; only the command's receipt is JSON.

Start the standalone host with `--checkpoint <file> [--role observer|controller]`.
It performs a bounded binary read and restores the embedded catalog and world
before creating the API. This option excludes any explicitly supplied `--catalog`,
`--rules`, `--region`, `--content-root`, `--center` or `--radius`. Invalid startup
input returns exit 2 with a stderr diagnostic before protocol stdout.

A restart creates a new session and API handles/events. Old handles are unknown;
native pending commands/results, world IDs/counters, cargo and progress persist.
Pending commands apply on the next step. The active catalog persists; future Lua
reloads use it as their base. Old Lua source/history and region/render provenance
are not reconstructed. The Windows desktop also supports native checkpoint
startup and its exact native-ID primitive view; see below and the
[desktop receipt](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md). The
[checkpoint receipt](DEVELOPMENT_REPORT_2026-10-22_CHECKPOINTS.md) retains the
earlier process restart/HTTP coverage. Cross-platform replay remains untested.


## Desktop native checkpoint startup

`OpenRepublic.exe --economy-checkpoint <file> [--economy-pipe-name <name>]`
restores a native catalog/world before window/API startup. Run from the client
build directory. This mode uses authored primitives with exact native building
and road IDs; no original map/source identity is reconstructed. Both X and Z
road/truck coordinates share one centered transform. Native inventory amounts
remain integers. Pending structural commands reconcile the rendered scene after
steps from either human controls or the optional trusted-local pipe.

Frame world explicitly rebases geometry and fits the camera, retaining selected
native ID without changing world state. Ordinary updates preserve the origin
and camera. Renderer precision is validated before startup/scene replacement;
compact clusters at large native coordinates can be recentered, while excessive
occupied spans reject. Later fixed-origin precision failure retains the last
scene, reports stale presentation and disables picking until a valid reframe.

Checkpoint mode excludes content/map/probe/capture/fixture/remote-economy and
scene-control-pipe options. Graphics backend selection and --modern are allowed.
Invalid inputs exit 2 before the graphics window. This adds startup restore;
in-place GUI load/save dialogs remain future work. See the
[desktop report](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md) for separate
source/test, live process/pipe and graphical acceptance receipts.


## Explicit command-history maintenance

The native history contract is version 1. `hello` advertises
`commandHistoryVersion: 1`, `commandHistoryPolicy: "explicit"`, the controller
method `compact_history`, action `compact_command_history` and
`limits.maxCommandHistoryRetention: 4096`. The observer may read history metadata;
only query+control permission permits maintenance. API/protocol v0 remains
additive and discoverable.

`world.commandHistory` contains integer `version: 1` and decimal-string
`firstRetainedCommandId`, `nextCommandId`, `retainedTerminalResults` and
`pendingCommands`. These are native command IDs/counts, distinct from API handles.

```json
{"id":"compact-1","protocolVersion":0,"sessionId":"<current-session>","method":"compact_history","revision":"<current-revision>","keepRecent":1024}
```

`revision` is a required decimal string; `keepRecent` is a required integer
0..4096. Extra/invalid fields reject as `invalid_request`. The typed equivalent
is `EconomyApi::CompactCommandHistory(expectedRevision, keepRecent)`. Permission
is checked before revision and typed retention validation. It reports
`permission_denied`, `stale_revision`, typed `invalid_argument`, or
`revision_exhausted` without changing the world/handle maps. Only successful
responses' history/revision fields describe authoritative current state.

On success the result contains `errorCode: "none"`, decimal-string
`removedResults`, `expiredHandles`, `revision`, and `commandHistory`. It discards
only an older completed prefix, keeping the newest requested terminal results
and every queued command/result. IDs are never reminted. Actual pruning advances
revision once without a tick; no-op maintenance preserves revision/hash. Pending
execution retains its reserved revision space. API handle mappings and rejection
markers below the native floor are retired too, including cleanup after direct
core compaction even if the API call removes no further core results.

Poll a retired current-session handle to receive `errorCode: "command_expired"`,
`state: "expired"`, `failure: "none"` and no former entity outputs. Its prior
outcome is unavailable. In the typed result, `commandId` is zero after the API
mapping has been retired; it must not be inferred from the handle. A still-mapped
result expired by a direct core caller may retain its actual native ID until API
cleanup. Zero/never-issued handles remain `unknown_command`; new sessions inherit
no old handles. Existing bounded event records may reference expired handles;
maintenance neither rewrites events nor invents rejected-command/tick events.

`SaveWorld` writes v3 with `HISTORY <firstRetainedCommandId>` immediately after
NEXT when the retained floor exceeds one. v1/v2 assume floor one; v3 reads a floor
in 1..nextCommand. Results must densely cover [floor,nextCommand), with the exact
queued suffix preserved and earlier results terminal. Persisted Expired states
reject. Lifetime applied-command validation counts all minted commands, including
expired history. Uncompacted v2 bytes/hashes are preserved; compacted hashes also
include a domain-tagged floor. The checkpoint wrapper stays v1, and export never
compacts implicitly or changes an already-frozen snapshot. Older executables
cannot load compacted v3 worlds.

Record any needed old outcomes before maintenance. Live entities, catalogs and
large pending queues can still exceed persistence bounds. See the
[command-history receipt](DEVELOPMENT_REPORT_2026-10-22_COMMAND_HISTORY.md) for
source, process continuation and remaining acceptance limits.


## Native desktop construction sandbox

`OpenRepublic.exe --native-economy [--economy-pipe-name <name>]` starts an empty
64x64 OpenRepublic-authored economy with the synthetic catalog and exact native-ID
primitive scene. Run from the client build directory so copied Content is found.
This flag and `--economy-checkpoint <file>` are mutually exclusive; neither may be
combined with original-content import/probe/capture, remote economy or scene-pipe
modes. Both native startup modes share these human controls:

- Choose a building type, press Place, then click its origin cell. The gold draft
  uses the retained scene transform and does not establish valid placement.
- Press Road; click a start cell and a different cell on the same row or column.
  A span of at most 64 unit segments queues independent `build_road` commands.
  Partial admission retains the remaining start; stepping may accept/reject each
  segment independently. There is no transactional road batch.
- Select a building and mark it as haul source, then select the destination.
  Choose a resource present in both buildings' positive storage capacities and
  choose the vehicle type before submitting. Shared resources are sorted by
  native catalog ID; empty storage and storage-only buildings are eligible.
  This menu is a storage compatibility query, not stock or route validation.
- Step 1, 24 or 240 ticks to apply queued work and advance production/hauls.
  Selected building status shows authoritative inventory and recipe progress.
  Queue admission is distinct from terminal acceptance; rejection feedback stays
  visible after stepping.
- Cancel clears the current construction draft. Remove selected queues the
  existing `remove_building` command: it discards that building's inventory and
  rejects endpoints still referenced by haul orders. No construction costs,
  refunds, construction time or workforce mechanics are introduced.

The tools call the typed API with the current revision. GUI input/palette dismissal
has priority over world placement. Exact native IDs, current scene-transform
inverse mapping and stale-scene write guards are shared with restored worlds.
In-app checkpoint dialogs and continuous run/pause remain future work. Actual
Escape dismissal and live GUI removal remain UNKNOWN in this increment; see the
[native construction receipt](DEVELOPMENT_REPORT_2026-10-22_NATIVE_CONSTRUCTION.md).


## Native haul-order repair controls

Native fresh/checkpoint desktops expose an exact-ID order dropdown with endpoints, resource, cargo and state. Selected-haul HUD adds truck ID/cell and failure reason. Choose a haul, select a destination building, then Retarget to selected; Cancel haul submits cancellation of the chosen order. Both use the existing revision-bound typed API and validate on the next tick. Loaded cancellation rejects rather than discarding cargo. Removed orders invalidate selection; no other order is substituted. Run applies queued work automatically; otherwise step and inspect terminal feedback. Stale native scenes require Frame world before using these controls. GUI acceptance is pending; see [order tools](DEVELOPMENT_REPORT_2026-10-22_ORDER_TOOLS.md).


Native road controls: choose the exact road ID/endpoints and Toggle road, then step or Run. Enabled state updates after authoritative execution. Repeated clicks before stepping request the same inverse current state, not cancellation of prior queued work. See [live receipt](DEVELOPMENT_REPORT_2026-10-22_ROAD_TOOLS.md).


Native Save checkpoint pauses the desktop and captures ExportCheckpoint at the queried revision before a Windows Save dialog. Select a new file; existing paths reject without overwrite. Cancel stays paused. The synchronous dialog suspends pipe request pumping until closed, so external clients may time out. Restore uses --economy-checkpoint; in-place loading is future work. [Evidence and limits](DEVELOPMENT_REPORT_2026-10-22_SAVE_UI.md).


App-owned imported regions now share explicit resource/vehicle and haul-order controls with native mode, using exact accepted-placement bindings and the current owned world/catalog. Construction and native road controls are hidden; no imported placement transform is implied. Compact/Details retains imported inventory formatting. Legacy remote economy-url stays separate. [Integration and fixture acceptance](DEVELOPMENT_REPORT_2026-10-22_REGION_LOGISTICS.md).
