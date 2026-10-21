# OpenRepublic

**An independent, open-source game engine and simulation inspired by Workers & Resources: Soviet Republic.**

OpenRepublic aims to preserve the original game's economic and logistical depth while rebuilding its client, simulation, rendering, and modding architecture. The current target stack is Wicked Engine for rendering and tools, an independent C++ simulation core, Lua for data and rules, and optional external C# AI controllers.

> OpenRepublic is an unofficial community project. It is not affiliated with, endorsed by, or a product of 3Division, Hooded Horse, or Valve. Workers & Resources: Soviet Republic and its content belong to their respective rights holders.

## Project status

**Early implementation and feasibility.** The repository now has a small OpenRepublic-owned Windows application linked to the pinned Wicked Engine. It builds in Release/x64 and reaches Wicked Engine initialization; a window-specific capture visually confirmed its synthetic plane and cube. The app source now implements keyboard camera movement, wheel zoom, and ray picking for the synthetic cube as stable ID `OR-0001`, including a selection highlight and Escape-to-clear behavior. The Release build passed after those controls were added, but a follow-up UI interaction check was interrupted and the controls are not yet runtime-verified. There is no WRSR-content importer, and the repository contains no WRSR assets or saves.

The upstream build/sample gate and the first OpenRepublic-owned build/startup/visual gate have passed. The immediate next gate is a runtime check of the implemented camera and picking controls, followed by a bounded local WRSR asset/material and terrain/coordinate investigation. Importing some assets would not, by itself, mean that maps, saves, mods, or simulation behavior are compatible.

## Compatibility target

- Use original WRSR content only from a user's local, legitimate installation, subject to applicable rights and terms.
- Do not upload original game assets, executables, saves, or decompiled code.
- Begin with a bounded world and selected assets; broad game, save, and Workshop compatibility are not alpha promises.
- Keep a synthetic-content mode so development and automated builds do not require the game.
- Treat visual similarity, world import, simulation behavior, save compatibility, and Steam integration as separate capabilities with separate evidence.

Local-only loading and non-redistribution are project safeguards. They do not establish that every standalone use is permitted. The compatibility and release gates are described in [the project plan](docs/PROJECT_PLAN.md).

## Start here

- [Project plan and staged feasibility sprint](docs/PROJECT_PLAN.md)
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
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic --parallel
```

The executable is written under `build/openrepublic-vs2022/src/app/Release/`. Run it with `build/openrepublic-vs2022/` as the working directory so Wicked Engine's copied `Content` folder is available. The build requires the initialized Wicked Engine submodule.

## First milestones

1. [x] Pin Wicked Engine and compile its upstream editor and template sample using VS 2022/MSVC v143.
2. [x] Launch the template sample; confirm DirectX 12 and the blank scene (screenshot captured in this task session).
3. [x] Survey local asset extension families and available tools read-only; no payload parsing or importing is established by this inventory.
4. [x] Visually verify the synthetic OpenRepublic plane and cube render.
5. [ ] Runtime-verify implemented keyboard camera movement, wheel zoom, and object picking (`OR-0001`); Release build passes, but the interaction check is pending.
6. [ ] Prove one representative local asset/material path and one small terrain/world-coordinate path, or document the blocker and use a synthetic/open interchange fixture for the first viewer.
7. [ ] Add one end-to-end road, vehicle, cargo, storage, and production loop before broadening the simulation.

Schedules are planning ranges, not guarantees. A viewer that renders selected local assets is an early feasibility result, not a completed port.

## License

OpenRepublic-authored source and documentation are licensed under the MIT License; see [LICENSE](LICENSE). That license does not cover WRSR, third-party projects, or other dependencies and their content. Third-party notices and any future submodule licenses must be reviewed separately.
