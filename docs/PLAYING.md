# Playing OpenRepublic

OpenRepublic currently offers synthetic economy sandboxes and an experimental local-content region view. These are OpenRepublic features. **WRSR gameplay and visual parity are UNKNOWN.** The native economy is C++ authoritative; the desktop controls submit actions to that simulation.

## Build and launch

On Windows, initialize the pinned Wicked Engine submodule, configure a Visual Studio 2022 client build, and build the desktop executable:

```powershell
git submodule update --init --recursive
cmake -S . -B build/openrepublic-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic
```

Launch from the build directory so Wicked Engine can find its copied `Content` folder:

```powershell
Set-Location build/openrepublic-vs2022
.\src\app\Release\OpenRepublic.exe --native-economy --native-coal-demo
```

The exact executable subdirectory can vary by generator. The launch examples below assume the current directory contains `src\app\Release\OpenRepublic.exe`.

## Playable demos

### Native sandbox

```powershell
OpenRepublic.exe --native-economy
```

This opens an OpenRepublic-authored synthetic economy world. Use the building chooser and **Place building** mode to place a building on a valid ground cell. Use road placement to connect building access cells. Commands enter the simulation queue and take effect at simulation tick boundaries. Step the world to see production and hauling progress. This sandbox uses the built-in synthetic catalog; it does not read WRSR files.

### Coal logistics demo

```powershell
OpenRepublic.exe --native-economy --native-coal-demo
```

This creates a 64 by 64 world with a road depot and two trucks, a border post, storage, a heating plant, and connecting roads. The bootstrap exports a small grain batch to fund coal imports. Start **Run** and use **Dispatch** to send truck #1 from the border post (OR-2) to storage (OR-3) with coal, then truck #2 from storage (OR-3) to the plant (OR-4). **Buy coal** queues a four-unit import. **Auto-run logistics** runs the deterministic planner that alternates exports, imports, and truck dispatches. **Follow truck** tracks the selected dispatch truck. The **Overview** panel reports heat output and stocks. The world is not saved unless you explicitly save a checkpoint.

The demo is fully synthetic: prices, production, capacities, heating, and travel timing are authored project values. WRSR parity is **UNKNOWN**. See [Desktop coal demo](DESKTOP_COAL_DEMO.md).

### Construction sites demo

```powershell
OpenRepublic.exe --native-economy --native-construction-sites
```

This selects an opt-in synthetic catalog containing a sawmill and brickworks that make construction materials. Mill, bakery, and market placements with a `build` declaration start as sites. Connect material producers to each site using roads and haul orders; when required materials arrive, work advances by simulation ticks. The inspector reports site state, progress, and held/required materials. The source and synthetic scenario path are covered; GUI delivery-to-completion has not been fully accepted. WRSR construction behavior and imported `$COST_*` meanings remain **UNKNOWN**. See [Construction sites](CONSTRUCTION_SITES.md).

### Region mode with a local WRSR installation

Region mode accepts local content and map directories for read-only compatibility experiments. It does not write to the game installation or saves. In PowerShell, substitute the actual local directories:

```powershell
OpenRepublic.exe --content-root "C:\path\to\WRSR\media_soviet" --map-package "C:\path\to\map-package" --map-region --map-radius 450
```

The app uses the bounded readers and current importer to display supported buildings, sampled terrain, and decoded roads in a selected region. In app-owned region economy mode, accepted placements and roads also seed the OpenRepublic economy. Import coverage is partial: skipped or unsupported content may be absent, production/runtime state is not reconstructed as a complete game, and imported data does not establish original formulas or semantics. Use a legitimate local install and keep its files read-only. Asset use in a standalone client does not itself establish permission to redistribute content or binaries; see [D-005](DECISIONS.md#d-005-local-assets-are-an-experiment-not-a-release-permission), [compatibility catalog](COMPAT_CATALOG.md), and the [handoff](HANDOFF_2026-10-24.md).

## Desktop controls and interface

The native economy toolbar exposes controls according to the active mode and window size. The exact arrangement can reflow into columns.

- **Run / Pause** starts or pauses the native economy clock. The native desktop clock advances at a bounded four ticks per second.
- **Step 1 tick**, **Step 24 ticks**, and **Step 240 ticks** advance a fixed number of simulation ticks. Some modes show a subset of these controls.
- **Frame world** reapplies the camera framing for the current economy scene. **Follow truck** toggles camera tracking for the selected Dispatch truck.
- Building controls choose a catalog type and enter placement mode. Click a valid ground cell to queue placement. In the construction sandbox, types with construction declarations are placed as sites. **Build road** enters road placement mode; **Cancel build** exits placement mode.
- **Dispatch** sends a depot-owned truck between a selected source and destination for a selected resource and amount. **Buy coal** and **Auto-run logistics** appear in the coal demo.
- **Overview** summarizes current tick and treasury, cumulative synthetic heat, stored resources and recent stock changes, population/workforce when enabled, construction sites and their status, and active haul/truck counts. It reports economy snapshot values, not WRSR statistics.
- **Details** expands the selected entity inspection text; **Compact** collapses it. Selecting a building shows its type, ID/location, enabled state, inventory/capacity, recipe/stall information, and, where applicable, construction materials/progress. In the coal demo, the depot-truck status lists each truck's owner, current leg, and cargo. Exact fields depend on the selected entity and feature declarations.
- The scene draws roads, buildings, and vehicles from the current OpenRepublic snapshots. Vehicle motion is interpolated for presentation between cell snapshots; that path is not proof of an original-game route or vehicle behavior. See [desktop traffic notes](DESKTOP_TRAFFIC.md).

The toolbar and inspector present the state of the OpenRepublic simulation. A control being visible does not mean its action has succeeded: queued commands are validated at tick boundaries, and status reports whether a command applied or was rejected. Camera, appearance, and interaction acceptance should be reported separately from source/build verification.

## Saves and checkpoints

Native economy checkpoints and the older simulation demo saves are OpenRepublic formats, not WRSR saves. Use the in-app checkpoint controls when available and choose a new output path. Do not point OpenRepublic at a WRSR save expecting it to load it. See [economy API/checkpoints](ECONOMY_API.md) and [checkpoint playbook](ECONOMY_AGENT_PLAYBOOK.md).

## Current limits

- Synthetic economy rules are intentionally partial. Recipes, prices, population, construction sites, utilities, calendar, depot dispatch, and rail are OpenRepublic-authored. Their WRSR parity is **UNKNOWN**.
- Region import supports bounded content paths; it does not recreate every map object, building state, gameplay rule, or save state.
- Some desktop actions and visuals have source/headless evidence without controlled live graphical acceptance. The handoff and feature guides state the latest evidence.
- Do not treat scenario scores, state hashes, successful builds, or a rendered preview as proof of original-game behavior or compatibility.
- Do not distribute WRSR assets, saves, extracted payloads, or decompiled material. Original-content and binary distribution rights remain unresolved.
