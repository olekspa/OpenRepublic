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
| Blender | 4.3 launcher appears in the installed-app inventory; `blender` is not on the normal PATH | Available for open/synthetic content authoring; no WRSR-specific import/export plugin was validated in this pass. |
| Node.js | 24.19.0 | Available; meets the current REA Windows guide's Node 24.11+ prerequisite. |
| .NET SDK | 9.0.319 and 10.0.402 | Available for optional external C# experiment/AI clients. Not required for the C++ core. |
| Lua executable | Not found on PATH | A system Lua executable is not necessarily needed; Wicked Engine provides embedded Lua APIs. |
| REA CLI | `rea-agents` 6.7.0 invoked through `npx`; Node.js 24.19.0 | Portable invocation works. Ghidra provider and Java pass REA's doctor checks; the overall doctor also reports unrelated optional providers and integrations missing. No global REA setup or Codex/Claude registration was applied. |
| Ghidra and JDK | Ghidra 12.1.4 with Temurin JDK 21.0.12.1 unpacked under the private research tools directory | REA's Windows Ghidra P0 path is available for a bounded, read-only static question. These portable tools and any reports/databases stay outside this public repository. |
| Wicked Engine | Pinned as a Git submodule at `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` (the local checkout is shallow) | Upstream `Editor_Windows` and `Template_Windows` Release/x64 targets compile with `/p:PlatformToolset=v143`; the sample opens on DirectX 12 and shows its blank scene. The new OpenRepublic Release/x64 target links the upstream CMake target and reaches engine initialization. |
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
**Build outcome:** the OpenRepublic `WIN32` target compiled and linked against the pinned `WickedEngine` CMake target. The root build disables the upstream editor, tests, ImGui examples, and template to build only the library and required content for this app.

From a VS 2022 Developer shell, quote the SDK version argument when using PowerShell:

```powershell
cmake -S . -B build/openrepublic-vs2022 -G "Visual Studio 17 2022" -A x64 -T v143 "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"
cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic --parallel
```

The runtime executable is `build/openrepublic-vs2022/src/app/Release/OpenRepublic.exe`. Run it with `build/openrepublic-vs2022/` as the working directory; Wicked Engine's `Content` directory is generated at that level.

## Template runtime observation

On 21 October 2026, the built `Template_Windows.exe` opened as `WickedEngineApplicationTemplate`. Its overlay identified Wicked Engine 0.72.126 and DirectX 12 at 2787x1540 (134 dpi). The first launch reported 12 pending graphics-pipeline creations; a repeat launch after the explicit-SDK build reported 27. Both notices cleared within 2.5 seconds, after which the window remained open with a black, empty scene. A screenshot was captured inline in the task session and was not added as a repository asset.

This confirms that the sample process starts and reaches a steady blank-scene loop on this workstation. The displayed 240 FPS is an application overlay value, not a controlled performance benchmark. The blank image does not establish that an OpenRepublic scene, asset importer, or useful renderer path works.

## OpenRepublic app runtime observation

On 21 October 2026, the OpenRepublic executable opened a window titled `OpenRepublic | Synthetic Scene Prototype`. After the window opened, the runtime log reached `[wi::initializer] Wicked Engine Initialized` (about 8.9 seconds after launch). A WM_CLOSE message was accepted and the process exited with code 0. A 13-second post-window wait was used before closing it. Source code constructs an OpenRepublic-owned synthetic plane, cube, and directional light.

This confirms build, engine initialization, window lifecycle, and visible rendering of the synthetic plane/cube only. The first capture attempt was occluded; a later window-specific capture displayed the OpenRepublic window directly and showed the scene. The screenshot was not added to the repository. A later source change added keyboard movement, wheel zoom, and ray picking for `OR-0001`; the Release/x64 build passed, but the follow-up UI interaction check was interrupted before those controls could be runtime-verified. The OpenRepublic graphics API, local WRSR asset import, and performance remain unverified. The build artifacts are local and ignored by Git.

## ModelViewer static inventory

On 21 October 2026, REA 6.7.0 with its Ghidra 12.1.4 provider completed a read-only inventory of the local ModelViewer executable. Ghidra reported 647 defined strings and 665 procedures in its analyzed inventory. The strings include the title `Soviet Model Viewer and Exporter`, `Load NMF`, `Import OBJ`, `Save NMF`, `Save As NMF`, material load/save controls, and texture warnings that refer to DDS and MTL files. The procedure/import inventory includes C3D mesh and material load/save names.

Static review of the `Load NMF` handler identifies a file-dialog path followed by `C3D_MESH::LoadFromFile`; the OBJ handler reaches an import helper and `C3D_MESH::LoadFromMemory`; save handlers reach `C3D_MESH::SaveToFile`. One handler has an error branch for a selected path outside `GAME_DIR/media_soviet`; static review has not established the scope of this check across all file operations. This narrows a later, local inspection experiment but does not establish a standalone export workflow or confirm a successful load. No ModelViewer window was launched and no game asset was opened, exported, or copied. Exact executable identity, addresses, decompiler output, and REA reports remain private and are not committed.

## Next toolchain actions

1. [x] Build the upstream editor/template with explicit MSVC v143 and SDK 10.0.26100.0; launch the template and record its DirectX 12 blank-scene startup.
2. [x] Create and build the first OpenRepublic-owned synthetic-scene app; confirm Wicked Engine initialization and clean window shutdown.
3. [x] Visually verify OpenRepublic synthetic scene pixels.
4. [x] Implement camera controls and object picking; [ ] runtime-verify them and record the actual render API, graphics driver, GPU, and build configuration.
5. [ ] Identify and validate one supported local model/material import path and one terrain/world-coordinate path without copying original assets to Git. Static ModelViewer control flow contains an out-of-directory error branch, but its scope across file operations is unresolved; a controlled UI probe and a separate interchange path are still needed.
6. [x] Prepare a portable REA/Ghidra/JDK path for the ModelViewer question. REA's current Windows Ghidra scope is experimental; keep analysis read-only and all tool outputs private.
7. Keep Python/Node/C# experiments separate from the authoritative simulation until profiling justifies a production role.

## Launch observation

The project owner reports that `steam.exe -applaunch 784150` starts the game, while launching the game's launcher executable directly and using `rungameid` fail. This was not independently tested during the repository setup task. Do not use a game launch as a build check; document the exact Steam/game version if a future controlled in-game comparison needs it.

## Sources

- [Wicked Engine Windows build instructions](https://github.com/turanszkij/WickedEngine)
- [REA installation](https://github.com/morluto/rea/blob/main/docs/installation.md)
- [REA Windows Ghidra P0](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
