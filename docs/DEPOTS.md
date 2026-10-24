# Depot owned trucks

This is an OpenRepublic authored synthetic logistics feature. It does not claim
to reproduce WRSR depot, dispatch-office, trade, vehicle, coal, heat, timing, or
pricing rules. Those original-game semantics remain **UNKNOWN**. Source and
tests verify only the C++ implementation described here.

## Catalog declarations

Depot and fleet declarations are optional catalog members:

```text
building road_depot 1 1
  depot
  fleet truck 2
  handling 2 1
end
```

Each `fleet` entry names a declared vehicle type and positive count. A placed
depot spawns its declared fleet in stable vehicle ID order at the depot access
cell. A depot type may declare at most 4,096 vehicles across its fleet rows.
`handling <loadTicks> <unloadTicks>` declares positive tick durations;
both default to one when omitted. A fleet type can appear once per depot type.
Storage only buildings remain ordinary `building` declarations with one or
more `store` slots and no `recipe`.

Catalogs without `depot`, `fleet`, and `handling` declarations keep their
canonical catalog text, native save version, hash input, and legacy vehicle JSON
fields unchanged.

## Dispatch and vehicle state

Protocol v0 controller command:

```json
{"kind":"dispatch_truck","vehicle":1,"source":2,"destination":3,"resource":"coal","amount":"4"}
```

`amount` is optional and encoded as a decimal string when supplied. Zero or
omitted means up to vehicle capacity. A command is queued and validated at the
next tick boundary. It requires a depot owned, idle truck and storage for the
selected non-utility resource at both endpoints. Submitting a busy truck is
rejected when applied. Use `world` for the current revision and `poll` for the
terminal command outcome.

The truck drives empty from its depot to the source, loads up to the requested
amount, its capacity, and available source stock, drives to the destination,
unloads into available storage, and returns to its owning depot. A full
destination leaves it loaded and waiting. An empty source waits during the
loading phase and retries on later ticks. When the truck returns, its order is
retired and it parks at its depot for a later command. Existing repeating
`create_haul_order` vehicles retain their existing source/destination cycle.

Depot-owned vehicles add these snapshot fields while preserving the old fields:

| Field | Meaning |
|---|---|
| `ownerDepotId` | Stable building ID of the owning depot |
| `parked` | True when the truck has no active order |
| `leg` | `parked`, `to_source`, `loading`, `to_destination`, `unloading`, or `returning` |
| `legTicksRemaining` | Remaining handling ticks in a loading or unloading phase |

The snapshot cell is the authoritative per-tick grid cell for road movement.
Rendering can place the vehicle there; `leg` and `legTicksRemaining` describe
the simulation state without renderer-owned state. The API page for vehicles
returns the same additive fields. The `summary` fleet object reports total,
parked, and dispatched owned trucks, and its text adds a fleet line only when a
fleet exists.

## Trade posts and heating

An existing trade post can import a catalog priced resource through the
controller `trade_import` operation. Import is explicit and consumes treasury;
the border post must declare storage for that resource. The scenario below first
exports one locally produced synthetic resource to fund the coal import. There
is no automatic or recurring import.

Heating is demonstrated as an ordinary authored building recipe that consumes
coal and produces `heat` into the building's own storage. It is not a special
utility or a recovered WRSR formula.

## Persistence and hashes

When the active catalog declares a depot, native saves write version 8. It
persists parked fleet IDs and ownership, active owned vehicle leg and handling
state, pending dispatch commands, and active depot orders. Readers continue to
accept versions 1 through 7. Worlds whose catalog has no depot declaration keep
the prior conditional writer versions. Depot catalog declarations and owned
vehicle/order state enter the canonical state hash only for depot catalogs.
Checkpoints inherit the same native world version and exact catalog binding.

This is OpenRepublic native persistence only, not WRSR save compatibility.

## Scoreable scenario

[`coal_to_heat.json`](../data/scenarios/coal_to_heat.json) is a bounded scripted
controller fixture. Its exact command sequence is:

1. Export 4 grain at border post building 1 to earn 4 synthetic treasury units.
2. Import 4 coal into border post building 1.
3. Dispatch depot truck 1 from border building 1 to storage building 3.
4. Dispatch depot truck 2 from storage building 3 to heating plant building 4.
5. Advance the remaining requested ticks while the trucks travel, load, unload,
   return, and the heating recipe consumes coal.

Run it with `python tools/scenario_score.py --host <OpenRepublicEconomyHost.exe> --scenario coal_to_heat --ticks 120`. The
scripted controller is explicit and deterministic; it does not depend on the
baseline autopilot's haul adviser. The score is stored heat units. The receipt
includes each owned truck's order, leg, parked state, cargo, and current cell.
All quantities and scores are synthetic OpenRepublic values.
