# Playable vehicles in imported map regions

The app-owned map-region path can opt into a synthetic OpenRepublic depot and
fleet with `--region-synthetic-fleet 1..16`. This guide covers how to inspect
and dispatch those trucks. It does not claim to reproduce original-game fleet
or logistics behavior; WRSR parity remains **UNKNOWN**.

## Start a region with trucks

```powershell
OpenRepublic.exe --content-root <local-content-root> --map-package <local-map-package> `
  --map-region --region-synthetic-fleet 2
```

This mode adds a separate authored depot beside an imported road cell. Imported
buildings stay ordinary imported buildings and are not assigned depot roles.
The truck display uses the same OpenRepublic procedural truck body, wheels,
cargo indicator, ID label, and cell-to-cell motion interpolation as the native
economy scene.

The region importer records its grid origin and cell size. The presentation maps
a vehicle cell center as `origin + (cell + 0.5) * cellSize`, then offsets that
position by the map region's presentation center and samples the imported
heightmap. This is a renderer mapping for the app-owned economy grid, not a claim
about WRSR vehicle coordinates or movement.

## Dispatch

The fleet option reveals the dispatch controls in the region toolbar. Choose an
owned truck, an imported source building, an imported destination building, a
storable resource, and an amount, then select **Dispatch**. The source and
destination lists contain imported buildings only; the synthetic depot is not
an endpoint. The command is submitted through the existing `dispatch_truck`
EconomyApi operation and applies on a simulation tick. Use **Step 1 tick** or
**Step 24 ticks** to advance the world and watch its cell-by-cell road movement.

The economy validates ownership, truck availability, endpoints, storage,
resource, route and cargo under its existing synthetic rules. The source must
have stock, the destination must have room, and the endpoints need a road route.
The selector's initial amount choices are 4 or 8 units; accepted amount is
still bounded by vehicle capacity and available storage/stock.

## Evidence boundary

**VERIFIED (OpenRepublic source and headless tests):** importer cell centers
map to the same coordinates as `NativeEconomyTransform`; non-finite origins and
unsupported cell sizes fail closed. Existing economy tests cover the
`dispatch_truck` command and fleet ownership rules.

**VERIFIED (source/build only after this increment):** the opt-in app path uses
the native economy vehicle scene visuals and motion logic, and enables the
dispatch selector only when `--region-synthetic-fleet` is provided.

**UNKNOWN:** graphical appearance on an imported map, live visual movement,
dispatch acceptance and road access on a real map, and all WRSR vehicle, depot,
road, dispatch and economy parity. Those require a controlled app session and
build-matched original-game evidence; synthetic OpenRepublic rules do not
establish them.
