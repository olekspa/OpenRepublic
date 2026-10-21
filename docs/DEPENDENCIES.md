# Dependency manifest

| Dependency | Pinned source | License | Current use/status | Notes |
|---|---|---|---|---|
| Wicked Engine | [upstream](https://github.com/turanszkij/WickedEngine), commit `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`, Git submodule `third_party/WickedEngine` | Upstream repository: MIT. Its `third_party_software.txt` and bundled component notices remain applicable. | First renderer candidate. Upstream editor/template and the OpenRepublic Windows target compiled locally. The app creates a synthetic plane/cube/light scene and reaches engine initialization; a window-specific capture confirmed the scene renders. Keyboard movement, wheel zoom, and ray picking are implemented in source and build, but runtime interaction remains unverified. WRSR asset import and performance remain open. | Git submodule preserves source provenance and avoids copying upstream source into the OpenRepublic-authored MIT tree. Review all bundled notices for any redistributed product. |

No other third-party source, SDK, binary, or game asset is currently included. Build outputs live in the submodule's ignored local build directory and are not committed.
