# Operating the economy as an agent (playbook)

For LLM or script operators of `OpenRepublicEconomyHost`. Protocol details are in `ECONOMY_API.md`; this page is the working method.

## Connect
Run the host behind the loopback wrapper so the world persists between calls:

```
python tools/economy_http_bridge.py --host-exe <path to OpenRepublicEconomyHost.exe> --port 8950 -- --role controller [--region <map dir> --content-root <content dir>]
```

Then `POST http://127.0.0.1:8950/rpc` with `{"method": "...", ...}` (`protocolVersion` and `id` are filled in). PowerShell: `Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8950/rpc -Body '{"method":"hello"}'`.

## Orientation loop
1. `hello` - role, allowed actions and, for an imported region, the provenance text (cell size, origin, import counts). Everything from a game map is local, experimental data.
2. `catalog` - resources, building types (storage slots, `operate` recipe inputs/outputs per tick) and vehicle types. Amounts are integer micro-units (1,000,000 = 1 unit); one tick is 1/24 of a game day in the compat catalog, so rates are per-tick micro-units.
3. `world` - buildings with inventories and a `stallReason` plus plain-text `stallExplanation`, roads, vehicles, haul orders, `tick`, `revision`, `stateHash`.
4. `explain` a building when it is not `running` (`missing_input`, `output_full`, `disabled`, ...).

## Acting
- Request shapes: `{"method":"submit","revision":"<world revision string>","command":{"kind":"create_haul_order","source":1,"destination":2,"resource":"rawiron","vehicleType":"truck"}}`, `{"method":"step","ticks":200}`, `{"method":"poll","handle":"1"}`, `{"method":"explain","buildingId":2}`. `revision` must equal the `revision` from your latest `world` read (stale values are rejected).
- `submit` commands (controller only): `place_building`, `remove_building`, `build_road`, `set_road_enabled`, `set_building_enabled`, `create_haul_order`, `cancel_haul_order`. Each returns a command handle; `poll` it. Commands apply at the next tick boundary, so `step` before reading results.
- `step` advances 1 to 100,000 ticks. Prefer short steps (50-500) while you diagnose, longer ones to let flows settle.
- A haul order moves one resource from a source building to a destination building with one truck; the truck waits (`DestinationFull`, `SourceEmpty`) rather than failing. Chain stalls: find the building whose `stallReason` is `missing_input`, find which resource, find a building that produces it and create a haul order.
- Build roads before ordering hauls between unconnected buildings; a building's access cell is the cell below its footprint's minimum-z edge centre, and `NoRoute` means no connected road path exists.

## Good practice
- Re-read `world` after every `step`; trust `stateHash` equality to verify replays.
- Compare before/after inventories to measure the effect of a command.
- Do not assume WRSR formulas: rates, storage sizes, footprints and truck data in the compat catalog are partly inferred (see `COMPAT_CATALOG.md` and the evidence ledger).

- `advise` (read-only, observer or controller): {"method":"advise"} lists inputs no haul order serves, with up to 3 nearest producers each and a ready-to-submit `create_haul_order` command, plus `unconsumedSurplus` for output_full buildings nothing consumes. Heuristic planner only; it reads the first recipe of each building type.
