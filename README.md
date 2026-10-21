# OpenRepublic

**An independent, open-source game engine and simulation inspired by Workers & Resources: Soviet Republic.**

OpenRepublic aims to preserve the original game's economic and logistical depth while rebuilding its client, simulation, rendering, and modding architecture. The current target stack is Wicked Engine for rendering and tools, an independent C++ simulation core, Lua for data and rules, and optional external C# AI controllers.

> OpenRepublic is an unofficial community project. It is not affiliated with, endorsed by, or a product of 3Division, Hooded Horse, or Valve. Workers & Resources: Soviet Republic and its content belong to their respective rights holders.

## Project status

**Early implementation and feasibility.** The repository has an OpenRepublic-owned Windows app linked to the pinned Wicked Engine, an independent fixed-step C++ demonstration simulation, an isolated Lua rules loader, and a bounded terrain DDS reader. The synthetic farm–mill–warehouse chain carries grain and flour by truck on a road graph and supports versioned OpenRepublic save/load; v3 saves persist simulation rules, while v1/v2 migrate to defaults. The Release/x64 app has bounded runtime evidence for scene/HUD rendering, pause/step, road toggling, building selection, zoom, prototype storage placement, GUI save/load, Lua startup rules, and valid/invalid reload. Rule changes apply while paused or at the next running tick boundary; rejected values leave active rules unchanged. Developer probes import `.glb` and `.wiscene` data. `--terrain-probe <heightmap.dds>` decodes a local legacy R32F heightmap, parses adjacent terrain metadata when present, and renders a centered, decimated preview; one copied local sample rendered as a 257x257 mesh. The candidate height transform, map placement, and row orientation remain unverified. Direct WRSR `.nmf` loading, texture fidelity, terrain/world alignment, keyboard movement, and controlled performance remain open. Storage placement has no costs, construction materials, builders, or WRSR placement rules. Client CTest passes 3/3 and the separate headless build passes 2/2. The repository contains no WRSR assets or saves.

The upstream sample and current OpenRepublic-owned app build/startup/visual gate have passed. Pause/step, road toggle, selection, wheel zoom, synthetic storage placement, GUI save/load, and validated Lua rule reload were checked in bounded runtime smoke passes. The GLB probe logs successful mesh/material/transform counts for a synthetic fixture and one private local-model-derived GLB; it is not yet visual fidelity or broad asset support. Next, validate materials visually, replace the private unlicensed conversion dependency with a documented and permitted asset path, and pursue terrain/world coordinates. Importing some models does not establish map, save, mod, or simulation compatibility.

## Compatibility target

- Use original WRSR content only from a user's local, legitimate installation, subject to applicable rights and terms.
- Do not upload original game assets, executables, saves, or decompiled code.
- Begin with a bounded world and selected assets; broad game, save, and Workshop compatibility are not alpha promises.
- Keep a synthetic-content mode so development and automated builds do not require the game.
- Treat visual similarity, world import, simulation behavior, save compatibility, and Steam integration as separate capabilities with separate evidence.

Local-only loading and non-redistribution are project safeguards. They do not establish that every standalone use is permitted. The compatibility and release gates are described in [the project plan](docs/PROJECT_PLAN.md).

## Start here

- [Project plan and staged feasibility sprint](docs/PROJECT_PLAN.md)
- [Autonomous goal prompt for sustained development](docs/AUTONOMOUS_GOAL_PROMPT.md)
- [Research findings and source links](docs/RESEARCH_FINDINGS.md)
- [Transfer map from Rival Republics research](docs/TRANSFER_FROM_RIVAL_REPUBLICS.md)
- [Toolchain readiness and first build receipt](docs/TOOLCHAIN_STATUS.md)
- [Pinned dependency and license inventory](docs/DEPENDENCIES.md)
- [Reverse-engineering evidence ledger](docs/REVERSE_ENGINEERING_LEDGER.md)
- [Project decisions](docs/DECISIONS.md)
- [Contribution and evidence rules](CONTRIBUTING.md)

## Get the source

Clone with submodules initialized so the pinned Wicked Engine source is present:

```sh
git clone --recurse-submodules https://github.com/olekspa/OpenRepublic.git
```

If you already cloned without submodules, run this from the repository root:

```sh
git submodule update --init --recursive
```

The upstream and OpenRepublic build receipts are recorded in [toolchain status](docs/TOOLCHAIN_STATUS.md).

Build the initial OpenRepublic Windows app with CMake and VS 2022 (from a Developer PowerShell or command prompt):

```powershell
cmake -S . -B build/openrepublic-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143 "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic OpenRepublicSimulationTests OpenRepublicLuaRulesTests --parallel
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
```

The executable is written under `build/openrepublic-vs2022/src/app/Release/`. Run it with `build/openrepublic-vs2022/` as the working directory so Wicked Engine's copied `Content` folder is available. The build requires the initialized Wicked Engine submodule.

## First milestones

1. [x] Pin Wicked Engine and compile its upstream editor and template sample using VS 2022/MSVC v143.
2. [x] Launch the template sample; confirm DirectX 12 and the blank scene (screenshot captured in this task session).
3. [x] Survey local asset extension families and available tools read-only; no payload parsing or importing is established by this inventory.
4. [x] Visually verify the synthetic OpenRepublic plane and cube render.
5. [x] Implement the first synthetic fixed-step production and truck-delivery loop with a versioned OpenRepublic save/load API. The original simulation-only CTest receipt was 1/1; the current Windows client suite is 2/2 and headless suite is 1/1.
6. [x] Runtime-verify the current farm/mill/warehouse scene, readable HUD, pause/step, road open/close, mouse wheel zoom, and picking (`OR-1002`). Keyboard camera movement still needs an input check; the old cube capture is historical evidence only.
7. [ ] Prove one representative local asset/material path and one small terrain/world-coordinate path, or document the blocker and use a synthetic/open interchange fixture for the first viewer.
8. [x] Connect the app to simulation-owned prototype storage placement and save/load controls; runtime-check a GUI round trip. The placement prototype has no construction costs, supply chain, or WRSR behavior.
9. [x] Load project Lua rules in an isolated, bounded VM; validate exact schema and ranges; runtime-check reload at paused and running tick boundaries and preservation of active values on rejection.
10. [~] Establish a reproducible WRSR model/material and terrain/world-content path or document an evidence-backed blocker and continue with synthetic/open interchange content. A local R32F heightmap can now be decoded and previewed; model materials, verified coordinates/orientation, and complete world-content handling remain open.

`OpenRepublic.exe --glb-probe <file.glb>` imports a local GLB into the synthetic viewer using Wicked Engine's pinned Editor importer. `--scene-probe <file.wiscene>` checks Wicked Engine archive loading. `python tools/create_synthetic_glb.py` creates the small redistributable GLB fixture used for the importer check. The GLB probe currently uses an upstream Editor implementation detail; it does not load `.nmf` files directly and does not establish broad WRSR asset compatibility.

`OpenRepublic.exe --terrain-probe <heightmap.dds>` accepts legacy 2D R32F DDS (FourCC 114). If a sibling `script.ini` is present, the app validates five observed terrain metadata keys and applies the candidate `raw * heightScale + worldPositionY` transform for display. The mesh is capped at 257x257 vertices, centered, row-ordered as stored, and vertically exaggerated for visibility. No map transform, north/south orientation, or game-equivalent terrain behavior is claimed. The sample/runtime evidence and unknowns are recorded in [research findings](docs/RESEARCH_FINDINGS.md) and the [reverse-engineering ledger](docs/REVERSE_ENGINEERING_LEDGER.md).

Schedules are planning ranges, not guarantees. A viewer that renders selected local assets is an early feasibility result, not a completed port.

## License

OpenRepublic-authored source and documentation are licensed under the MIT License; see [LICENSE](LICENSE). That license does not cover WRSR, third-party projects, or other dependencies and their content. Third-party notices and any future submodule licenses must be reviewed separately.
