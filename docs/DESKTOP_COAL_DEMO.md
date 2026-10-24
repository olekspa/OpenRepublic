# Desktop coal demo

The authored coal catalog and `--native-coal-demo` bootstrap are an opt-in
OpenRepublic synthetic fixture. Coal prices, output rates, capacities, vehicle
timing, and heating are project-authored; WRSR parity is **UNKNOWN**.

## Start

Build `OpenRepublic` using the normal Windows client build, then run:

```powershell
OpenRepublic.exe --native-economy --native-coal-demo
```

The option requires `--native-economy`. It creates a 64 by 64 native world,
places a border post, two-truck road depot, storage, and heating plant, and lays
a straight road along their access cells. The bootstrap produces and exports a
small grain batch through the economy rules to fund later coal imports. The
world remains unsaved unless the user explicitly saves a checkpoint.

## Run the logistics loop

1. Start with the command above and let the native economy run. The prebuilt
   setup has buildings OR-1 through OR-4: depot, border post, storage, plant.
2. In the Dispatch row, choose a depot truck, source, destination, resource,
   and amount, then select **Dispatch**. The truck returns to its owning depot
   after unloading. For the fixture sequence, send truck #1 from OR-2 to OR-3
   with coal, then truck #2 from OR-3 to OR-4.
3. **Buy coal** queues a four unit import at the fixture border post. Imports
   spend treasury; use **Auto-run logistics** to alternate grain exports for
   funding, coal imports, and the two truck legs continuously.
4. Select **Overview** for cumulative heat output and stock totals. Native
   status shows each depot truck's owner, current leg, and cargo.

Dispatch, imports, and exports use the app-owned typed `EconomyApi`; execution
and rejection remain authoritative at economy tick boundaries. Auto-run is a
deterministic one-command-at-a-time planner over snapshots. It does not mutate
the world directly.

`VERIFIED` source/headless coverage currently includes the built-in catalog and
the deterministic auto-run action planner. The client target and full build
compile, but desktop startup layout, control interaction, truck animation, and
visible heat production have not been run or accepted. WRSR parity remains
**UNKNOWN**.
