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

Wicked Engine's upstream repository is MIT-licensed and documents a C++ framework, rendering/editor tooling, Lua scripting, and DirectX 12 on Windows with Vulkan available. It is a reasonable first rendering choice, but its cost, data model, import pipeline, and performance for this project still need a local build and benchmark.

REA can present bounded static analysis and Ghidra operations to agents. The current project docs describe Windows Ghidra support as experimental and read-only P0; its documented prerequisites include Node 22.19+ or 24.11+, operator-installed Ghidra 12.1.x, and a full JDK 21 or newer. REA does not remove the need to install/manage Ghidra and Java. Use it only for a focused unanswered question and recheck package release scope before setup.

### What the official modding page does and does not say

The official WRSR wiki explains in-game asset/mod workflows. It says WRSR models may be modified and uploaded to the game's Workshop. This is not a grant to distribute those models in a standalone client or through GitHub. Local-only loading and no asset redistribution are necessary project controls, but rights and applicable terms still need review before shipping an asset-reading runtime or a playable release.

## Initial approach decision

Proceed with an independent standalone client and a headless C++ simulation as the target. Use Wicked Engine for the first rendering experiment. Treat a local content viewer as a feasibility probe, not as the final compatibility layer. Preserve a synthetic/open content path if content rights or proprietary format support blocks the WRSR viewer.

Do not begin with multiplayer, full save conversion, DLSS/RTX features, one-million-citizen benchmarks, or a rewrite of all WRSR formulas. The first vertical slice is one bounded world plus one end-to-end logistics and production loop.

## Sources

- [REA](https://github.com/morluto/rea) and its [installation guide](https://github.com/morluto/rea/blob/main/docs/installation.md) / [Windows Ghidra guide](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
- [Wicked Engine](https://github.com/turanszkij/WickedEngine)
- [TesmioLoader](https://github.com/MaxLegend/TesmioLoader) and its [architecture](https://github.com/MaxLegend/TesmioLoader/blob/master/docs/01-architecture.md) / [RE notes](https://github.com/MaxLegend/TesmioLoader/blob/master/docs/03-reverse-engineering.md)
- [RepublicMP](https://github.com/E3418/RepublicMP)
- [TesmioLoader community plugins](https://github.com/Kespri/WRSR-MODS)
- [Official WRSR modding documentation](https://wiki.hoodedhorse.com/Workers_Resources_Soviet_Republic/General_modding)
- [MeitouClient](https://github.com/VlcekM/MeitouClient)
