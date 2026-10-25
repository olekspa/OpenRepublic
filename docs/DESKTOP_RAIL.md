# Native desktop rail sandbox

Start the opt-in OpenRepublic synthetic rail catalog with:

```powershell
OpenRepublic.exe --native-economy --native-rail
```

The catalog contains a stock-producing `rail_station` building and a rail-only
`train` type. These authored station stock, train capacity and speed values are
fixture rules. WRSR rail gameplay and visual parity remain **UNKNOWN**.

## Build a short rail route

1. Choose **Place station**, then click a clear grid cell. Place a second station
   on the same row or column, with enough room between their access cells.
2. The train row lists placed station buildings. Select the two stations and
   choose **Build rail**. The desktop tool queues one authoritative `BuildRail`
   command per adjacent cell between their access cells. This first UI route is
   straight only, and is limited to 64 segments and the remaining pending-command
   budget. The economy validates each segment when the world is stepped.
3. Select a train type, the same station pair, and a stored resource, then choose
   **Dispatch train**. This queues the existing rail-mode haul-order command;
   the synthetic economy creates its train and moves it over connected rail.

Rail geometry is rendered as OpenRepublic procedural ballast, sleepers, and twin
rails. Rail-only vehicle snapshots use a procedural locomotive followed by two
procedural wagons. The scene maps each rail edge through the same four-display-
units-per-cell transform used by the native economy view and uses the shared
cell-motion interpolation policy for train movement.

## Evidence boundary

**VERIFIED** source and headless tests cover the opt-in catalog declarations,
cardinal rail-edge mapping, scene-space length/center/heading, invalid-edge
rejection, and shared movement interpolation. A successful client build proves
compilation only. Desktop appearance, controls, train animation, and graphics
acceptance remain unobserved until a separate desktop run. All catalog values,
rail behavior, geometry, and train presentation are OpenRepublic synthetic
rules; WRSR parity remains **UNKNOWN**.
