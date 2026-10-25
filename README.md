# OpenRepublic

OpenRepublic is an independent C++ simulation and desktop client inspired by *Workers & Resources: Soviet Republic* (WRSR). It uses Wicked Engine for rendering and provides OpenRepublic-authored synthetic content, scenario tools, Lua extensions, and optional external controllers. It is a standalone project, not a WRSR mod, loader, or repackaging.

## Status

The repository contains playable OpenRepublic synthetic economy demos, a native desktop economy interface, scenario scoring, scripted and rule-based autopilots, and an optional LLM controller harness. Region mode can read selected content from a local WRSR installation for compatibility experiments. The simulation, economy formulas, construction, trade, population, utilities, rail, graphics, and controllers are independent project behavior; WRSR gameplay and visual parity remain **UNKNOWN**. The wider parity goal is active and incomplete.

Original game files and saves are not included. They remain local, read-only inputs. The project license covers OpenRepublic-authored files only; distribution rights for builds that use original content remain unresolved. See [project decisions](docs/DECISIONS.md#d-005-local-assets-are-an-experiment-not-a-release-permission) and [parity roadmap](docs/PARITY_ROADMAP.md).

## Quick start

For the native playable demos, build the Windows client with the pinned Wicked Engine submodule initialized:

```powershell
cmake -S . -B build/openrepublic-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic
```

Run the executable from its build directory so the engine's copied `Content` folder is available:

```powershell
cd build/openrepublic-vs2022
.\src\app\Release\OpenRepublic.exe --native-economy --native-coal-demo
```

The coal demo starts a prebuilt logistics loop with a depot, two trucks, border post, storage, and heating plant. To try the construction sandbox instead, launch `OpenRepublic.exe --native-economy --native-construction-sites`. Both use synthetic OpenRepublic rules. The native desktop UI includes run/pause, step, placement, roads, dispatch controls, overview, and entity inspection; see [Playing OpenRepublic](docs/PLAYING.md) for controls and the region-mode setup.

## Screenshots

<!-- Screenshot gallery placeholder. Add approved, OpenRepublic-authored captures here when available. -->

See [Playing OpenRepublic](docs/PLAYING.md) for the current demo descriptions and [graphics notes](docs/GRAPHICS.md) for rendering features and their evidence limits. This repository does not embed WRSR screenshots or assets.

## Guides

- [Playing OpenRepublic](docs/PLAYING.md): demos, controls, interface, and region mode.
- [Modding](docs/MODDING.md): catalog text, Lua scripts and rules, and scenario JSON.
- [AI controllers](docs/AI_CONTROLLERS.md): host protocol, autopilot, LLM harness, and scenario scoring.
- [FAQ](docs/FAQ.md): project scope, content, parity, and troubleshooting.
- [Current handoff](docs/HANDOFF_2026-10-24.md): dated technical status and open verification gaps.

OpenRepublic is unofficial and is not affiliated with or endorsed by 3Division, Hooded Horse, or Valve. WRSR and its content belong to their respective rights holders.

## License

OpenRepublic-authored source and documentation are licensed under MIT; see [LICENSE](LICENSE). The license does not cover WRSR, third-party projects, or dependencies and their content. Review [third-party dependencies](docs/DEPENDENCIES.md) separately.
