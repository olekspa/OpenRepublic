# Toolchain readiness

**Checked:** 21 October 2026
**Scope:** Windows development workstation and the OpenRepublic workspace. This inventory includes the successful initial app build below; tool versions remain specific to this workstation.

| Component | Observed state | OpenRepublic impact |
|---|---|---|
| Git | Available | Ready for source control. |
| GitHub CLI | Available and authenticated as the project owner | Ready for repository publication. Credentials are not recorded here. |
| Visual Studio | Visual Studio 2022 Community 17.14.37806.7 | Installed; use a VS Developer PowerShell/command prompt for MSVC environment. |
| MSVC | 14.44.35207, x64 tools | Installed. `cl` is not on a normal shell PATH, which is expected outside a developer environment. |
| Windows SDK | 10.0.19041.0, 10.0.22621.0, 10.0.26100.0 | Explicitly selected 10.0.26100.0 for the repeat upstream build below. |
| CMake | 4.4.3 available through an existing local toolchain | Configured the initial OpenRepublic Windows target with the Visual Studio 17 2022 generator. |
| Ninja | Not found on PATH | Not needed if the first build uses the upstream Visual Studio solution/generator. Revisit only if the selected workflow requires it. |
| Python | 3.12.10 | Available for bounded local format/benchmark helpers. |
| Blender | Installed launcher 4.3; portable Blender 5.2.2 downloaded from the official Blender release server and extracted under private research tools, SHA-256 verified against Blender's published checksum. | The 5.2.2 portable build successfully ran one private background NMF import and GLB export using the community beta plug-in on a scratch copy. The plug-in remains unlicensed and outside the repository; material/texture fidelity and wider compatibility are unproven. |
| Node.js | 24.19.0 | Available; meets the current REA Windows guide's Node 24.11+ prerequisite. |
| .NET SDK | 9.0.319 and 10.0.402 | Available for optional external C# experiment/AI clients. Not required for the C++ core. |
| Lua executable | Not found on PATH | A system Lua executable is not necessarily needed; Wicked Engine provides embedded Lua APIs. |
| REA CLI | `rea-agents` 6.7.0 invoked through `npx`; Node.js 24.19.0 | Portable invocation works. Ghidra provider and Java pass REA's doctor checks; the overall doctor also reports unrelated optional providers and integrations missing. No global REA setup or Codex/Claude registration was applied. |
| Ghidra and JDK | Ghidra 12.1.4 with Temurin JDK 21.0.12.1 unpacked under the private research tools directory | REA's Windows Ghidra P0 path is available for a bounded, read-only static question. These portable tools and any reports/databases stay outside this public repository. |
| Wicked Engine | Pinned as a Git submodule at `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` (the local checkout is shallow) | Upstream `Editor_Windows` and `Template_Windows` Release/x64 targets compile with `/p:PlatformToolset=v143`; the sample opens on DirectX 12 and shows its blank scene. The new OpenRepublic Release/x64 target links the upstream CMake target and reaches engine initialization. |
| Bundled Lua | Wicked Engine submodule includes Lua 5.4.8 and CMake target `LUA` | OpenRepublic's isolated rule loader links the bundled library. A system Lua executable is not required; the loader opens no standard libraries and exposes no Wicked Engine APIs. |
| WRSR installation | Steam app manifest and installed game directory were found locally | Read-only asset experiments remain a separate gate; do not put local paths or game files in this repository. |
| WRSR ModelViewer | Present locally and inspected read-only with REA/Ghidra; it was not launched | Static strings include `Load NMF`, `Import OBJ`, `Save NMF`, `Save As NMF`, and material/texture controls. This is evidence of intended features, not proof the tool successfully loads a vanilla model or exports it to OpenRepublic. |

## Build receipt

**Upstream revision:** `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`
**Configuration:** Release, x64; VS 2022 Community 17.14.37806.7; MSVC 14.44.35207; Windows SDK 10.0.26100.0 selected explicitly for the repeat build.
**Compile outcome:** `Editor_Windows` and `Template_Windows` compiled.

The upstream Visual Studio projects specify toolset v145. The installed v143 toolset succeeded with an MSBuild property override. The editor solution target and sample project were built separately; when invoking the sample `.vcxproj` directly, pass the upstream root as `SolutionDir` so its include/library paths resolve. Both commands below select Windows SDK 10.0.26100.0 explicitly.

From an x64 VS Developer Command Prompt, these commands reproduce the targets (replace `<repo>` with the checkout root):

```bat
msbuild "<repo>\third_party\WickedEngine\WickedEngine.sln" /t:Editor_Windows /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.26100.0 /m /nologo /v:minimal

msbuild "<repo>\third_party\WickedEngine\Samples\Template_Windows\Template_Windows.vcxproj" /p:SolutionDir="<repo>\third_party\WickedEngine\" /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.26100.0 /m /nologo /v:minimal
```

The compile emitted warnings from bundled `lodepng` (`C4334`, `C4267`) and `zstd` (`C4551`); no upstream source was changed and warnings were not suppressed. Generated executables/libraries are ignored inside the submodule and are not in the public commit.

## OpenRepublic app build receipt

**Configuration:** Visual Studio 17 2022 generator, x64, Release; CMake 4.4.3; VS 2022 Community 17.14.37806.7; MSVC v143 / 14.44.35207; Windows SDK 10.0.26100.0.
**Build outcome:** the OpenRepublic `WIN32` target compiled and linked against the pinned `WickedEngine` CMake target. The root build disables the upstream Editor executable, tests, ImGui examples, and template, while compiling the pinned `Editor/ModelImporter_GLTF.cpp` source into OpenRepublic for a bounded local GLB import path.

From a VS 2022 Developer shell, quote the SDK version argument when using PowerShell:

```powershell
cmake -S . -B build/openrepublic-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143 "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic OpenRepublicSimulationTests OpenRepublicLuaRulesTests --parallel
```

The runtime executable is `build/openrepublic-vs2022/src/app/Release/OpenRepublic.exe`. Run it with `build/openrepublic-vs2022/` as the working directory; Wicked Engine's `Content` directory is generated at that level. Edit `build/openrepublic-vs2022/data/rules/demo_rules.lua` and press **Reload Lua rules** to apply local project rules; a copy is also placed beside the executable.

## Independent simulation build and test receipt

The root project also configures without the Windows client: `OPENREPUBLIC_BUILD_CLIENT=OFF` builds only the headless simulation and its tests. On 21 October 2026, the Windows-client configuration built both simulation and Lua tests; CTest passed 2/2. A separate Visual Studio x64 headless build at `build/openrepublic-headless-vs2022` passed CTest 1/1. The previous `build/openrepublic-headless` cache used a different generator platform and was preserved. These checks cover only synthetic simulation/rules and save invariants; they do not compare WRSR behavior or runtime-check an asset scene.

```powershell
cmake -S . -B build/openrepublic-headless-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143 -DOPENREPUBLIC_BUILD_CLIENT=OFF -DOPENREPUBLIC_BUILD_TESTS=ON
cmake --build build/openrepublic-headless-vs2022 --config Release --target OpenRepublicSimulationTests
ctest --test-dir build/openrepublic-headless-vs2022 -C Release --output-on-failure
```

## Template runtime observation

On 21 October 2026, the built `Template_Windows.exe` opened as `WickedEngineApplicationTemplate`. Its overlay identified Wicked Engine 0.72.126 and DirectX 12 at 2787x1540 (134 dpi). The first launch reported 12 pending graphics-pipeline creations; a repeat launch after the explicit-SDK build reported 27. Both notices cleared within 2.5 seconds, after which the window remained open with a black, empty scene. A screenshot was captured inline in the task session and was not added as a repository asset.

This confirms that the sample process starts and reaches a steady blank-scene loop on this workstation. The displayed 240 FPS is an application overlay value, not a controlled performance benchmark. The blank image does not establish that an OpenRepublic scene, asset importer, or useful renderer path works.

## Earlier OpenRepublic app runtime observation (cube-only version)

On 21 October 2026, the OpenRepublic executable opened a window titled `OpenRepublic | Synthetic Scene Prototype`. After the window opened, the runtime log reached `[wi::initializer] Wicked Engine Initialized` (about 8.9 seconds after launch). A WM_CLOSE message was accepted and the process exited with code 0. A 13-second post-window wait was used before closing it. Source code constructs an OpenRepublic-owned synthetic plane, cube, and directional light.

This confirms build, engine initialization, window lifecycle, and visible rendering of the synthetic plane/cube in that earlier version only. The first capture attempt was occluded; a later window-specific capture displayed the OpenRepublic window directly and showed the scene. The screenshot was not added to the repository. The current app has separate build and runtime evidence below. The OpenRepublic graphics API, local WRSR asset import, and performance remain unverified. The build artifacts are local and ignored by Git.

## Current OpenRepublic app runtime smoke check

On 21 October 2026, the rebuilt Release/x64 `OpenRepublic.exe` opened a window and displayed the current synthetic farm/mill/warehouse scene with a road, truck, and readable status HUD. A contrast adjustment to the HUD is included in `src/app/OpenRepublicApplication.cpp`; the Lua control was fitted to the observed 904-pixel client width, and the HUD now shows rules on a separate line.

The visible interaction checks were bounded and reproducible in the app:

- Pause was clicked at tick 47; the tick remained 47 after 1.5 seconds.
- With simulation paused, “Step one tick” advanced the tick from 47 to 48.
- Edge 202 was closed and reopened; the button text, HUD status, and road color changed with it.
- Clicking the mill changed the window title to `Selected OR-1002` and highlighted the mill.
- Mouse-wheel input changed the camera framing.
- “Place storage” created a simulation-owned prototype storage block and incremented the HUD count.
- “Save demo” wrote an OpenRepublic text save under the ignored local build directory. At tick 77 with one storage, “Step one tick” advanced to 78 and “Place storage” added a second; “Load demo” restored tick 77, one storage, and the prior inventory/truck/road state.
- Startup loaded the bundled default rates. While paused at tick 21, a temporary edit to farm `5 every 3 ticks` and mill `2/tick` applied immediately. An out-of-range farm value `13` was rejected while active `5/3/2` rates stayed visible. While running, a valid edit to `4/1/2` applied at the next fixed tick boundary. The build-local rule files were restored to the source defaults afterward.

These checks validate only the current synthetic scene, OpenRepublic save, and three-field OpenRepublic rules. Lua uses a fresh embedded state with no standard libraries, text-only chunks, a 64 KiB source cap, an 8 MiB state-allocation cap, and an instruction-count limit; this narrow declarative interface is not a general third-party mod sandbox. Prototype placement has no cost, construction materials/stages, builder, or WRSR placement rule. Keyboard camera movement did not receive a controlled runtime check. The original WRSR installation was not read by this interaction sequence; a separate import probe did load a private GLB derived from one copied local NMF model, as recorded below. No WRSR terrain or save was loaded; graphics API, driver, and performance remain unmeasured. Screenshots were shown during the earlier UI session and were not committed.

## Local GLB and WISCENE import probes

On 21 October 2026, `tools/create_synthetic_glb.py` generated an OpenRepublic-authored 896-byte GLB under the ignored build scratch directory. The rebuilt Release/x64 app launched with `--glb-probe` and its runtime log reported **1 mesh, 1 material, 2 transforms**. A separate probe loaded a scratch copy of the prior private GLB export from one copied local NMF; its runtime log reported **3 meshes, 2 materials, 4 transforms**. No source NMF, GLB, material, or texture was added to the repository. These checks confirm parsing into Wicked scene components only; no screenshot, visual comparison, texture resolution, transform accuracy, or broad asset/variant coverage was measured.

The app also launched with `--scene-probe` pointed at the pinned Wicked Engine `cube.wiscene` sample; the runtime log reported **1 transform added**. This validates the `.wiscene` archive path. The GLB path compiles `third_party/WickedEngine/Editor/ModelImporter_GLTF.cpp` into OpenRepublic and is coupled to an Editor implementation detail. Treat it as an experimental bridge that needs an isolated OpenRepublic content interface before expanding. The NMF-to-GLB input was produced with a community add-on of unknown license/provenance, so that conversion step is not a shippable dependency or public workflow.

## Private NMF-to-GLB interchange probe

The community Blender 5.2 beta importer accepted one copied local NMF model in a clean background scene. Blender's GLB exporter completed and wrote a private scratch output. OpenRepublic subsequently loaded a scratch copy of that GLB into the pinned Wicked importer, adding 3 meshes, 2 materials, and 4 transforms according to the runtime log. This does **not** establish that all game materials/textures were resolved, that other NMF variants work, that direct NMF loading works, or that a public standalone asset workflow is permitted. The add-on has no known license/provenance and is not an OpenRepublic dependency. Input copy, `.debug.txt`, script, log, JSON report, and GLB remain under private research scratch storage.

## ModelViewer static inventory

On 21 October 2026, REA 6.7.0 with its Ghidra 12.1.4 provider completed a read-only inventory of the local ModelViewer executable. Ghidra reported 647 defined strings and 665 procedures in its analyzed inventory. The strings include the title `Soviet Model Viewer and Exporter`, `Load NMF`, `Import OBJ`, `Save NMF`, `Save As NMF`, material load/save controls, and texture warnings that refer to DDS and MTL files. The procedure/import inventory includes C3D mesh and material load/save names.

Static review of the `Load NMF` handler identifies a file-dialog path followed by `C3D_MESH::LoadFromFile`; the OBJ handler reaches an import helper and `C3D_MESH::LoadFromMemory`; save handlers reach `C3D_MESH::SaveToFile`. One handler has an error branch for a selected path outside `GAME_DIR/media_soviet`; static review has not established the scope of this check across all file operations. This narrows a later, local inspection experiment but does not establish a standalone export workflow or confirm a successful load. No ModelViewer window was launched and no game asset was opened, exported, or copied. Exact executable identity, addresses, decompiler output, and REA reports remain private and are not committed.

## Next toolchain actions

1. [x] Build the upstream editor/template with explicit MSVC v143 and SDK 10.0.26100.0; launch the template and record its DirectX 12 blank-scene startup.
2. [x] Create and build the first OpenRepublic-owned synthetic-scene app; confirm Wicked Engine initialization and clean window shutdown.
3. [x] Visually verify OpenRepublic synthetic scene pixels.
4. [x] Implement the synthetic scene, pause/step/road HUD, object picking, and wheel zoom; [x] runtime-check the current scene and these controls. [x] Add simulation-owned prototype storage placement and GUI save/load; [x] runtime-check a save/load restoration. [x] Add isolated Lua rule loading and tick-boundary reload; [x] verify running/paused reload and rejection preservation in the app. [ ] Verify keyboard movement and record the actual render API, graphics driver, GPU, and build configuration.
5. [~] Identify and validate local content paths without copying original assets to Git. The bounded R32F terrain decode/preview is complete for one copied sample; direct model/material loading and verified terrain/world coordinates remain open. Static ModelViewer control flow contains an out-of-directory error branch whose scope across file operations is unresolved.
6. [x] Prepare a portable REA/Ghidra/JDK path for the ModelViewer question. REA's current Windows Ghidra scope is experimental; keep analysis read-only and all tool outputs private.
7. [x] Configure and build the independent fixed-step C++ simulation in Windows-client and headless-only configurations. Current client CTest passed 3/3 (simulation, terrain reader, Lua rules); headless CTest passed 2/2 (simulation, terrain reader).
8. Keep Python/Node/C# experiments separate from the authoritative simulation until profiling justifies a production role.

## Launch observation

The project owner reports that `steam.exe -applaunch 784150` starts the game, while launching the game's launcher executable directly and using `rungameid` fail. This was not independently tested during the repository setup task. Do not use a game launch as a build check; document the exact Steam/game version if a future controlled in-game comparison needs it.

## Bounded R32F terrain reader and runtime preview

On 21 October 2026, the Windows Release/x64 build compiled `OpenRepublicContent` and the `--terrain-probe` app route. Client CTest passed 3/3 (simulation, terrain reader, Lua rules); the separate headless build passed 2/2 (simulation, terrain reader). Terrain-reader fixtures cover valid decoding plus malformed/truncated DDS headers and payloads, unsupported topology/format, non-finite samples, size bounds, transactional failure, and terrain metadata syntax/dimension checks.

The app was launched against one **copied local save** heightmap in the separate Rival Republics research tree, with its sibling `script.ini`. The log recorded a 2048x2048 R32F input and 257x257 decimated mesh; the app screenshot showed a visible terrain surface after matching Wicked Engine's built-in plane winding. A first screenshot exposed a backface-culling error; the winding was corrected and the new screenshot showed the front-facing surface. The input remained read-only and no proprietary asset entered Git. The preview uses a centered display frame, preserves source row order, and applies a candidate height mapping with explicit 5x vertical display exaggeration. It does not establish world origin, row orientation, map alignment, material masks, water/buildability, or accurate height semantics. Adapter memory/performance measurements and broader format coverage are still open.

## Sources

- [Wicked Engine Windows build instructions](https://github.com/turanszkij/WickedEngine)
- [REA installation](https://github.com/morluto/rea/blob/main/docs/installation.md)
- [REA Windows Ghidra P0](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
