# OpenRepublic

**An independent, open-source game engine and simulation inspired by Workers & Resources: Soviet Republic.**

OpenRepublic aims to preserve the original game's economic and logistical depth while rebuilding its client, simulation, rendering, and modding architecture. The current target stack is Wicked Engine for rendering and tools, an independent C++ simulation core, Lua for data and rules, and optional external C# AI controllers.

> OpenRepublic is an unofficial community project. It is not affiliated with, endorsed by, or a product of 3Division, Hooded Horse, or Valve. Workers & Resources: Soviet Republic and its content belong to their respective rights holders.

## Project status

**Early implementation and feasibility; development paused at the project owner's request (21 October 2026).** The repository has an OpenRepublic-owned Windows app linked to the pinned Wicked Engine, an independent fixed-step C++ simulation, an isolated Lua rules loader, and bounded terrain/model research probes. The synthetic farm–mill–storage chain carries grain and flour by truck on a road graph. Save v7 persists inventories, active routes, and pending commands, with v1–v6 migrations covered. The last completed client/headless Release CTest receipts are 15/15 and 14/14, respectively, from before the latest mixed-normal regression assertion. The client Release rebuild including that assertion succeeded; CTest has not been rerun, and the headless build has not been rebuilt after it. Agent API v0 is exposed to a separate local process through a bounded JSONL stdio host and a Python controller; it verifies delivery in the synthetic world. The host defaults to observer permissions; `--role controller` selects a local controller permission profile and is not authentication. There is no network server, MCP adapter, or C# client; see [Agent API v0](docs/AGENT_API.md). The bounded NMF reader and preview remain limited research tools. Developer-only captures support up to eleven views with optional zoom and elevation; omitted camera overrides preserve the existing views. Optional local texture preview is experimental, and slot-to-map/UV interpretation remains inferred. A private same-input ModelViewer reference shows the complete school after the user zoomed out, but camera/bounds matching and visual acceptance remain open. The asset-reading runtime remains local under D-005. Terrain/world alignment, broad compatibility, performance, and rights review remain open. The repository contains no WRSR assets or saves. The local integration work is not published as a new release: the shared synthetic simulation and Agent API are already on the public baseline, while the unpublished NMF commits remain gated by D-005.

The OpenRepublic app build/startup and bounded UI smoke checks have passed; those checks do not establish target-matched NMF preview acceptance. Pause/step, road toggle, selection, wheel zoom, synthetic storage placement, GUI save/load, and validated Lua rule reload were checked in bounded runtime smoke passes. Private captures verify nonzero depth coverage through the OpenRepublic render target, but do not establish recognizable geometry or fidelity. The same-input ModelViewer reference now shows the complete building, though its camera and bounds are not aligned with the OpenRepublic captures. Optional local texture loading has been exercised as a preview experiment; its map and UV assignments remain inferred. NMF visual acceptance and the D-005 publication gate remain open. Importing a model does not establish map, save, mod, or simulation compatibility.

## Compatibility target

- Use original WRSR content only from a user's local, legitimate installation, subject to applicable rights and terms.
- Do not upload original game assets, executables, saves, or decompiled code.
- Begin with a bounded world and selected assets; broad game, save, and Workshop compatibility are not alpha promises.
- Keep a synthetic-content mode so development and automated builds do not require the game.
- Treat visual similarity, world import, simulation behavior, save compatibility, and Steam integration as separate capabilities with separate evidence.

Local-only loading and non-redistribution are project safeguards. They do not establish that every standalone use is permitted. The compatibility and release gates are described in [the project plan](docs/PROJECT_PLAN.md).

## Start here

- [Project plan and staged feasibility sprint](docs/PROJECT_PLAN.md)
- [Typed Agent API v0 and local JSONL host contract and limits](docs/AGENT_API.md)
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
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic OpenRepublicAgentHost OpenRepublicSimulationTests OpenRepublicAgentApiTests OpenRepublicAgentProtocolTests OpenRepublicTerrainReaderTests OpenRepublicMtlSyntaxTests OpenRepublicMtlDependencyTests OpenRepublicNmfHeaderTests OpenRepublicNmfGeometryTests OpenRepublicLuaRulesTests --parallel
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
11. [x] Add the typed C++ Agent API over the authoritative simulation and a bounded local JSONL stdio host, then prove that a separate Python process can inspect and control the synthetic logistics scenario without GUI automation. Network access, MCP, C#, authentication, and WRSR behavior remain future work.
12. [x] Add a metadata-only MTL dependency manifest for recognized `$TEXTURE` directives with caller-supplied roots, ambiguity and traversal handling, and synthetic tests. This does not interpret texture slots or establish WRSR path semantics.
13. [x] Add a bounded NMF header profile reader with synthetic fixtures. It reads the fixed prefix and identifier table only.
14. [x] Add a bounded static type-0 NMF geometry reader and normalized local preview path, with synthetic coverage for single-LOD nodes, raw metadata, auxiliary streams, subset draw ranges, table references, and opaque auxiliary subset indices. The profile is limited to root/no-child nodes with identity raw matrix fields; it preserves but does not trust the type-0 +4 field as a payload bound. The preview matches referenced NMF table identifiers to sibling MTL section names by exact token and file-order-independent lookup. Unique matches convert parseable `$DIFFUSECOLOR` values to Wicked base colors through an approximate preview adapter; source color-space, alpha, lighting, and shader equivalence are unknown. Missing or duplicate names and float-unrepresentable values remain diagnostic. Optional local texture loading remains an experimental inferred adapter under D-005 and is not original shader parity. Field and attribute semantics not established by loader evidence remain neutral or inferred.
15. [x] Run and inspect bounded local Release NMF preview captures. The capture path validates nonzero main-depth coverage for every requested view. Expanded multi-view runs and capture-only zoom/elevation controls support visual comparison, but no camera/bounds-matched comparison has established geometry, material, or visual fidelity. Inputs and captures remain outside Git under D-005.
16. [ ] Compare the same private input in OpenRepublic and its local viewer at matched camera and bounds, then record only the differences supported by that comparison. A qualitative same-input reference exists, but target-matched acceptance remains open; keep source-normal, placement, and standalone-use-rights claims separate.

The NMF preview preserves two loader-labelled vertex streams using an inferred float-pair/UV adapter, covered by synthetic tests. Optional local texture loading is also an experiment; texture-slot-to-map and slot-to-UV relationships remain inferred. The private same-input ModelViewer reference and OpenRepublic captures are not matched for camera or bounds, so recognition and visual fidelity remain **UNKNOWN**.

The preview's CPU geometry transform and flattening handoff is independently testable in `BuildNmfPreviewGeometry`: it records source-position extents, preserves object/index order and subset draw ranges, retains mixed source-normal presence, and reports structural zero-area/normal-magnitude diagnostics. The runtime writes a compact geometry audit line with object/vertex/index counts, expected complete draw-range indices and triangles, subset tails, extents, zero-area count, and source-normal lengths, followed by a separate limits line. These are CPU-side expectations, not GPU draw or rasterization telemetry. Synthetic tests and sequential client/headless Release suites cover this OpenRepublic adapter behavior; they do not establish source axes, winding, dimensions, normal conventions, or visual fidelity.

`OpenRepublic.exe --glb-probe <file.glb>` imports a local GLB into the synthetic viewer using Wicked Engine's pinned Editor importer. `--scene-probe <file.wiscene>` checks Wicked Engine archive loading. `python tools/create_synthetic_glb.py` creates the small redistributable GLB fixture used for the importer check. The GLB probe uses an upstream Editor implementation detail and does not establish broad WRSR asset compatibility. The separate `--nmf-probe <file.nmf>` path accepts only the bounded static profile and submits retained subset draw ranges through a normalized debug mesh. NMF field interpretations remain build-scoped or inferred as documented in the ledger. Developer-only `--capture-preview <absolute-output.png>` accepts up to eleven output paths, waits through 60 startup frames, and captures the isolated NMF scene to OpenRepublic render targets. `--capture-preview-zoom <factor>` accepts 0.5–2.5 and `--capture-preview-elevation <degrees>` accepts 0–60; both are capture-only and require a capture output. With no camera overrides, the existing preset directions remain unchanged. `--nmf-unlit-preview` bypasses MTL adaptation and source normals for a neutral diagnostic; `--nmf-material-unlit-preview` retains MTL colors, UVs, and optional local texture candidates while forcing unlit rendering. `--nmf-texture-root <absolute-local-directory>` supplies an explicit local content root for the experimental slot-0 texture path. That slot-to-map and UV assignment is inferred, not accepted source behavior. Captures, inputs, detailed evidence, and the asset-reading runtime remain local under D-005. Nonzero depth coverage verifies raster-path output only. Camera/bounds-matched visual acceptance, original material behavior, placement, broad format support, and standalone-use rights remain unverified.

`OpenRepublic.exe --terrain-probe <heightmap.dds>` accepts legacy 2D R32F DDS (FourCC 114). Without an explicit transform, a sibling `script.ini` is validated when present and its candidate `raw * heightScale + worldPositionY` mapping is shown in a centered preview. For controlled experiments, pass `--terrain-transform originX originZ baseHeight sizeX sizeZ heightScale rowOrder`, where `rowOrder` is `first-row-at-min-z` or `first-row-at-max-z`. This option uses only the supplied transform for bilinear height sampling, skips adjacent metadata, and still centers the preview mesh for display. The preview grid is capped at 257x257 vertices and uses vertical exaggeration; it does not claim map placement, a validated WRSR transform, north/south orientation, or game-equivalent terrain behavior. The sample/runtime evidence and unknowns are recorded in [research findings](docs/RESEARCH_FINDINGS.md) and the [reverse-engineering ledger](docs/REVERSE_ENGINEERING_LEDGER.md).

Schedules are planning ranges, not guarantees. A viewer that renders selected local assets is an early feasibility result, not a completed port.

## License

OpenRepublic-authored source and documentation are licensed under the MIT License; see [LICENSE](LICENSE). That license does not cover WRSR, third-party projects, or other dependencies and their content. Third-party notices and any future submodule licenses must be reviewed separately.
