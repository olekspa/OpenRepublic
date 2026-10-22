# Native utility networks

OpenRepublic distinguishes stored material resources from utility resources.
Utility quantities are transferred within a tick through explicit connected
networks. They have no warehouse stock, storage capacity or truck cargo. This is
an independent first utility model, with no claim of original-game electrical,
water, heating or sewage parity.

## Catalog and simulation

The native catalog accepts `utility power` alongside ordinary `resource coal`.
Existing recipe input/output lists can reference either kind. For example:

```text
openrepublic-economy 1
resource coal
resource ore
utility power
building generator 1 1
  store coal 8
  recipe generate every 1 in coal:1 out power:3
end
building powered_mine 1 1
  store ore 16
  recipe extract every 1 in power:2 out ore:2
end
vehicle truck capacity 8 speed 4
```

All utility-bearing recipes currently require interval one. Generators may
consume materials and produce materials/utilities, but cannot consume utilities.
That restriction keeps bootstrap and recursive utility conversion unsupported
explicitly. Supply and demand use the catalog's integer units per tick; no
physical wattage or original-game conversion is implied.

Each tick applies queued commands, processes existing logistics, then executes
generators before consumers. A generator must be enabled, possess its material
inputs and have room for every material coproduct. It commits those material
changes before publishing power. Successful generators always burn their stated
fuel, including when no consumer requests power. Unused supply is discarded.

Connections form an undirected graph for one exact utility resource. An isolated
producer supplies only its own component. Consumers are considered in stable
building-ID/recipe-index order. Each recipe requires its complete utility demand,
material inputs and output room before any consumption occurs. A blocked recipe
does not reserve power from another consumer. Shortage uses the existing
missing-input diagnostic with the utility's name. No batteries, partial output,
transmission capacity/loss, voltage, switchgear or maximum connection span is
modeled. A link represents idealized connectivity, not a recovered power line.

Utility worlds use serial production for deterministic allocation. Material-only
worlds retain the existing production path. Lua tuning preserves resource kinds
and rejects changes that violate the one-tick utility recipe restriction.

## Shared human and controller actions

The typed API provides `ConnectUtility(revision, resourceName, a, b)` and
`DisconnectUtility(...)`. JSONL uses the normal session/revision-bound submission:

```json
{"id":"power-link","protocolVersion":0,"method":"submit","revision":"42","command":{"kind":"connect_utility","resource":"power","a":2,"b":3}}
```

Use `disconnect_utility` with the same resource/endpoints to remove a link.
Endpoints are canonicalized, and duplicate/missing connections reject when
applied. Both buildings must exist and differ. Building demolition requires
disconnecting retained utility links first. Submission queues work; step and poll
through the existing command contract. Observer and stale-session restrictions
continue to apply.

Catalog queries mark resource kinds with `utility`. World queries include
`utilityConnections`; bounded paging uses kind `utility_connections` with the
existing revision/offset/limit contract. Logistics advice does not propose utility
cargo hauls. Controller action discovery advertises both new commands.

In the desktop, choose a building using Set haul source, select a second building,
choose the utility in the separate utility menu, then Connect utility or
Disconnect utility. These controls reach the same C++ API in native and app-owned
imported regions. The existing source selector is shared; utilities do not use the
cargo resource/vehicle menu. Native authored scenes render elevated colored links
to show connectivity. Colors do not indicate supply or actual power flow.

## Imported definitions

Both the region host and desktop accept repeatable `--utility-resource <name>`
options in app-owned region mode. Each exact name must exist in the parsed
definitions. Declared recipe amounts keep the adapter's existing conversion;
marked resources receive no inferred or explicit storage. Nothing is consumed
without network supply. Profiles remain authored policy; marking a resource does
not restore original stock, spawn a generator, connect original utility geometry,
or establish physical units. Unmarked resources retain legacy behavior.

## Reproducible authored scenario

The process regression builds a fuel source, generator, powered mine, powered
processor and depot, then routes material trucks and utility connections. It can
also write a new checkpoint for the desktop:

```powershell
python -B tests/economy_host/utility_controller.py `
  build/openrepublic-vs2022/src/economy_host/Release/OpenRepublicEconomyHost.exe `
  --write-checkpoint D:/scratch/powered.checkpoint
build/openrepublic-vs2022/src/app/Release/OpenRepublic.exe `
  --economy-checkpoint D:/scratch/powered.checkpoint
```

The output directory must exist; the helper refuses to overwrite a checkpoint.
All scenario content is OpenRepublic-authored. Native construction and existing
Run/Pause, haul, road and checkpoint controls remain available after restore.
