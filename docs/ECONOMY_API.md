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
source/build and local runtime wiring have been exercised, but graphical UI
interaction on a synthetic/open fixture remains unverified. The fixture launch
path is described below; its first window showed the controls and importer status,
but the 3D viewport appeared black and no control interaction was completed.

### Synthetic/open graphical-acceptance fixture

Run `OpenRepublic.exe --synthetic-region-fixture` for a self-contained
OpenRepublic-authored Farm/Mill/Bakery scene, a visible primitive road, and a
matching economy input. It accepts no WRSR paths and cannot be combined with map,
probe, capture, or economy-endpoint options. The scene exists to exercise the same
app-owned region selection and economy controls; it is not a WRSR-content showcase
or a compatibility claim. The latest Release build succeeded, but the first live
frame's black viewport and incomplete picking/action flow leave graphical
acceptance **UNKNOWN**.

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
`catalog`, `world`, `explain`, `poll`, and `events`. Controllers may also call
`submit`, `step`, and `shutdown`. `world` currently returns all four sorted
collections; the typed API provides revision-checked pages of up to 32 items
for `buildings`, `roads`, `vehicles`, or `orders`.

## Methods

- `catalog`: returns resource names, building footprints/storage/recipes, and
  vehicle capacity and speed. Recipe and storage amounts are decimal strings.
- `world`: returns revision, tick, state hash, buildings with inventories and
  stall text, roads, vehicles, and haul orders.
- `explain`: requires integer `buildingId`; returns current stall reason and
  explanation text.
- `submit`: requires decimal-string `revision` and a tagged `command` object.
  Kinds are `place_building` (`type`, `x`, `z`), `remove_building`
  (`buildingId`), `build_road` (`ax`, `az`, `bx`, `bz`), `set_road_enabled`
  (`roadId`, `enabled`), `set_building_enabled` (`buildingId`, `enabled`),
  `create_haul_order` (`source`, `destination`, `resource`, `vehicleType`), and
  `cancel_haul_order` (`orderId`). Names resolve exactly through the catalog.
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
