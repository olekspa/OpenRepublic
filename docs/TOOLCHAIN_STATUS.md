# Toolchain readiness

**Checked:** 21 October 2026
**Scope:** Windows development workstation and the new OpenRepublic workspace. Versions below are an initial inventory, not a successful OpenRepublic build.

| Component | Observed state | OpenRepublic impact |
|---|---|---|
| Git | Available | Ready for source control. |
| GitHub CLI | Available and authenticated as the project owner | Ready for repository publication. Credentials are not recorded here. |
| Visual Studio | Visual Studio 2022 Community 17.14.37806.7 | Installed; use a VS Developer PowerShell/command prompt for MSVC environment. |
| MSVC | 14.44.35207, x64 tools | Installed. `cl` is not on a normal shell PATH, which is expected outside a developer environment. |
| Windows SDK | 10.0.19041.0, 10.0.22621.0, 10.0.26100.0 | Explicitly selected 10.0.26100.0 for the repeat upstream build below. |
| CMake | 4.4.3 available through an existing local toolchain | Available; not yet used to configure OpenRepublic or Wicked Engine. |
| Ninja | Not found on PATH | Not needed if the first build uses the upstream Visual Studio solution/generator. Revisit only if the selected workflow requires it. |
| Python | 3.12.10 | Available for bounded local format/benchmark helpers. |
| Node.js | 24.19.0 | Available; meets the current REA Windows guide's Node 24.11+ prerequisite. |
| .NET SDK | 9.0.319 and 10.0.402 | Available for optional external C# experiment/AI clients. Not required for the C++ core. |
| Lua executable | Not found on PATH | A system Lua executable is not necessarily needed; Wicked Engine provides embedded Lua APIs. |
| REA CLI | Not found in targeted PATH/locations | Not yet configured for OpenRepublic. Recheck current REA release documentation before installation/setup. |
| Ghidra launcher | Not found in targeted standard locations | Existing Ghidra analysis project artifacts are present from earlier work, but the Ghidra installation/runtime was not confirmed. Java is not on PATH. |
| Wicked Engine | Pinned as a Git submodule at `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` (the local checkout is shallow) | Upstream `Editor_Windows` and `Template_Windows` Release/x64 targets compile with `/p:PlatformToolset=v143`; the sample opens on DirectX 12 and shows its blank scene. No OpenRepublic app exists. |
| WRSR installation | Steam app manifest and installed game directory were found locally | Read-only asset experiments remain a separate gate; do not put local paths or game files in this repository. |

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

## Template runtime observation

On 21 October 2026, the built `Template_Windows.exe` opened as `WickedEngineApplicationTemplate`. Its overlay identified Wicked Engine 0.72.126 and DirectX 12 at 2787x1540 (134 dpi). The first launch reported 12 pending graphics-pipeline creations; a repeat launch after the explicit-SDK build reported 27. Both notices cleared within 2.5 seconds, after which the window remained open with a black, empty scene. A screenshot was captured inline in the task session and was not added as a repository asset.

This confirms that the sample process starts and reaches a steady blank-scene loop on this workstation. The displayed 240 FPS is an application overlay value, not a controlled performance benchmark. The blank image does not establish that an OpenRepublic scene, asset importer, or useful renderer path works.

## Next toolchain actions

1. [x] Build the upstream editor and template with explicit MSVC v143 and Windows SDK 10.0.26100.0; launch the template and record its DirectX 12 blank-scene startup.
2. Create the smallest OpenRepublic-owned window/scene application against the pinned library, then record its build and runtime separately from the upstream editor/sample build.
3. Record the render API, graphics driver, GPU, and build configuration used for the first OpenRepublic blank-scene run.
4. Only install/manage Ghidra, a compatible JDK, and REA when a named static-analysis question remains after public text files, community reports, and controlled observations are checked. REA's current Windows Ghidra scope is experimental; use its read-only boundary and verify the exact package release.
5. Keep Python/Node/C# experiments separate from the authoritative simulation until profiling justifies a production role.

## Launch observation

The project owner reports that `steam.exe -applaunch 784150` starts the game, while launching the game's launcher executable directly and using `rungameid` fail. This was not independently tested during the repository setup task. Do not use a game launch as a build check; document the exact Steam/game version if a future controlled in-game comparison needs it.

## Sources

- [Wicked Engine Windows build instructions](https://github.com/turanszkij/WickedEngine)
- [REA installation](https://github.com/morluto/rea/blob/main/docs/installation.md)
- [REA Windows Ghidra P0](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
