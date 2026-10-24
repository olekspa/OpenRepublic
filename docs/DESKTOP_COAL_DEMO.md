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

`VERIFIED` source/headless coverage currently includes the built-in catalog and
the deterministic auto-run action planner. Desktop startup layout, control
interaction, truck animation, and visible heat production have not been run or
accepted in this change. WRSR parity remains **UNKNOWN**.
