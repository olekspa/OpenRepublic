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
exercise the same in-process buttons without loading a WRSR-shaped package. The
latest build opened the controls and reported three imported buildings, but the
first 3D viewport appeared black; selection and economy-button acceptance remain
open.

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
- `submit` commands (controller only): `place_building`, `remove_building`, `build_road`, `set_road_enabled`, `set_building_enabled`, `create_haul_order`, `cancel_haul_order`. Each returns a command handle; `poll` it. Commands apply at the next tick boundary, so `step` before reading results. The desktop overlay polls at most one outstanding handle per world-poll cycle to bound local request traffic.
- `step` advances 1 to 100,000 ticks. Prefer short steps (50-500) while you diagnose, longer ones to let flows settle.
- A haul order moves one resource from a source building to a destination building with one truck; the truck waits (`DestinationFull`, `SourceEmpty`) rather than failing. Chain stalls: find the building whose `stallReason` is `missing_input`, find which resource, find a building that produces it and create a haul order.
- Build roads before ordering hauls between unconnected buildings; a building's access cell is the cell below its footprint's minimum-z edge centre, and `NoRoute` means no connected road path exists.

## Good practice
- Re-read `world` after every `step`; trust `stateHash` equality to verify replays.
- Compare before/after inventories to measure the effect of a command.
- Do not assume WRSR formulas: rates, storage sizes, footprints and truck data in the compat catalog are partly inferred (see `COMPAT_CATALOG.md` and the evidence ledger).

- `advise` (read-only, observer or controller): {"method":"advise"} lists inputs no haul order serves, with up to 3 nearest producers each and a ready-to-submit `create_haul_order` command, plus `unconsumedSurplus` for output_full buildings nothing consumes. Heuristic planner only; it reads the first recipe of each building type.

- Lua economy tuning: start the host with `--rules data/rules/economy_rules.lua` to scale building speed/storage and vehicle capacity/speed (integer percent 10..1000, names must exist). The script runs in the same sandbox as the demo rules (no libraries, 64 KiB, 8 MiB, instruction cap); any error rejects the whole bundle and the host refuses to start.

- Baseline autonomous republic: `python tools/republic_autopilot.py --host <OpenRepublicEconomyHost.exe> --rounds 6 --ticks 500 <host args>` loops `advise` -> place an unpowered-resource generator near the consumer -> haul orders -> step. On the private map region it placed a solar plant and created 15 haul orders. It is a rule-based baseline for LLM/learned controllers, with no scoring beyond the running-building count.

- Benchmark: add `--json` to `tools/republic_autopilot.py` for one result line comparing the controller with an idle run of the same world and tick count (metric `stored_non_raw_units`; electricity counts as raw). N22, 6,016 ticks: controller 952.9 vs idle 0.0. Any LLM controller can be scored the same way.
