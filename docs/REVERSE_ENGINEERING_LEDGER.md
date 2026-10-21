# Reverse-engineering and compatibility evidence ledger

Use one entry per claim. Do not collapse observed behavior into a format/API contract. Keep detailed game-specific file hashes and proprietary analysis artifacts in private local research, not this public file.

## Status vocabulary

- **VERIFIED:** directly checked at the stated layer (for example, byte layout or repeatable output); semantics may still be unresolved.
- **OBSERVED:** reproduced in the original game or a tool under a named version/scenario; scope is limited to that observation.
- **INFERRED:** supported interpretation, not directly proven.
- **UNKNOWN:** unresolved or not yet checked.

## Active claims

| Claim | Status | Provenance/scope | Counterexample or limitation | Next action |
|---|---|---|---|---|
| OpenRepublic will use an independent client and authoritative simulation. | VERIFIED project decision | Owner's project request; recorded in `docs/DECISIONS.md`. | This is the target architecture, not implemented behavior. | Keep new interfaces independent from original process pointers. |
| Wicked Engine revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` is pinned as a submodule and used by the initial OpenRepublic app. | VERIFIED | Upstream Git commit is recorded in the repository gitlink; the OpenRepublic CMake target links the upstream `WickedEngine` target. | The initial scene is synthetic, and the engine choice is still a feasibility candidate rather than a long-term performance decision. | Keep the revision fixed; update only through a reviewed dependency change. |
| The upstream Wicked Engine editor and template sample compile on the local VS 2022/MSVC 14.44 toolchain. | VERIFIED (compile and bounded runtime) | Release/x64; local command-line property override `PlatformToolset=v143`, Windows SDK 10.0.26100.0; template opened on DirectX 12 and reached its blank scene. | The project's v145 default is unavailable here; the sample's displayed FPS was not benchmarked. Third-party source emitted warnings. | Retain the upstream build receipt and measure only with a controlled scene/workload. |
| The initial OpenRepublic application builds, reaches Wicked Engine initialization, and visibly renders its synthetic scene. | VERIFIED (build, window lifecycle, and visual smoke check) | `CMakeLists.txt` and `src/app/**`; CMake + VS 2022 17.14, MSVC v143, Windows SDK 10.0.26100.0, Release/x64. The `OpenRepublic | Synthetic Scene Prototype` window opened; log reported `Wicked Engine Initialized`; a window-specific capture showed the plane and cube; WM_CLOSE succeeded and process exit was 0. | This is a tiny synthetic scene only. Source now implements keyboard movement, wheel zoom, ray picking, and selection feedback for `OR-0001`; those interactions have not been runtime-verified. Graphics API, local game assets, terrain/world placement, and performance also remain open. The capture is not stored in the repository. | Runtime-check camera movement and picking, then establish bounded local asset and terrain import paths. |
| Current WRSR modding reports can inform selected behavior and file research. | OBSERVED (research intake) | Existing local project documents and public community reports were surveyed. | Those reports have different game builds and may contain inferred fields; they do not define an OpenRepublic API or full WRSR schema. | Promote each candidate only through a separate evidence entry with exact source, build/hash, experiment, and counterexamples in the private ledger. |
| `steam.exe -applaunch 784150` launches this installation. | OBSERVED (owner report only) | Owner statement in the project request. | Not independently launched or verified in this task; other launch routes reported failing. | If needed, test only a user-owned, controlled launch and record game/build; this is not required for the OpenRepublic build. |
| A local WRSR installation contains building/vehicle model, material, texture, and supporting definition file families. | OBSERVED (extension/layout inventory only) | Read-only inventory of one local installation; `.nmf`, `.mtl`, `.dds`, `.ini`, `.bbox`, and `.fire` families were present. | Contents were not parsed or rendered by OpenRepublic; versions, DLC/mod variants, format coverage, and standalone-use rights are not established. | Establish a documented local read path for one model/material pair and a separate terrain/placement sample before claiming import support. |
| The installed ModelViewer has mesh load, OBJ import, and save handler paths, plus an out-of-directory error branch. | VERIFIED (static control-flow review only) | Private REA/Ghidra analysis of the locally installed ModelViewer executable; exact executable identity and content hash are retained in the private evidence record. Handler paths reference mesh load/import/save routines and an error branch for a selected path outside `GAME_DIR/media_soviet`. | No runtime UI action was performed, no vanilla model was opened, and no output was consumed by OpenRepublic. The scope of the path check across file operations is unresolved. | If UI validation resumes, check one local model read-only; separately pursue an independent parser/interchange path and keep all outputs private. |
| Standalone loading of WRSR assets is permitted by Workshop modding guidance. | UNKNOWN | Official wiki page reviewed. | It discusses assets used in WRSR Workshop mods, not distribution/use in an independent standalone client. | Obtain rights-holder clarification or appropriate legal review before releasing an asset-reading runtime/content. |

## Entry template

| System | Claim | Status | Exact source path/hash/version | Address/offset/schema field | Scenario | Counterexamples | Next action |
|---|---|---|---|---|---|---|---|
|  |  | UNKNOWN |  |  |  |  |  |

## Rules

- Include the game build and executable/file identity for executable-specific claims. Keep private exact hashes out of public docs unless there is a clear reason to publish them.
- A parsed candidate list is not a complete inventory until counts, duplicates, bounds, variants, and failure handling are reconciled.
- A static code path does not prove that it is a supported API. A successful hook does not prove addresses remain stable across builds.
- A geometric road candidate or offline path plan does not prove native road connectivity or in-game access.
- Bytes can be verified while their meaning remains inferred/unknown.
- Never include decompiler output, proprietary file bytes, saves, captures containing licensed content, or database projects here.
