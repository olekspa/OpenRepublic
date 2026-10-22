# OpenRepublic Economy API v0

`OpenRepublicEconomyApi` exposes the deterministic `economy::World` and immutable
`economy::Catalog` through typed calls. `OpenRepublicEconomyHost` provides a
trusted-local JSONL adapter. The API and protocol versions are both `0` and are
OpenRepublic interfaces; they make no compatibility claim about WRSR.

The host reuses `third_party/WickedEngine/Editor/json.hpp`. CMake exposes the
containing directory as the `OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR` cache path.
It defaults to this checkout's initialized Wicked Engine submodule, or to
`D:/.CODEX_Projects/OpenRepublic/third_party/WickedEngine/Editor` when that
submodule header is absent and the main checkout is available. Set the cache
path explicitly when the header is stored elsewhere.

## Host and framing

The host reads one UTF-8 JSON object per line from stdin and writes one JSON
response per line to stdout. Start without arguments as an observer, or use
`--role controller`; an optional `--catalog <file>` loads strict catalog v1
text instead of `MakeSyntheticCatalog()`. The process owns a 64 by 64 empty
world. Role selection is not authentication and this stdio process must remain
trusted and local.

Every request has `id` (1 to 128 bytes), `protocolVersion: 0`, and `method`.
Responses echo `id`, include `protocolVersion`, and contain `ok` with `result`,
or `ok: false` and an `error` code/message. Unknown fields, duplicate keys,
invalid types, malformed JSON, and unsupported versions are rejected before
calling the API. Request lines are limited to 64 KiB; overlong input returns
`request_too_large` and exits with status 5. The process accepts at most 8192
requests, JSON nesting is limited to 16 and parsed structure to 4096 nodes.
Every `uint64_t` is encoded as a decimal string; bounded IDs and coordinates
are JSON integers.

`hello` and `capabilities` return role-aware `methods`, available `actions`,
limits, and the uint64 encoding. Observers may call `hello`, `capabilities`,
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
