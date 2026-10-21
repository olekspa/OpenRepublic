# Toolchain readiness

**Checked:** 21 October 2026
**Scope:** Windows development workstation and the new OpenRepublic workspace. Versions below are an initial inventory, not a successful OpenRepublic build.

| Component | Observed state | OpenRepublic impact |
|---|---|---|
| Git | Available | Ready for source control. |
| GitHub CLI | Available and authenticated as the project owner | Ready for repository publication. Credentials are not recorded here. |
| Visual Studio | Visual Studio 2022 Community 17.14.37806.7 | Installed; use a VS Developer PowerShell/command prompt for MSVC environment. |
| MSVC | 14.44.35207, x64 tools | Installed. `cl` is not on a normal shell PATH, which is expected outside a developer environment. |
| Windows SDK | 10.0.19041.0, 10.0.22621.0, 10.0.26100.0 | Several SDK versions are available. Pin the one used by the first reproducible build. |
| CMake | 4.4.3 available through an existing local toolchain | Available; not yet used to configure OpenRepublic or Wicked Engine. |
| Ninja | Not found on PATH | Not needed if the first build uses the upstream Visual Studio solution/generator. Revisit only if the selected workflow requires it. |
| Python | 3.12.10 | Available for bounded local format/benchmark helpers. |
| Node.js | 24.19.0 | Available; meets the current REA Windows guide's Node 24.11+ prerequisite. |
| .NET SDK | 9.0.319 and 10.0.402 | Available for optional external C# experiment/AI clients. Not required for the C++ core. |
| Lua executable | Not found on PATH | A system Lua executable is not necessarily needed; Wicked Engine provides embedded Lua APIs. |
| REA CLI | Not found in targeted PATH/locations | Not yet configured for OpenRepublic. Recheck current REA release documentation before installation/setup. |
| Ghidra launcher | Not found in targeted standard locations | Existing Ghidra analysis project artifacts are present from earlier work, but the Ghidra installation/runtime was not confirmed. Java is not on PATH. |
| Wicked Engine | No dependency pinned in the new workspace | Not built or tested yet. Pin an upstream revision before implementation. |
| WRSR installation | Steam app manifest and installed game directory were found locally | Read-only asset experiments remain a separate gate; do not put local paths or game files in this repository. |

## Next toolchain actions

1. Open a VS Developer PowerShell and record `cl`, Windows SDK, and CMake versions for a clean baseline.
2. Select and pin a Wicked Engine commit; build its documented Windows solution/sample before making OpenRepublic-specific code.
3. Record the actual render API, graphics driver, GPU, and build configuration used for the first blank-scene run.
4. Only install/manage Ghidra, a compatible JDK, and REA when a named static-analysis question remains after public text files, community reports, and controlled observations are checked. REA's current Windows Ghidra scope is experimental; use its read-only boundary and verify the exact package release.
5. Keep Python/Node/C# experiments separate from the authoritative simulation until profiling justifies a production role.

## Launch observation

The project owner reports that `steam.exe -applaunch 784150` starts the game, while launching the game's launcher executable directly and using `rungameid` fail. This was not independently tested during the repository setup task. Do not use a game launch as a build check; document the exact Steam/game version if a future controlled in-game comparison needs it.

## Sources

- [Wicked Engine Windows build instructions](https://github.com/turanszkij/WickedEngine)
- [REA installation](https://github.com/morluto/rea/blob/main/docs/installation.md)
- [REA Windows Ghidra P0](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
