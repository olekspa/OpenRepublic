# OpenRepublic

**An independent, open-source game engine and simulation inspired by Workers & Resources: Soviet Republic.**

OpenRepublic aims to preserve the original game's economic and logistical depth while rebuilding its client, simulation, rendering, and modding architecture. The current target stack is Wicked Engine for rendering and tools, an independent C++ simulation core, Lua for data and rules, and optional external C# AI controllers.

> OpenRepublic is an unofficial community project. It is not affiliated with, endorsed by, or a product of 3Division, Hooded Horse, or Valve. Workers & Resources: Soviet Republic and its content belong to their respective rights holders.

## Project status

**Research and feasibility. There is no OpenRepublic playable build yet.** The repository contains project direction, evidence-led research, and a staged feasibility plan, and pins Wicked Engine. Its upstream Windows editor and template sample compiled successfully under the installed VS 2022 toolchain with a local v143 override. The sample has not been launched; OpenRepublic has no app, renderer code, or game-content importer yet. The repository contains no WRSR assets or saves.

The first engineering gate is a repeatable Wicked Engine build and a blank scene. The next gate is a read-only, local content-viewer experiment. Importing some assets would not, by itself, mean that maps, saves, mods, or simulation behavior are compatible.

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
- [Toolchain readiness and first build receipt](docs/TOOLCHAIN_STATUS.md)
- [Pinned dependency and license inventory](docs/DEPENDENCIES.md)
- [Reverse-engineering evidence ledger](docs/REVERSE_ENGINEERING_LEDGER.md)
- [Project decisions](docs/DECISIONS.md)
- [Contribution and evidence rules](CONTRIBUTING.md)

## First milestones

1. [x] Pin Wicked Engine and compile its upstream editor and template sample using VS 2022/MSVC v143.
2. [ ] Launch the template sample and record the graphics backend and a blank-scene screenshot.
3. [ ] Inventory candidate formats and approved local tooling without altering the game installation.
4. [ ] Prove one representative asset/material path and one small terrain/world-coordinate path, or document the blocker and switch to a synthetic/open interchange fixture.
5. [ ] Build an OpenRepublic-owned camera, selection, and world inspector.
6. [ ] Add one end-to-end road, vehicle, cargo, storage, and production loop before broadening the simulation.

Schedules are planning ranges, not guarantees. A viewer that renders selected local assets is an early feasibility result, not a completed port.

## License

OpenRepublic-authored source and documentation are licensed under the MIT License; see [LICENSE](LICENSE). That license does not cover WRSR, third-party projects, or other dependencies and their content. Third-party notices and any future submodule licenses must be reviewed separately.
