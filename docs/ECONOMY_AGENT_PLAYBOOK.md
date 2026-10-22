# Operating the economy as an agent (playbook)

For LLM or script operators of `OpenRepublicEconomyHost`. Protocol details are in `ECONOMY_API.md`; this page is the working method.

## Desktop app-owned map-region mode

By default, launching the desktop app with `--map-region` creates the selected bounded economy world inside that app process. It imports the catalog, accepted buildings, and decoded roads, then routes human inspection, toggle, haul, and 24-tick-step actions through the typed `EconomyApi`.

```
OpenRepublic.exe --content-root <content dir> --map-package <map dir> --map-region --map-building <index> --map-radius <meters>
```

Each rendered instance carries its index from the exact loaded `allPlacements` vector. Selection uses the importer mapping from that source index to the economy building ID; missing, rejected, or unrendered placements have no action binding. The index is load-local and is not a durable identifier across package changes.

For an open-content UI run that needs no local game files, launch:

```
OpenRepublic.exe --synthetic-region-fixture
```

This creates OpenRepublic-authored Farm, Mill, and Bakery block visuals with
matching plain placement data and the app's synthetic economy catalog. It can
exercise the same in-process buttons without loading a WRSR-shaped package. A
later live run displayed all three buildings and verified picking, toggle/step,
haul creation and Farm-to-Mill production. The current local UI adds one-tick
stepping, wrapped status, native synthetic integer quantities and smaller truck
markers. A coordinated follow-up observed movement at single ticks 27/28,
mill production at tick 52 and overflow wheel/scrollbar interaction with
selection preserved. See the [UI report](DEVELOPMENT_REPORT_2026-10-22_ACCESS_AND_UI.md).

To let a trusted local controller process share this exact app-owned world, opt in to the Windows named pipe:

```text
OpenRepublic.exe --content-root <content dir> --map-package <map dir> --map-region --map-radius <meters> --economy-pipe-name OpenRepublicController
```

The client connects to `\\.\pipe\OpenRepublicController` with UTF-8 JSONL, one request and response per line. Send `hello` first, then include its `sessionId` on every subsequent request except `hello` and `capabilities`. Read `world` for the current revision before `submit`; the API rejects a stale revision. Controller `step` advances the same world directly. Embedded `shutdown` is disabled because the app owns the endpoint lifetime. The endpoint is disabled by default and cannot be combined with `--economy-url`, capture, or probe modes.

This endpoint is local and trusted, not authenticated. It uses the process token's default security descriptor and rejects remote clients. Any local process allowed by that descriptor can submit controller commands. One synchronous client is served at a time, and a client that leaves an incomplete line open holds the slot until disconnect or app shutdown; no idle-read timeout is implemented yet. The wire protocol is OpenRepublic JSONL v0, not a WRSR protocol. The endpoint has passed a Release build and local controller smoke against a read-only imported region; human picking/buttons on a synthetic/open fixture still need graphical acceptance.

## Connect to the standalone host

Use this mode when an external client should control the standalone `OpenRepublicEconomyHost` world. It is a separate process and separate simulation from the desktop app-owned mode.

Run the host behind the loopback wrapper so the world persists between calls:

```
python tools/economy_http_bridge.py --host-exe <path to OpenRepublicEconomyHost.exe> --port 8950 -- --role controller [--region <map dir> --content-root <content dir>]
```

The explicit `--economy-url` option selects the legacy desktop bridge to that host. For these controls, start the host and app with the **same** local map package, content root, center placement index, and requested radius (host radius must be 10–1000 m):

```
python tools/economy_http_bridge.py --host-exe <path to OpenRepublicEconomyHost.exe> --port 8950 -- --role controller --region <same map dir> --content-root <same content dir> --center <index> --radius <meters>
OpenRepublic.exe --content-root <same content dir> --map-package <same map dir> --map-region --map-building <index> --map-radius <meters> --economy-url http://127.0.0.1:8950
```

The app checks only that the host's center index and requested radius match the preview; it cannot verify map-package identity, so passing the same inputs is required. Select a mapped building in the scene to inspect it. **Toggle selected** queues its enabled state; **Step 24 ticks** advances the shared host. To haul, select the source and choose **Set haul source**, select the destination, then choose **Haul to selected** and advance the world. The app picks the first compatible catalog output/input resource and the first listed vehicle type. Matching uses building type and position; tied or otherwise ambiguous matches stay unbound. The host remains a separate process, and the rendered map geometry is static while economy state, color cues, and truck markers update. No original game process is required for the adapter itself.

Then `POST http://127.0.0.1:8950/rpc` with `{"method": "...", ...}` (`protocolVersion` and `id` are filled in). PowerShell: `Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8950/rpc -Body '{"method":"hello"}'`.

## Orientation loop
1. `hello` - process-local `sessionId`, role, allowed actions and, for an imported region, the provenance text (cell size, origin, import counts). Include that session ID on follow-up requests if they must be bound to this host instance; the ID is only a host-replacement marker, not authentication. Everything from a game map is local, experimental data.
2. `catalog` - resources, building types (storage slots, `operate` recipe inputs/outputs per tick) and vehicle types. Amounts are integer micro-units (1,000,000 = 1 unit); one tick is 1/24 of a game day in the compat catalog, so rates are per-tick micro-units.
3. `world` - buildings with inventories and a `stallReason` plus plain-text `stallExplanation`, roads, vehicles, haul orders, `tick`, `revision`, `stateHash`.
4. `explain` a building when it is not `running` (`missing_input`, `output_full`, `disabled`, ...).

## Acting
- Request shapes: `{"method":"submit","revision":"<world revision string>","command":{"kind":"create_haul_order","source":1,"destination":2,"resource":"rawiron","vehicleType":"truck"}}`, `{"method":"step","ticks":200}`, `{"method":"poll","handle":"1"}`, `{"method":"explain","buildingId":2}`. `revision` must equal the `revision` from your latest `world` read (stale values are rejected).
- `submit` commands (controller only): `place_building`, `remove_building`, `build_road`, `set_road_enabled`, `set_building_enabled`, `create_haul_order`, `cancel_haul_order`, `retarget_haul_order`. Each returns a command handle; `poll` it. Commands apply at the next tick boundary, so `step` before reading results. The desktop overlay polls at most one outstanding handle per world-poll cycle to bound local request traffic.
- `step` advances 1 to 100,000 ticks. Prefer short steps (50-500) while you diagnose, longer ones to let flows settle.
- A haul order moves one resource from a source building to a destination building with one truck; the truck waits (`DestinationFull`, `SourceEmpty`) rather than failing. Chain stalls: find the building whose `stallReason` is `missing_input`, find which resource, find a building that produces it and create a haul order.
- Build roads before ordering hauls between unconnected buildings; a building's access cell is the cell below its footprint's minimum-z edge centre, and `NoRoute` means no connected road path exists.

## Good practice

- Replan an existing route with `submit` command
  `{"kind":"retarget_haul_order","orderId":1,"destination":3}` using the
  current world revision. Step and poll the handle. A loaded truck keeps its
  cargo and travels toward the new destination; an empty truck returns to the
  original source before loading. `no_route` and `destination_full` preserve
  cargo while waiting. Same-source, incompatible, and duplicate destinations
  are rejected.
- Cancellation rejects a loaded order with `cargo_in_transit`. Retarget it or
  let it unload, then cancel while empty.
- Rule tuning preserves recipe resource identity. A vehicle capacity reduction
  below cargo already aboard rejects the entire reload, preserving the old rules.

- Re-read `world` after every `step`; trust `stateHash` equality to verify replays.
- Compare before/after inventories to measure the effect of a command.
- Do not assume WRSR formulas: rates, storage sizes, footprints and truck data in the compat catalog are partly inferred (see `COMPAT_CATALOG.md` and the evidence ledger).

- `advise` (read-only, observer or controller): {"method":"advise"} lists inputs no haul order serves, with up to 3 nearest producers each and a ready-to-submit `create_haul_order` command, plus `unconsumedSurplus` for output_full buildings nothing consumes. Heuristic planner only; it reads the first recipe of each building type.

- `advise.retargets` is a separate route/capacity-aware suggestion list for loaded
  trucks waiting at full storage. Submit each returned `command` using the
  advice `revision`, step and poll. It chooses enabled-road alternatives with
  room for the whole cargo after conservative loaded-inbound and suggestion
  reservations, excluding duplicate haul tuples. At most 32 suggestions are
  emitted; `retargetsTruncated` reports eligible work left unexamined, which may
  still be infeasible. Re-read advice after advancing the world. Capacity/path
  checks describe the snapshot; they cannot guarantee room at arrival. See
  [the replanning receipt](DEVELOPMENT_REPORT_2026-10-22_REPLANNING.md).

- Lua economy tuning: start the host with `--rules data/rules/economy_rules.lua` to scale building speed/storage and vehicle capacity/speed (integer percent 10..1000, names must exist). The script runs in the same sandbox as the demo rules (no libraries, 64 KiB, 8 MiB, instruction cap); any error rejects the whole bundle and the host refuses to start.

- Baseline autonomous republic: `python tools/republic_autopilot.py --host <OpenRepublicEconomyHost.exe> --rounds 6 --ticks 500 <host args>` loops advice -> missing raw producer placement -> refreshed advice -> retargets -> new haul links -> step/poll. It reports placements and queued/accepted/rejected haul creates and retargets separately, refreshes stale advice once and retains only actual command handles. It does not cancel loaded trucks or permanently remember failed creates. The earlier private-map run placed a solar plant and created 15 haul orders; that historical result predates this controller increment and has not been remeasured.

- Benchmark: add `--json` to `tools/republic_autopilot.py` for one result line comparing the controller with an idle run of the same world and tick count (metric `stored_non_raw_units`; electricity counts as raw). N22, 6,016 ticks: controller 952.9 vs idle 0.0. Any LLM controller can be scored the same way.

- Hot reload: a controller can send `{"method":"reload_rules","text":"<lua>"}` (inline text, never a path) to swap in a Lua-tuned catalog between ticks. The rules are applied to the catalog the host started with (including a startup `--rules` file), and each reload replaces the previous one rather than stacking. The structure must be unchanged (same names, recipes, storage slots); a swap that would leave a building over its reduced capacity is rejected and the world is untouched. In-progress recipe timers are clamped to a shortened interval.

- Rule reloads now appear in `events` as `rules_reloaded` (with the tick), so a replay knows when to resend them. The autopilot has `--count-electricity` to include power-plant output in the score (N22, tick 6,016: 9,866 stored units vs 953 without it).

- Batch haul orders: `{"method":"create_haul_orders","revision":"<rev>","orders":[{"source":1,"destination":2,"resource":"rawiron","vehicleType":"truck"}, ...]}` queues up to 64 orders under one revision check and returns one `{errorCode, handle, state}` per order; poll each handle after stepping. Controller only.

- `rules_history` (observer or controller) lists every accepted `reload_rules` text with the tick it applied at; replaying a run means resending those at the same ticks. Rejected reloads are not recorded.


## Save and resume the native economy

Export the current live standalone/bridge world to a new local file:

```powershell
python tools/economy_checkpoint.py --url http://127.0.0.1:8950 --output <new-file.checkpoint>
```

For an enabled Windows app-owned economy endpoint, the client also accepts
`--pipe OpenRepublicController` instead of `--url`. Live pipe export from a
checkpoint-restored desktop is verified in the [desktop receipt](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md). The tool
uses query permission, binds its session, reads one frozen chunked snapshot and
refuses an existing output unless `--force` explicitly requests replacement.
Only completed validated data is published; no file path is sent to the host.

Resume in a new standalone host process:

```text
OpenRepublicEconomyHost.exe --role controller --checkpoint <file.checkpoint>
```

Do not combine checkpoint startup with catalog, rules or region import options.
The file contains its exact active catalog, including tuning, and native world
state. Re-handshake after restart: old session IDs and API handles do not carry
over. Inspect the world; queued native commands are still pending until the next
step. Old event journals and Lua source history are not reconstructed. Later Lua
percentages are relative to the catalog embedded in the checkpoint.

Limits remain explicit: at most 1 MiB each for the catalog and native world,
2 MiB + 128 total. A large retained command history can exceed the world bound;
export then reports `persistence_failed`, preserving an earlier snapshot/file.
The wrapper does not authenticate history or establish WRSR save compatibility.
For a native primitive desktop view, run `OpenRepublic.exe --economy-checkpoint
<file.checkpoint> [--economy-pipe-name OpenRepublicController]` from the client
build directory. Exact native IDs bind selection; Frame world explicitly
recenters the view without stepping. Pending work applies on the next step.
Huge occupied spans outside float precision reject before the graphics window.
Keep genuine imported state local under the project's existing research
boundaries. [Current verification and next goals](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md).


## Keep completed command history bounded

Poll and record the outcomes you need before discarding them. On hosts advertising
`compact_history` and history contract version 1, query the current world and
send `compact_history` with that decimal-string revision and integer `keepRecent`
0..4096. This controller-only maintenance retains every queued command and the
newest requested terminal results; it does not step production or change cargo.
Actual pruning advances revision once, so refresh before subsequent commands.
No-op requests preserve world hash/revision. Never assume export prunes history.

Expired issued handles return `command_expired` / `expired`, failure `none`, with
the historical outcome unavailable. Never-issued handles remain unknown. The typed
result uses commandId zero when its native mapping has been retired; handles and
native command IDs are independent. Bounded journal events can still mention an
expired handle. A fresh API session inherits no old handles.

The autopilot discovers support, polls its round's command outcomes, then keeps
1024 completed results by default. `--retain-command-results N` accepts 0..4096;
`--retain-command-results all` explicitly opts out. Older hosts without the
method retain their existing behavior. A stale maintenance request gets one
bounded refresh/retry. Output reports retired counts; JSON adds
`commandResultsRetired` and `commandResultsRetention` (null for all).

Compacted worlds use native save v3 inside checkpoint wrapper v1; uncompacted
worlds retain v2 bytes/hashes. Older executables cannot read v3. Retained terminal
history can be reduced; oversized live entities, catalogs or pending queues can
still prevent export. [Current receipt and limits](DEVELOPMENT_REPORT_2026-10-22_COMMAND_HISTORY.md).


## Build a native world through the desktop

From `build/openrepublic-vs2022`, launch:

```powershell
.\srcpp\Release\OpenRepublic.exe --native-economy --economy-pipe-name OpenRepublicController
```

The authored 64x64 world begins empty. Place the chosen building type, then use
Road to connect two cells on the same row/column (maximum 64 unit segments per
span). Step to apply the queue before selecting the resulting buildings. Drafts
are previews; inspect terminal applied/rejected feedback after stepping.

For a resource chain, mark source and destination, choose their shared storage
resource and a vehicle type, then queue the haul. Repeat for Farm -> Mill ->
Bakery -> Market using grain, flour and bread. Step 240 advances the paused world;
selected recipe status shows cycles and remaining ticks. The resource menu does
not require current stock and supports storage-only endpoints. Roads, placement
and haul requests still undergo authoritative validation.

Cancel exits construction mode. Remove selected discards inventory through the
existing native command and rejects buildings referenced by haul orders. Road
spans are independent unit commands and can partially succeed. Export through
`tools/economy_checkpoint.py --pipe OpenRepublicController --output <new.checkpoint>`;
resume with `--economy-checkpoint <file>` and the same native tools. Native flags
are exclusive with import/probe/capture/remote modes. GUI save dialogs and
continuous run/pause remain future work.

The [acceptance receipt](DEVELOPMENT_REPORT_2026-10-22_NATIVE_CONSTRUCTION.md)
records a GUI-built chain, visible collision rejection, actual export/restart
hash parity and input isolation. Escape-key dismissal and GUI removal were not
visibly verified; use the Cancel button for the demonstrated construction exit.


## Native haul-order repair controls

Native fresh/checkpoint desktops expose an exact-ID order dropdown with endpoints, resource, cargo and state. Selected-haul HUD adds truck ID/cell and failure reason. Choose a haul, select a destination building, then Retarget to selected; Cancel haul submits cancellation of the chosen order. Both use the existing revision-bound typed API and validate on the next tick. Loaded cancellation rejects rather than discarding cargo. Removed orders invalidate selection; no other order is substituted. Run applies queued work automatically; otherwise step and inspect terminal feedback. Stale native scenes require Frame world before using these controls. GUI acceptance is pending; see [order tools](DEVELOPMENT_REPORT_2026-10-22_ORDER_TOOLS.md).


Native road controls: choose the exact road ID/endpoints and Toggle road, then step or Run. Enabled state updates after authoritative execution. Repeated clicks before stepping request the same inverse current state, not cancellation of prior queued work. See [live receipt](DEVELOPMENT_REPORT_2026-10-22_ROAD_TOOLS.md).


Native Save checkpoint pauses the desktop and captures ExportCheckpoint at the queried revision before a Windows Save dialog. Select a new file; existing paths reject without overwrite. Cancel stays paused. The synchronous dialog suspends pipe request pumping until closed, so external clients may time out. Restore uses --economy-checkpoint; in-place loading is future work. [Evidence and limits](DEVELOPMENT_REPORT_2026-10-22_SAVE_UI.md).


App-owned imported regions now share explicit resource/vehicle and haul-order controls with native mode, using exact accepted-placement bindings and the current owned world/catalog. Construction and native road controls are hidden; no imported placement transform is implied. Compact/Details retains imported inventory formatting. Legacy remote economy-url stays separate. [Integration and fixture acceptance](DEVELOPMENT_REPORT_2026-10-22_REGION_LOGISTICS.md).
