# OpenRepublic

**Storage integration (22 October 2026):** imported-region desktop/host modes now accept optional authored capacity profiles and supported storage-only buildings. A synthetic process test delivers to the exact depot capacity and resumes an identical checkpoint. Full Release builds succeed; CTest passes **60/60 client** and **59/59 headless**. Real-content profile/utility parity remains open. See the [storage-profile report](docs/DEVELOPMENT_REPORT_2026-10-22_STORAGE_PROFILES.md).

**Real-content follow-up:** the integrated region toolbar now has a live exact-binding/iron-haul acceptance check on one real-content region. The independent order waits for missing source stock; original production-state import and gameplay parity remain open. Vehicle labels stay readable with detailed values in tooltips. See the [real-region evidence](docs/DEVELOPMENT_REPORT_2026-10-22_REAL_REGION.md).

**Reviewed local milestone (22 October 2026):** native construction/resource flow, Run/Pause, order/road repair, strict persistence, bounded command history and app-owned region logistics are integrated. The milestone review repairs API handle exhaustion before command admission. Full Windows Release checks pass57/57 headless and58/58 client. See the [milestone review](docs/DEVELOPMENT_REPORT_2026-10-22_MILESTONE_REVIEW.md) and [region-logistics evidence](docs/DEVELOPMENT_REPORT_2026-10-22_REGION_LOGISTICS.md). Historical uncommitted/pause statements below describe earlier checkpoints. The global game goal remains incomplete; real-content and remaining save/load acceptance are open.

## Earlier continuation receipts

**Latest source integration:** native Save checkpoint pauses and exports through the typed API to a new file. Windows build and focused3/3 pass; dialog opening is observed, end-to-end GUI save remains unverified. See the [save UI report](docs/DEVELOPMENT_REPORT_2026-10-22_SAVE_UI.md).

**Latest integration:** native road selection and enable/disable controls now use the shared authoritative API. Release build/focused5/5 pass; GUI disable/reopen matches exact headless replay. See the [road-controls report](docs/DEVELOPMENT_REPORT_2026-10-22_ROAD_TOOLS.md).

**Latest source integration:** native haul-order inspection, retarget and cancellation now use exact IDs through the shared API. Release build and focused7/7 pass; GUI loaded retarget preserves truck/cargo, and compact HUD fixes default-window picking. Cancellation acceptance remains pending. See the [order-tools report](docs/DEVELOPMENT_REPORT_2026-10-22_ORDER_TOOLS.md).

**Latest integration:** native continuous Run/Pause now uses the shared authoritative API and a bounded four-ticks/second desktop clock. Focused tests pass; live Run/Pause and manual-step acceptance now matches headless replay. See the [Run/Pause receipt](docs/DEVELOPMENT_REPORT_2026-10-22_RUN_PAUSE.md). The construction receipt below retains the last full-suite and live GUI evidence.

**Latest integration:** native continuous Run/Pause now uses the shared authoritative API and a bounded four-ticks/second desktop clock. Focused tests pass; live Run/Pause and manual-step acceptance now matches headless replay. See the [Run/Pause receipt](docs/DEVELOPMENT_REPORT_2026-10-22_RUN_PAUSE.md). The construction receipt below retains the last full-suite and live GUI evidence.

**An independent, open-source game engine and simulation inspired by Workers & Resources: Soviet Republic.**

OpenRepublic aims to preserve the original game's economic and logistical depth while rebuilding its client, simulation, rendering, and modding architecture. The current target stack is Wicked Engine for rendering and tools, an independent C++ simulation core, Lua for data and rules, and optional external C# AI controllers.

> OpenRepublic is an unofficial community project. It is not affiliated with, endorsed by, or a product of 3Division, Hooded Horse, or Valve. Workers & Resources: Soviet Republic and its content belong to their respective rights holders.

## Project status

**Current continuation (22 October 2026): Development is active.** The local source over `d409d91` now includes a fresh native construction sandbox, building/road tools, explicit haul resource/vehicle choices and selected recipe progress, alongside retargeting, validated saves/access, autonomous replanning, catalog-bound checkpoints, desktop restore and command-history compaction. Full Windows Release builds succeed; CTest passes **55/55 headless** and **56/56 client**. **OBSERVED:** a Farm -> Mill -> Bakery -> Market chain built entirely through the GUI consumes bread; invalid overlapping construction reports its rejection. **VERIFIED:** its actual pipe export resumes at the exact hash and matches another 24 ticks in a fresh host. See the [native construction report](docs/DEVELOPMENT_REPORT_2026-10-22_NATIVE_CONSTRUCTION.md) for evidence and limits, including keyboard acceptance that remains UNKNOWN. Changes remain local and uncommitted; the broader playable-alpha goal remains incomplete.

Read the [initial continuation report](docs/DEVELOPMENT_REPORT_2026-10-22_LOGISTICS.md), [save-integrity report](docs/DEVELOPMENT_REPORT_2026-10-22_SAVE_INTEGRITY.md) and latest report above for scope, commands, limitations and remaining persistence goals. Existing graphics work and private DLSS experiments are summarized in [graphics status](docs/GRAPHICS.md) and the [historical handoff](docs/HANDOFF_2026-10-22.md).

### Earlier checkpoints (historical)

The dated paragraphs below preserve earlier reports. Their pause, revision, latest-capture, and next-target statements are superseded by the current continuation above and the linked development report.

**Status (22 October 2026 03:10 EDT / 07:10 UTC): Early implementation; development is paused at the owner's request and the persistent project goal remains active and incomplete.** Public `main` and `origin/main` are synchronized at `101c8a8` (`Add rules_history for replaying rule reloads`). The v7 showcase captures remain the latest sun evidence: vector `(0.643, 0.415, 0.643)` aligned to the camera, enhanced intensity 6.0, unchanged baseline versus v5, and an 11.7/255 increase in central-city-crop mean luminance. These are OpenRepublic render observations, not WRSR lighting or image-match evidence. Synthetic benchmarks report 1.1 ms/tick for 40,000 buildings without hauls and 0.080/0.282 ms/tick for 100/400 haul orders. The batch haul host method and autopilot integration are committed; the progress log reports the N22 result unchanged. Valid rule reloads now have replay history; rejected reloads are excluded. One shared uncommitted edit remains in `tests/economy/economy_bench.cpp` and has no evidence recorded yet. **Next target on resume:** inspect and validate that benchmark change, then profile whole-world sorting under haul-heavy input. A separate safe graphics increment remains an OpenRepublic-authored synthetic texture on the existing height mesh; authentic terrain and vegetation semantics remain **UNKNOWN**. This pause task ran no build, test, or capture; status-document edits are uncommitted.

The latest showcase-capture pair uses one fixed city-region center, LOD, and camera in the OpenRepublic/Wicked Engine path. Each v7 run logged 75 building instances from 37 models, 81 placements selected, 470 road links drawn as chords, one unresolved type, two failed loads, and identical 1260×748 output with 62.426% depth coverage. The enhanced preset aligns the sun with the camera's full outward direction and sets directional-light intensity to 6.0 (up from 4.2), while retaining its blue graded sky, lighter volumetric cloud cover, SSAO/FXAA/bloom, and deterministic height/slope-driven vertex tint on the verified height mesh. The v7 baseline is byte-identical to v5; the enhanced central city crop mean luminance rose by 11.7/255 versus v5. The scene still has no imported terrain surface textures, vegetation, native traffic/rail/water, or decoded curved roads; several facades remain dark. Exact inputs, source hashes, logs, and images remain in private research storage. This lighting change is an OpenRepublic presentation adjustment, not a claim about WRSR lighting or its image-success criterion.

An OpenRepublic-owned Release capture run resolved the local `school` building type and its declared model/material for WRSR build `23935965`, then saved three regular and three material-retaining force-unlit views at 1260x748. All passed the app's nonzero depth-coverage gate; the complete multi-storey school and repeated window facade are recognizable in the diagonal views. This verifies that one authentic type-based model path renders pixels in OpenRepublic. The same-input ModelViewer reference is still not camera- or bounds-matched, and no saved reference image is available in the current research capture directory, so geometry and material fidelity remain **UNKNOWN**. UV and texture-slot interpretation also remain inferred. Captures, game inputs, and exact hashes remain in private research storage and are not in this repository. OpenRepublic-authored source through `2856513` is published on `origin/main`. This does not settle rights for distributing original content or a binary release using it, and D-005 remains an open distribution gate.

The current app-owned school capture establishes a recognizable model render, not target-matched fidelity. Continue with a saved same-model reference and matched-view geometry comparison; do not treat successful import or nonzero depth coverage as visual acceptance. The `--map-lod` selector/runtime update is published in `a28ac80`; exact local inputs and captures remain private and are not in this repository. The app-owned map-region controller endpoint is source/build/test verified and has a local runtime wiring smoke; synthetic-fixture graphical picking, action buttons, status, and vehicle presentation remain unknown. Capture inputs and exact local hashes stay in private research storage.

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

For the empty OpenRepublic-authored native construction sandbox, run from that build directory:

```powershell
.\src\app\Release\OpenRepublic.exe --native-economy --economy-pipe-name OpenRepublicController
```

Choose a building and Place, connect its access cell with straight roads, then step to apply construction. Select haul source/destination, choose the shared resource and vehicle, and submit each link. Step 240 advances production. `--economy-checkpoint <file>` restores a native checkpoint with the same tools; it is exclusive with `--native-economy`. See the [construction receipt and usage](docs/DEVELOPMENT_REPORT_2026-10-22_NATIVE_CONSTRUCTION.md) and [checkpoint export playbook](docs/ECONOMY_AGENT_PLAYBOOK.md). These modes use authored primitives and native rules; construction costs, continuous run/pause and in-app file dialogs remain future work.

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

`OpenRepublic.exe --glb-probe <file.glb>` imports a local GLB into the synthetic viewer using Wicked Engine's pinned Editor importer. `--scene-probe <file.wiscene>` checks Wicked Engine archive loading. `python tools/create_synthetic_glb.py` creates the small redistributable GLB fixture used for the importer check. The GLB probe uses an upstream Editor implementation detail and does not establish broad WRSR asset compatibility. The separate `--nmf-probe <file.nmf>` path accepts only the bounded static profile and submits retained subset draw ranges through a normalized debug mesh. NMF field interpretations remain build-scoped or inferred as documented in the ledger. Developer-only `--capture-preview <absolute-output.png>` accepts up to eleven output paths, waits through 60 startup frames, and captures an isolated NMF model resolved either by `--nmf-probe` or by `--content-root <dir> --building-type <name>`. Type-based loading uses the type's declared MODEL and MATERIAL paths. `--capture-preview-zoom <factor>` accepts 0.5–2.5 and `--capture-preview-elevation <degrees>` accepts 0–60; both are capture-only and require a capture output. With no camera overrides, the existing preset directions remain unchanged. `--nmf-unlit-preview` bypasses MTL adaptation and source normals for a neutral diagnostic; `--nmf-material-unlit-preview` retains MTL colors, UVs, and optional local texture candidates while forcing unlit rendering. `--nmf-texture-root <absolute-local-directory>` supplies an explicit local content root for the experimental slot-0 texture path. That slot-to-map and UV assignment is inferred, not accepted source behavior. Captures, inputs, detailed evidence, and the asset-reading runtime remain local under D-005. Nonzero depth coverage verifies raster-path output only. Camera/bounds-matched visual acceptance, original material behavior, placement, broad format support, and standalone-use rights remain unverified.

`OpenRepublic.exe --map-package <local-map-dir> --content-root <local-content-dir> --map-region --map-radius 250 --map-lod 0 --showcase-capture <absolute-output.png> --showcase-look baseline --showcase-camera 45 24.5 1.5` captures one actual map-region render target. Use `--showcase-look enhanced` for the optional lighting/sky/cloud/fog/post-process preset; use the same map, radius, LOD, and camera arguments for a matched pair, changing only the output path and look. `--showcase-camera` is yaw degrees, elevation degrees, and distance as a multiple of the region radius. Output paths are not overwritten. The feature still shows only content the current map-region loader supports: imported buildings, sampled terrain, and decoded road links.

`OpenRepublic.exe --terrain-probe <heightmap.dds>` accepts legacy 2D R32F DDS (FourCC 114). Without an explicit transform, a sibling `script.ini` is validated when present and its candidate `raw * heightScale + worldPositionY` mapping is shown in a centered preview. For controlled experiments, pass `--terrain-transform originX originZ baseHeight sizeX sizeZ heightScale rowOrder`, where `rowOrder` is `first-row-at-min-z` or `first-row-at-max-z`. This option uses only the supplied transform for bilinear height sampling, skips adjacent metadata, and still centers the preview mesh for display. The preview grid is capped at 257x257 vertices and uses vertical exaggeration; it does not claim map placement, a validated WRSR transform, north/south orientation, or game-equivalent terrain behavior. The sample/runtime evidence and unknowns are recorded in [research findings](docs/RESEARCH_FINDINGS.md) and the [reverse-engineering ledger](docs/REVERSE_ENGINEERING_LEDGER.md).

Schedules are planning ranges, not guarantees. A viewer that renders selected local assets is an early feasibility result, not a completed port.

## License

OpenRepublic-authored source and documentation are licensed under the MIT License; see [LICENSE](LICENSE). That license does not cover WRSR, third-party projects, or other dependencies and their content. Third-party notices and any future submodule licenses must be reviewed separately.
