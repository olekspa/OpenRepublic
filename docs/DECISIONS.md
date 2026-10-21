# Project decisions

Record the reason and consequences when a project-wide choice changes. These decisions apply to OpenRepublic only; they do not change the existing WRSR modding projects.

## D-001: Standalone client, not an injected replacement renderer

**Date:** 21 October 2026
**Status:** Accepted

OpenRepublic targets an independent client and simulation. Original-game plugins and loaders remain research references and possible controlled-observation tools, not required runtime components. This aligns with the requested modern architecture and avoids making the new engine inherit the original process lifecycle as its main abstraction.

**Consequence:** world rendering, commands, save/load, and simulation must be implemented in OpenRepublic. Compatibility tasks are larger, so scope remains one bounded world and one vertical slice at a time.

## D-002: Wicked Engine is the first renderer candidate

**Date:** 21 October 2026
**Status:** Accepted for feasibility evaluation; no dependency revision pinned

Use Wicked Engine's upstream Windows build and blank scene as the first renderer/tooling gate. Revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` is pinned as a shallow Git submodule. The upstream editor and template sample compile in Release/x64 using the installed VS 2022/MSVC v143 override; the source projects request v145 by default.

**Consequence:** the compile does not prove runtime rendering or OpenRepublic app integration. No performance or usability claim is made until a local build and benchmark exist. If it creates a measured blocker, evaluate an alternative renderer against the same headless simulation contract.

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
