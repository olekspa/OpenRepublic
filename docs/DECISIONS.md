# Project decisions

Record the reason and consequences when a project-wide choice changes. These decisions apply to OpenRepublic only; they do not change the existing WRSR modding projects.

## D-001: Standalone client, not an injected replacement renderer

**Date:** 21 October 2026
**Status:** Accepted

OpenRepublic targets an independent client and simulation. Original-game plugins and loaders remain research references and possible controlled-observation tools, not required runtime components. This aligns with the requested modern architecture and avoids making the new engine inherit the original process lifecycle as its main abstraction.

**Consequence:** world rendering, commands, save/load, and simulation must be implemented in OpenRepublic. Compatibility tasks are larger, so scope remains one bounded world and one vertical slice at a time.

## D-002: Wicked Engine is the first renderer candidate

**Date:** 21 October 2026
**Status:** Exact revision pinned for feasibility; no long-term production pin approved

Use Wicked Engine's upstream Windows build and blank scene as the first renderer/tooling gate. Revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` is pinned as a Git submodule; the local checkout was fetched shallowly. The upstream editor and template sample compile in Release/x64 with VS 2022/MSVC v143 and Windows SDK 10.0.26100.0; the source projects request v145 by default. The template opens on DirectX 12 and reaches its blank scene.

**Consequence:** a blank sample scene confirms startup and basic runtime stability only; it does not establish useful rendering, OpenRepublic app integration, or a performance result. If the engine creates a measured blocker, evaluate an alternative renderer against the same headless simulation contract.

## D-003: Keep research/artifact intake separate from the public repository

**Date:** 21 October 2026
**Status:** Accepted

Private game paths, file hashes, saves, captures, Ghidra databases, REA outputs, and detailed local reverse-engineering notes stay outside the public checkout. Public ledgers contain non-proprietary summaries and source links only.

**Consequence:** a local research report may contain information that is intentionally absent from public project docs; any future transfer requires a conscious provenance and license review.

## D-004: MIT license for OpenRepublic-authored files

**Date:** 21 October 2026
**Status:** Accepted as the initial project license

The repository's original source and documentation use MIT. The license does not cover WRSR or third-party materials; dependencies retain their own licenses. Do not copy GPL-licensed loader/plugin code into the repository under the assumption that MIT remains valid.

**Consequence:** review all future dependencies and contribution terms; consider any change to a copyleft license before accepting contributions that make relicensing impractical.

## D-005: Local assets are an experiment, not a release permission

**Date:** 21 October 2026
**Status:** Accepted

A legitimate installation and read-only loading are project assumptions and engineering controls. Neither proves that use in an independent client is authorized. The official in-game Workshop modding guidance is narrower than standalone distribution.

**Consequence:** keep early content experiments local and make rights-holder clarification/legal review a gate before publishing an asset-reading runtime or content.

## D-006: Prove the OpenRepublic viewer with synthetic content first

**Date:** 21 October 2026
**Status:** Accepted for the first viewer milestone

The first OpenRepublic-owned application scene uses only OpenRepublic-created geometry and materials. Current community and official modding documents describe game-side mod and ModelViewer workflows, but do not establish a standalone vanilla-asset export/import contract. A synthetic scene lets us validate the app and renderer integration before a content-format experiment.

**Consequence:** this is a sequencing choice, not a replacement for the requested local WRSR-asset investigation. Keep the first local asset/material and terrain/placement gates explicit; do not call the synthetic viewer WRSR-compatible.

## D-007: Use one typed Agent API over the authoritative simulation

**Date:** 21 October 2026
**Status:** Accepted

Human UI, tests, Lua-facing tools, and future LLM or external controllers must reach gameplay through the same validated command and query paths over the authoritative C++ simulation. The first implementation is an in-process typed C++ interface with capability discovery, revision-checked entity queries and commands, bounded paging and event reads, permission checks, and distinct queued/applied/rejected results. It does not own a second simulation and does not expose renderer entities as world identity.

**Consequence:** keep command/query types and state revisions explicit, and apply mutations at simulation tick boundaries. The internal API is not a WRSR protocol or compatibility claim. The first local adapter is recorded separately in D-008.

## D-008: Start external-agent access with a bounded local JSONL host

**Date:** 21 October 2026
**Status:** Accepted for the synthetic Agent API foundation

Expose the typed Agent API through a separate process that reads one versioned JSON request per stdin line and writes one JSON response per stdout line. The host owns a synthetic OpenRepublic simulation and reuses the existing API; it does not attach to WRSR or to the Wicked Engine process. The first controller is a Python stdlib example. The host uses the MIT-licensed nlohmann/json 3.12.0 header already present in the pinned Wicked Engine submodule and remains optional in configurations that do not have that header.

**Consequence:** keep the host local and off the network. Its observer/controller switch selects permissions but does not authenticate callers. C# IPC, MCP, multi-client service, and WRSR control remain separate future work and need their own versioned contracts and security review.
