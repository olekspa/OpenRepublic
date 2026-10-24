# OpenRepublic: scope, goal and target

OpenRepublic is an independent, open-source (MIT), next-generation reimplementation of *Workers & Resources: Soviet Republic* (WRSR) built on Wicked Engine, C++ and Lua, with an LLM-native architecture. It is not a mod, not a loader and not a repackaging of the original game.

`AGENTS.md` holds the non-negotiable contributor rules (it applies to every coding agent). This file adds the scope, goal, target, architecture and working method. Read both before changing anything.

## Goal

Build the game. The target is **authentic WRSR gameplay parity** on a modern engine, with:

1. **Gameplay parity:** the economy, logistics, construction, population, utilities, trade and campaign systems of WRSR, reproduced from evidence, not from guesses.
2. **Next-generation graphics:** Wicked Engine rendering (ray-traced reflections/AO/GI, volumetric sky and clouds, DLSS/frame generation, a modern vehicle, vegetation, water and road presentation).
3. **Performance and scale:** large maps and economies with deterministic, multi-threaded simulation.
4. **LLM-native play:** humans and AI controllers (scripted, rule-based, or LLM-driven) operate the same authoritative C++ simulation through the same versioned interfaces; autonomous AI republics are a first-class feature.
5. **Modding:** Lua rules and scene scripts in a sandbox, data-driven catalogs and scenarios.

Parity is a long-running target: it is never marked complete by a single milestone. Every claim of parity must be backed by evidence labelled as below.

## Scope

In scope: the standalone client and simulation; readers for the user's own legitimately obtained WRSR install (read-only, local) to import content for compatibility experiments; synthetic/open content so the engine runs without any game files; AI controller tooling, scenarios and benchmarks; documentation of verified findings.

Out of scope: redistributing any WRSR binary, asset, save, extracted cache or decompiled source (in Git or in releases); running inside the original process; reading or writing the original save format as a goal (OpenRepublic has its own versioned save format); copying GPL mod-loader code. Distribution rights for builds that use original content or NVIDIA runtimes are an open decision (see `docs/DECISIONS.md`, D-005).

## Rules that must never be skipped

- The C++ simulation is the single authority. Rendering, Lua rules and external controllers talk to it only through explicit, versioned interfaces (typed `EconomyApi`, JSONL host protocol v0, scene control protocol).
- Evidence labels: **VERIFIED** (writer/reader/consumer or source proof), **OBSERVED** (seen in data or a run, build context named), **INFERRED**, **UNKNOWN**. Never upgrade a label without the evidence. Do not invent WRSR formats, formulas or compatibility claims. Synthetic mechanics are labelled "OpenRepublic synthetic rules; WRSR parity UNKNOWN".
- Game-specific evidence names the game build; do not reuse offsets between builds without revalidation. Exact install paths, hashes, decompiler output and private analysis stay outside the repository (private research lives on the local machine, not in Git).
- Do not launch the original game, modify saves or write to the Steam installation unless the task explicitly requires it.
- Features are opt-in: a catalog or option that does not declare a new feature must behave byte-identically and keep the same state hashes.
- Keep source validation, build results, original-game comparison and graphics/hardware acceptance separate in reports. A worker or tool report that says "complete" is a claim to verify, not a result.

## Architecture (where things live)

| Path | Role |
|---|---|
| `src/economy`, `src/economy_api`, `src/economy_host` | Authoritative economy (C++), typed API, JSONL host (controller/observer roles), deterministic hashes, persistence |
| `src/compat`, `src/content` | Importers and bounded readers for the local WRSR install (building definitions, maps, roads and networks, trees, header, workers, terrain masks) |
| `src/runtime` | Headless-testable glue: scene plans, clocks, overview, checkpoint files |
| `src/app` | Wicked Engine desktop client (native economy mode, map region views, showcase look, toolbar and inspector) |
| `src/procgeo` | Procedural geometry (vehicles, props, wires, rails) |
| `src/scenectl`, `src/scripting` | Scene control protocol and sandboxed Lua scene scripts and economy rules |
| `src/agent_api`, `src/agent_host` | Agent-facing API and host |
| `src/sim` | Older demo simulation (not the place for new gameplay; new gameplay belongs under `src/economy*`, see D-008) |
| `tools/` | Python tooling: rule-based autopilot, LLM republic harness and tournament, scenario scorer, scene control, patches for the pinned Wicked Engine |
| `data/` | Scenarios, rules and scripts shipped with the project |
| `docs/` | Plan, decisions, the evidence ledger, per-feature guides, handoffs and the chronological `PROGRESS_LOG.md` |
| `third_party/WickedEngine` | Pinned engine submodule (private DLSS/Streamline builds apply patches from `tools/patches`; SDK and runtime files never enter Git) |

## Current state (pointer, keep this section short)

Read the newest `docs/HANDOFF_*.md`, `docs/PARITY_ROADMAP.md` and the tail of `docs/PROGRESS_LOG.md`. In one paragraph: a playable synthetic economy with AI controllers, scenarios and scoring; opt-in synthetic population, trade, construction sites and heating/water; decoded map, road/network, header, building and worker record layouts with many fields still UNKNOWN; a Wicked Engine showcase and native desktop mode; DLSS super resolution works in private builds. The main open parity gaps are road width/lanes/speed constants, building-state and header-option meanings, the original-game screenshot comparison, and most economy formulas.

## Build, test, run

- Client (Windows, Visual Studio 2022): `cmake --build build/openrepublic-vs2022 --config Release --target OpenRepublic`; full build: same without `--target`. Use PowerShell for `/m`-style flags.
- Tests: `ctest --test-dir build/openrepublic-vs2022 -C Release` (expect all tests to pass; run the full build first so test executables are current).
- Economy host and scenarios: `python tools/scenario_score.py --host <OpenRepublicEconomyHost.exe> --scenario data/scenarios/<id>.json --ticks N`; controllers: `tools/republic_autopilot.py`, `tools/llm_republic.py`, `tools/republic_tournament.py` (scripted/rule-based controllers need no network).
- Desktop sandbox with construction sites: `OpenRepublic.exe --native-economy --native-construction-sites`. Showcase and overlay switches are documented in `docs/GRAPHICS.md`, `docs/NETWORK_KINDS.md` and the latest handoff.
- **Read the build output before committing app changes** (a compile error was pushed once because only the test summary was checked).

## Working method

- Read `docs/PROJECT_PLAN.md`, `docs/DECISIONS.md` and the relevant ledger entry first. Make small, scoped changes; add tests with each behaviour change.
- Record every step chronologically in `docs/PROGRESS_LOG.md` with the real clock time (see its erratum about estimated times). Keep documentation current: new feature guide, ledger entry, handoff pointer.
- Commit with explicit file paths, never `git add -A`: another Codex stream may be committing to the same checkout. Check `git status` first; do not stash or revert files you did not change.
- Parallel work uses git worktrees on `luna/*` branches, one worker per independent area; merge one branch at a time into `main` with `--no-ff`, run the full build and ctest after each merge, resolve doc conflicts by keeping both sides, then push.
- Codex agents are orchestrated through the local bridge (`CodexBridge-v5-OpenRepublic`, port 8890; restart with its `start_dev.ps1`); they run on the account's Codex quota, which can run out. Worker builds go on `C:` (junctioned build directories); keep large private data on `C:` because `D:` has overfilled repeatedly.
- Do not run GUI automation against the desktop app while the owner may be using the machine: a second OpenRepublic window may be theirs.

## Definition of done for a parity feature

Source tests pass; the full build is clean; behaviour is exercised through the public interface (API/host/scenario); the evidence labels and sources are recorded in the ledger/docs; defaults and hashes of existing content are unchanged; and a report states separately what was verified by source, by headless run, by GUI, and by comparison with the original game (usually: not yet).
