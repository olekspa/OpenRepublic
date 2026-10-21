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
| Wicked Engine revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` is pinned as a submodule. | VERIFIED | Upstream Git commit is recorded in the repository gitlink; upstream reports MIT licensing. | No OpenRepublic code depends on it yet. | Keep the revision fixed; update only through a reviewed dependency change. |
| The upstream Wicked Engine editor and template sample compile on the local VS 2022/MSVC 14.44 toolchain. | VERIFIED (compile only) | Release/x64; local command-line property override `PlatformToolset=v143`; build output in private ignored artifacts. | The project's v145 default is unavailable here; runtime/graphics output and an OpenRepublic-owned app are unverified. Third-party source emitted warnings. | Run the template sample and record the graphics API before implementing the first OpenRepublic window. |
| Current WRSR modding reports can inform selected behavior and file research. | OBSERVED (research intake) | Existing local project documents and public community reports were surveyed. | Those reports have different game builds and may contain inferred fields; they do not define an OpenRepublic API or full WRSR schema. | Promote each candidate only through a separate evidence entry with exact source, build/hash, experiment, and counterexamples in the private ledger. |
| `steam.exe -applaunch 784150` launches this installation. | OBSERVED (owner report only) | Owner statement in the project request. | Not independently launched or verified in this task; other launch routes reported failing. | If needed, test only a user-owned, controlled launch and record game/build; this is not required for the OpenRepublic build. |
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
