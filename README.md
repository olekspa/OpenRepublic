# OpenRepublic

**An independent, open-source game engine and simulation inspired by Workers & Resources: Soviet Republic.**

OpenRepublic aims to preserve the original game's economic and logistical depth while rebuilding its client, simulation, rendering, and modding architecture. The current target stack is Wicked Engine for rendering and tools, an independent C++ simulation core, Lua for data and rules, and optional external C# AI controllers.

> OpenRepublic is an unofficial community project. It is not affiliated with, endorsed by, or a product of 3Division, Hooded Horse, or Valve. Workers & Resources: Soviet Republic and its content belong to their respective rights holders.

## Project status

**Early implementation and feasibility.** The repository has an OpenRepublic-owned Windows app linked to the pinned Wicked Engine, an independent fixed-step C++ simulation, an isolated Lua rules loader, and bounded terrain/model research probes. The synthetic farm–mill–storage chain carries grain and flour by truck on a road graph. Save v7 persists inventories, active routes, and pending commands, with v1–v6 migrations covered. The current Release client passes 14/14 CTest checks; the separate headless build passes 13/13. The typed C++ Agent API v0 is exposed to a separate local process through a bounded JSONL stdio host. Its Python controller builds roads and storage, assigns a transport plan, checks command outcomes/events, observes the active synthetic rules and cumulative flour-delivery counter, and verifies delivery in the synthetic world. The host defaults to observer permissions; `--role controller` selects a local controller permission profile and is not authentication. There is no network server, MCP adapter, or C# client; see [Agent API v0](docs/AGENT_API.md). A metadata-only MTL dependency resolver now creates an ordered manifest for recognized `$TEXTURE` directives using caller-supplied roots with an aggregate cap of 262,144 candidate path/root inspections per manifest; texture slots, `$TEXTURE_MTL`, and original-game path semantics remain unknown. A bounded NMF header reader and a narrow static type-0 geometry profile cover a build-scoped layout. The preview accepts only root nodes without children, identity raw matrix fields, and exactly one LOD per node; multiple LODs are rejected because the inspected build selects one LOD per draw. The parser preserves the unproven object +4 field without using it as a type-0 geometry bound, then parses count-driven streams to the declared file end. LOD fields +0x10 and +0x18 remain neutral because their meanings are inferred. Static loader evidence from build `23935965` identifies flag `0x8` as the `_NRM` float3 vertex stream; the preview now uses its raw values when present, with Wicked smooth normals as a fallback. The optional loader coordinate reflection is not applied, and cross-build meaning and shading parity remain unknown. The `--nmf-probe` path creates a normalized triangle preview and binds NMF subset identifiers to sibling MTL sections by exact name, independent of file order, then converts parseable matched `$DIFFUSECOLOR` values to Wicked base colors as an approximate preview adapter; source color-space, alpha, lighting, and shader equivalence are unknown. Missing or duplicate names and float-unrepresentable values remain diagnostic; if sibling MTL input is unavailable, names are not counted as missing. Texture slots and path interpretation remain unsupported. This bridge does not reproduce original shader behavior, and target-matched visual acceptance remains unknown. Other object and vertex-set variants, original material and texture interpretation, source placement, visual fidelity, broad support, and standalone-use rights remain open. Bounded runtime checks cover the scene/HUD, pause/step, road controls, picking, placement, GUI save/load, and Lua reload. A later synthetic UI run routed flour to a newly placed OpenRepublic storage, but spatial picking of that storage and a UI save/reload after the route change remain unverified. `--terrain-probe <heightmap.dds>` decodes legacy R32F DDS and renders a centered, decimated preview. A new caller-supplied transform option feeds the explicit transform into the bilinear preview sampler; synthetic tests cover it, while its app-rendered result and any real map transform remain unverified. The original content, asset rights, broad WRSR `.nmf` variants and material/texture fidelity, terrain/world alignment, controlled performance, and full alpha acceptance remain open. Storage placement has no costs, construction materials, builders, or WRSR placement rules. The repository contains no WRSR assets or saves.

The OpenRepublic app build/startup and bounded UI smoke checks have passed; those checks do not establish target-matched NMF preview acceptance. Pause/step, road toggle, selection, wheel zoom, synthetic storage placement, GUI save/load, and validated Lua rule reload were checked in bounded runtime smoke passes. The GLB probe logs successful mesh/material/transform counts for a synthetic fixture and one private local-model-derived GLB; this is not visual fidelity or broad asset support. The NMF preview performs the evidence-supported exact-name join and uses an approximate adapter for parseable matched `$DIFFUSECOLOR` values; source color-space, alpha, lighting, and shader equivalence remain unknown. Two private, application-owned 1260x748 captures from one NMF/MTL pair associated with installed build `23935965` show dark, stepped, low-poly building-like geometry from opposing diagnostic diagonal views. The captures demonstrate render-target readback, but recognizability, fidelity, and a matched-camera ModelViewer comparison remain unverified. Texture loading awaits path/slot evidence and the rights gate. Terrain/world coordinates and full compatibility remain separate. Importing a model does not establish map, save, mod, or simulation compatibility.

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
14. [x] Add a bounded static type-0 NMF geometry reader and normalized local preview path, with synthetic coverage for single-LOD nodes, raw metadata, auxiliary streams, subset draw ranges, table references, and opaque auxiliary subset indices. The profile is limited to root/no-child nodes with identity raw matrix fields; it preserves but does not trust the type-0 +4 field as a payload bound. The preview matches referenced NMF table identifiers to sibling MTL section names by exact token and file-order-independent lookup. Unique matches convert parseable `$DIFFUSECOLOR` values to Wicked base colors through an approximate preview adapter; source color-space, alpha, lighting, and shader equivalence are unknown. Missing or duplicate names and float-unrepresentable values remain diagnostic. Textures are not loaded and this is not original shader parity. Field and attribute semantics not established by loader evidence remain neutral or inferred.
15. [x] Run and inspect a bounded local Release `--nmf-probe` smoke capture for one private NMF/MTL pair associated with installed build `23935965`. Two application-owned 1260x748 render-target captures from opposing diagnostic diagonal views show dark, stepped, low-poly building-like geometry. This verifies a nonblank render-target readback for the sample; recognition, fidelity, and matched-camera reference comparison remain unverified. Inputs and captures remain outside Git.
16. [ ] Compare the same permitted model in OpenRepublic and its local viewer at a fixed camera, then record geometry/material differences while keeping source-normal limits, placement, and standalone-use-rights claims separate.

`OpenRepublic.exe --glb-probe <file.glb>` imports a local GLB into the synthetic viewer using Wicked Engine's pinned Editor importer. `--scene-probe <file.wiscene>` checks Wicked Engine archive loading. `python tools/create_synthetic_glb.py` creates the small redistributable GLB fixture used for the importer check. The GLB probe currently uses an upstream Editor implementation detail and does not establish broad WRSR asset compatibility. The separate `--nmf-probe <file.nmf>` path accepts only the bounded static type-0 profile and submits retained subset draw ranges through one normalized debug mesh, matching referenced table identifiers to sibling MTL `$SUBMATERIAL` names by exact token equality and converting uniquely matched parseable `$DIFFUSECOLOR` values to Wicked base colors through an approximate preview adapter; source color-space, alpha, lighting, and shader equivalence remain unknown. For loader build `23935965`, flag `0x8` is observed as the `_NRM` float3 stream, which the preview copies to Wicked's per-vertex normal data before render-data upload; absent streams use Wicked's generated smooth normals. The source loader's optional coordinate reflection is not applied. A developer-only `--capture-preview <absolute-output.png>` option can be repeated once for the second view; it waits through 60 complete startup frames, isolates the NMF scene from synthetic content, frames the normalized model bounds in two opposing diagnostic diagonal views, and saves OpenRepublic's own 3D render result without GUI or clipboard capture while showing a nonactivating window. Two 1260x748 views of one private NMF/MTL pair associated with installed build `23935965` were inspected and show dark, stepped, low-poly building-like geometry. A matched-camera ModelViewer comparison and visual-equivalence claim remain open. The PNG conversion clips high dynamic range values. Missing or ambiguous material names and float-unrepresentable values retain diagnostic colors; unavailable sibling MTL input is not counted as missing. Textures are not loaded, and this does not reproduce original shader behavior. Target-matched visual comparison, original materials, placement, general format support, visual equivalence, and standalone-use rights remain unverified.

`OpenRepublic.exe --terrain-probe <heightmap.dds>` accepts legacy 2D R32F DDS (FourCC 114). Without an explicit transform, a sibling `script.ini` is validated when present and its candidate `raw * heightScale + worldPositionY` mapping is shown in a centered preview. For controlled experiments, pass `--terrain-transform originX originZ baseHeight sizeX sizeZ heightScale rowOrder`, where `rowOrder` is `first-row-at-min-z` or `first-row-at-max-z`. This option uses only the supplied transform for bilinear height sampling, skips adjacent metadata, and still centers the preview mesh for display. The preview grid is capped at 257x257 vertices and uses vertical exaggeration; it does not claim map placement, a validated WRSR transform, north/south orientation, or game-equivalent terrain behavior. The sample/runtime evidence and unknowns are recorded in [research findings](docs/RESEARCH_FINDINGS.md) and the [reverse-engineering ledger](docs/REVERSE_ENGINEERING_LEDGER.md).

Schedules are planning ranges, not guarantees. A viewer that renders selected local assets is an early feasibility result, not a completed port.

## License

OpenRepublic-authored source and documentation are licensed under the MIT License; see [LICENSE](LICENSE). That license does not cover WRSR, third-party projects, or other dependencies and their content. Third-party notices and any future submodule licenses must be reviewed separately.
