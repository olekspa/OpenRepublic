# Research findings: existing WRSR work and adjacent projects

**Review date:** 21 October 2026. This is a project intake note, not a claim that the listed projects or evidence prove OpenRepublic is feasible or legally cleared.

## Findings that shape the project

### Existing WRSR work is primarily an extension path

TesmioLoader and related plugins extend the running WRSR process and its existing data/runtime. This is valuable research into game build identity, file access, plugin lifecycle, and particular mechanics. It does not give OpenRepublic a stable API or remove the cost of implementing its own world, save model, rendering, simulation, and mod interface.

Keep the extension route as an optional *research harness* for controlled observations. Do not make it a runtime dependency of the standalone client. Any proposed code reuse needs a separate compatibility and license decision; this project starts with independent implementations.

### Current modding research has strict evidence boundaries

Prior local work covers some terrain channels, building and infrastructure fields, road/utility observations, save-byte candidates, and offline route/planning tools. The useful transfer is the evidence discipline and domain questions. Recovered bytes or a native call site may still have unknown semantics. A planner contract or simulator interface created by the separate project is not a WRSR contract. A static target plus a live observation from one build does not prove a public engine API or general behavior.

Transfer a candidate only with: source artifact, game build/hash if relevant, verified/observed/inferred/unknown status, counterexamples, licensing/provenance, and a new OpenRepublic acceptance check. Keep original databases, captures, extracted data, and exact proprietary hashes in private local research outside this public repository.

### Community projects worth studying

- **TesmioLoader:** plugin lifecycle and targeted native research. It injects/patches the original process; raw addresses are version-sensitive. Study its docs and published findings rather than transplanting loader internals.
- **RepublicMP:** useful design material for ordered commands, tick scheduling, logs, state sync, and explicit protocol versioning. Its WRSR plugin behavior is not evidence of cross-platform determinism or of an OpenRepublic contract.
- **TesmioLoader community plugins:** demonstrate extension of resource/deposit and selected gameplay rules. Check source and licenses per component; do not pull code or game packages into this repository without review.
- **MeitouClient:** independent game reimplementation with local content and incremental engine development. It is the closest strategic comparison in this initial survey. The repository is GPL-3.0-or-later with a license exception; it is a reference, not an OpenRepublic dependency.

### Engine and research tools

Wicked Engine's upstream repository is MIT-licensed and documents a C++ framework, rendering/editor tooling, Lua scripting, and DirectX 12 on Windows with Vulkan available. Revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` is pinned as a submodule. The upstream editor and template sample compile with the installed VS 2022/MSVC v143 override. The OpenRepublic Windows target builds and renders a synthetic farm/mill/warehouse, road/truck scene, and simulation-owned prototype storage driven from C++ snapshots. Runtime checks verified the readable HUD, pause/step, road open/close, mill selection, wheel zoom, storage placement, GUI save/load, startup Lua rules, valid reload at paused and running tick boundaries, and rejection without replacing active rules. The isolated Lua loader has source, instruction, and allocation caps; its tests pass alongside the simulation tests. The independent simulation also builds headlessly. Keyboard movement, WRSR content/world import, the OpenRepublic render API/driver, measured performance, and behavior parity remain open. The placement demo has no WRSR cost, construction, or placement semantics.

### Local asset families and the standalone import gap

A read-only inventory of the local installation found building and vehicle content families containing `.nmf` models, `.mtl` materials, `.dds` textures, and supporting `.ini`/`.bbox`/`.fire` files. This was an extension/layout inventory only: file contents were not copied into this repository, parsed, or rendered by OpenRepublic.

The official modding guidance describes using ModelViewer to import a model authored as `.obj` and save it in the game's `.nmf` format; it also describes `.mtl` and `.dds` as material and texture files. A read-only REA/Ghidra inventory of the installed ModelViewer found labels for `Load NMF`, `Import OBJ`, `Save NMF`, and `Save As NMF`, along with material and texture controls. This is **OBSERVED static evidence** of intended tool features, not proof that the current installation can load a vanilla building and its dependencies or export it for OpenRepublic. The ModelViewer documentation explains in-game mod workflows and measurements. Those pages do not establish an export path from installed vanilla assets into a standalone viewer, nor do they establish standalone-use rights. Therefore an independent `.nmf` reader/importer remains unproven. The best next probe is a bounded, local ModelViewer load/export with a simple representative model, followed by a separate OpenRepublic import check.

REA can present bounded static analysis and Ghidra operations to agents. For the focused ModelViewer question, portable REA 6.7.0, Ghidra 12.1.4, and JDK 21 were prepared outside the public checkout. REA's Ghidra and Java provider checks passed; unrelated optional providers and agent registrations are still absent. A static handler review links the `Load NMF` branch to a mesh file dialog and `LoadFromFile`, the OBJ branch to an import helper and `LoadFromMemory`, and the save branches to `SaveToFile`. One handler error branch is reached for a selected path outside `GAME_DIR/media_soviet`; static review has not established the scope of this check across all load/save operations. This makes ModelViewer a plausible local inspection route, but not an established standalone exporter: no vanilla asset was loaded, no runtime UI action was performed, and no output was consumed by OpenRepublic. The decompilation report and exact local executable identity remain private. An earlier attempted action trace using a UI string failed because REA expected a procedure or address; that failed command alone established no behavior. Windows Ghidra support remains experimental and read-only P0, so static control flow must not be presented as runtime behavior.

### Transfer from the existing Rival Republics work

The adjacent local project already contains substantial readers, terrain extraction, road/network analysis, planning, and simulation reports. The useful transfer is primarily in how OpenRepublic records evidence and structures its interfaces; the existing project is an AI/modding system operating alongside WRSR, not a standalone engine. The reviewed lessons and non-transfer boundaries are summarized in [the Rival Republics transfer map](TRANSFER_FROM_RIVAL_REPUBLICS.md).

### What the official modding page does and does not say

The official WRSR wiki explains in-game asset/mod workflows. It says WRSR models may be modified and uploaded to the game's Workshop. This is not a grant to distribute those models in a standalone client or through GitHub. Local-only loading and no asset redistribution are necessary project controls, but rights and applicable terms still need review before shipping an asset-reading runtime or a playable release.

### Community Blender NMF importer lead

A second community lead surfaced an old Blender NMF add-on and newer community beta variants. The downloaded archive has no included license, source URL, or commit provenance, so its redistribution status is unknown and none of its code is in OpenRepublic. Static review found parser bounds gaps and a `.debug.txt` sidecar write beside the selected input. In a private background Blender 5.2.2 process, a copied local NMF model imported and exported to GLB; the original installation remained read-only. This is one file/tool/version observation only. It does not establish broad NMF support, correct coordinates/material textures, an OpenRepublic runtime importer, or rights to use assets in a standalone client. Keep the add-on private and use scratch copies for any further experiments.

Wicked Engine's ordinary runtime `wi::scene::LoadModel` path deserializes `.wiscene` archives, while GLTF/GLB parsing is implemented by the separate Editor importer. OpenRepublic now builds the pinned `Editor/ModelImporter_GLTF.cpp` behind a developer-only `--glb-probe` option. `tools/create_synthetic_glb.py` produces a project-authored 896-byte GLB; a bounded runtime launch logged 1 mesh, 1 material, and 2 transforms. A separate launch with a scratch copy of the previously exported GLB from one copied local NMF logged 3 meshes, 2 materials, and 4 transforms. The `.wiscene` probe loaded the upstream engine cube scene and added one transform. These runtime logs establish importer/archive acceptance only; no visual comparison, texture resolution, broad asset coverage, direct NMF read, terrain, or coordinate path is established. The NMF-to-GLB step used a community add-on with unknown license/provenance and must not be treated as a distributable or dependable workflow. Compiling the Editor importer into the app is also an upstream implementation-detail coupling; isolate it behind a versioned OpenRepublic content interface before production use.

### Bounded terrain heightmap path

Rival Republics contains a substantial, bounds-checked DDS decoder and terrain-layer pipeline; its implementation remains private research because the local repository has no root license. OpenRepublic now has an independent C++17 reader for one deliberately narrow format: legacy single-level 2D DDS with FourCC 114 (`R32F`). It validates header flags/sizes, exact row pitch and payload length, topology, finite samples, and input/sample limits. A separate metadata parser reads `$GRIDSIZE`, `$WORLDSIZE`, `$WORLDPOSITION`, `$HEIGHTSCALE`, and `$HEIGHTMAPSIZE` without assigning terrain semantics.

On 21 October 2026, OpenRepublic parsed a copied local 2048x2048 R32F heightmap and its adjacent metadata, then rendered a 257x257 decimated static mesh in the Wicked Engine app. The preview is centered and preserves stored row order. The candidate vertical mapping is `raw * heightScale + worldPositionY`, with 5x display exaggeration; local Rival research and the community map guide support investigating that mapping, but its exact game arithmetic, map origin, axis/orientation, placement, masks, water, and buildability are not verified. This is a terrain-byte-to-preview path, not a reconstructed WRSR world or a proof of broad terrain compatibility. The official [Map making wiki](https://wiki.hoodedhorse.com/Workers_Resources_Soviet_Republic/Map_making) points to a Steam [heightmap guide](https://steamcommunity.com/sharedfiles/filedetails/?id=1835594235) by LovelyPL. The page is currently marked removed; its 2019 text historically describes 2048x2048 greyscale heightmaps, R32F with no mipmaps, and optional map-editor scale/offset controls. Treat that community guide as a historical lead, not a current official format contract.

## Initial approach decision

Proceed with an independent standalone client and a headless C++ simulation as the target. Use Wicked Engine for the first rendering experiment. Treat a local content viewer as a feasibility probe, not as the final compatibility layer. Preserve a synthetic/open content path if content rights or proprietary format support blocks the WRSR viewer.

Do not begin with multiplayer, full save conversion, DLSS/RTX features, one-million-citizen benchmarks, or a rewrite of all WRSR formulas. The first vertical slice is one bounded world plus one end-to-end logistics and production loop. A synthetic version of that loop now exists in C++; local WRSR assets and production formulas have not been connected.

## Sources

- [REA](https://github.com/morluto/rea) and its [installation guide](https://github.com/morluto/rea/blob/main/docs/installation.md) / [Windows Ghidra guide](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
- [Wicked Engine](https://github.com/turanszkij/WickedEngine)
- [TesmioLoader](https://github.com/MaxLegend/TesmioLoader) and its [architecture](https://github.com/MaxLegend/TesmioLoader/blob/master/docs/01-architecture.md) / [RE notes](https://github.com/MaxLegend/TesmioLoader/blob/master/docs/03-reverse-engineering.md)
- [RepublicMP](https://github.com/E3418/RepublicMP)
- [TesmioLoader community plugins](https://github.com/Kespri/WRSR-MODS)
- [Official WRSR modding documentation](https://wiki.hoodedhorse.com/Workers_Resources_Soviet_Republic/General_modding)
- [Official ModelViewer documentation](https://wiki.hoodedhorse.com/Workers_Resources_Soviet_Republic/Modelviewer)
- [Official Map making wiki](https://wiki.hoodedhorse.com/Workers_Resources_Soviet_Republic/Map_making) and [developer-linked heightmap guide](https://steamcommunity.com/sharedfiles/filedetails/?id=1835594235)
- [MeitouClient](https://github.com/VlcekM/MeitouClient)
