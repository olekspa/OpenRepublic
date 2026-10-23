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

**Consequence:** keep original game files, saves, captures, and extracted payloads local and read-only. Rights-holder clarification/legal review remains a gate before distributing original content or a binary release that uses it.

**Update 2026-10-22:** the project owner explicitly authorized publication of OpenRepublic-authored source. That source was initially published on `origin/main` through `da8f460`; the latest public source snapshot at the 03:10 EDT pause checkpoint is `101c8a8`; the repository contains no WRSR binaries, assets, saves, captures, or extracted data. This source-publication decision does not grant rights to distribute original content, establish standalone-use rights, or clear a binary/content release. No separate release build has been published.

## D-006: Prove the OpenRepublic viewer with synthetic content first

**Date:** 21 October 2026
**Status:** Accepted for the first viewer milestone

The first OpenRepublic-owned application scene uses only OpenRepublic-created geometry and materials. Current community and official modding documents describe game-side mod and ModelViewer workflows, but do not establish a standalone vanilla-asset export/import contract. A synthetic scene lets us validate the app and renderer integration before a content-format experiment.

**Consequence:** this is a sequencing choice, not a replacement for the requested local WRSR-asset investigation. Keep the first local asset/material and terrain/placement gates explicit; do not call the synthetic viewer WRSR-compatible.

## D-007: Use one typed Agent API over the authoritative simulation

**Date:** 21 October 2026
**Status:** Accepted

Human UI, tests, Lua-facing tools, and future LLM or external controllers must reach gameplay through the same validated command and query paths over the authoritative C++ simulation. The first implementation is an in-process typed C++ interface with capability discovery, revision-checked entity queries and commands, bounded paging and event reads, permission checks, and distinct queued/applied/rejected results. It does not own a second simulation and does not expose renderer entities as world identity.

**Consequence:** keep command/query types and state revisions explicit, and apply mutations at simulation tick boundaries. World queries may expose typed, read-only simulation rules and cumulative outcome counters so controllers can verify synthetic behavior; these values remain OpenRepublic state and do not imply WRSR formula parity. The internal API is not a WRSR protocol or compatibility claim. The first local adapter is recorded separately in D-008.

## D-008: Start external-agent access with a bounded local JSONL host

**Date:** 21 October 2026
**Status:** Accepted for the synthetic Agent API foundation

Expose the typed Agent API through a separate process that reads one versioned JSON request per stdin line and writes one JSON response per stdout line. The host owns a synthetic OpenRepublic simulation and reuses the existing API; it does not attach to WRSR or to the Wicked Engine process. The first controller is a Python stdlib example. The host uses the MIT-licensed nlohmann/json 3.12.0 header already present in the pinned Wicked Engine submodule and remains optional in configurations that do not have that header.

**Consequence:** keep the host local and off the network. Its observer/controller switch selects permissions but does not authenticate callers. C# IPC, MCP, multi-client service, and WRSR control remain separate future work and need their own versioned contracts and security review.

## D-008: The data-driven economy core is the target simulation; `src/sim` is a demo

**Date:** 22 October 2026
**Status:** Accepted

An architecture review found `src/sim` (hard-coded Farm/Mill/Storage, grain/flour fields baked into the snapshot, API, save and UI) cannot host authentic content. A new standalone `src/economy` library (catalog, deterministic world, haul orders, explanations, save/load) with its own typed API and JSONL host (`src/economy_api`, `src/economy_host`) is the migration target. The old simulation, API and desktop UI remain during migration. The map-region view now has a bounded human adapter to the separate economy host, but the host still owns the authoritative world process and the main demo UI remains intact elsewhere in the app.

**Consequence:** new gameplay work targets `src/economy*`; authentic content enters only through adapters (`src/compat`, local under D-005) that produce `economy::Catalog` data, never by adding resource types to `src/sim`. The desktop app's content preview code now lives in `ContentPreview.*` so presentation and content import stay separate from simulation.

**Update 2026-10-22:** the owner-authorized source publication is recorded under D-005. `origin/main` currently includes source through `da8f460`; distribution rights for original content and binary releases remain unresolved. A local working-tree increment now makes the map-region path app-owned by default: it imports a bounded region into `economy::World`, uses the typed `EconomyApi` for human actions, and maps rendered placements to imported IDs through the source index from that exact loaded placement vector. The optional explicit `--economy-url` path remains a separate host and retains session binding and approximate matching. Client and headless Release builds succeed and both full test suites pass 29/29; importer tests cover exact key mapping and duplicate-key failure transactionality. UI runtime acceptance remains open, external controller access to the app-owned instance is not implemented, and the loaded-vector index is not a durable cross-version identity. The next integration target is a versioned local controller interface into the same app-owned world, followed by synthetic-fixture graphical acceptance. Package identity is not fingerprinted.

## D-009: Use an opt-in Windows named pipe for in-app controller access

**Date:** 22 October 2026
**Status:** Accepted for the initial app-owned controller endpoint

`--economy-pipe-name <name>` exposes the existing map-region `EconomyApi` through a Windows named pipe at `\\.\pipe\<name>`. The embedded JSONL dispatcher borrows the app's API instance, requires session binding after `hello`/`capabilities`, and does not advertise or accept `shutdown`. A transport worker performs bounded line framing and response handoff only; the app update thread executes dispatcher calls against the same world used by the UI. The endpoint is disabled by default and remains separate from the standalone `OpenRepublicEconomyHost` and `--economy-url` bridge.

**Consequence:** this is a trusted-local controller surface, not authentication. The pipe uses the process token's default security descriptor and rejects remote clients, so any local process allowed by that descriptor can issue controller commands. The initial transport serializes one client at a time; a connected client that never completes a line can retain the slot until disconnect or app shutdown. Keep protocol v0 bounds, session/revision checks, and lifecycle limits explicit. A synthetic/open-fixture UI interaction is still required before treating the graphical control path as accepted.


## D-010: Bind native economy checkpoints to their active catalog

**Date:** 22 October 2026
**Status:** Accepted for standalone checkpoint export/restart

Wrap the existing native world save with exact canonical active-catalog bytes in
a bounded versioned checkpoint. Owned restore keeps the catalog alive for its
world; loading into an explicitly supplied catalog requires exact canonical
identity. Use immutable chunked export through the typed query API and shared
JSONL dispatcher, then bounded file restore before a standalone API session is
created. File publication belongs to the client, with exclusive creation by
default and explicit replacement. Do not accept arbitrary host filesystem paths
through JSONL or implement an unversioned live-world reset behind existing handles.

**Consequence:** a new process retains authoritative world state and pending
commands but creates fresh sessions, handles and event journals. The embedded
active catalog becomes the base for subsequent tuning; historical Lua source and
region/render provenance are not synthesized. Payload bounds remain 1 MiB per
section and 2 MiB + 128 total. Legacy raw world saves remain separately supported
and depend on their caller-supplied catalog. This is native persistence, not
WRSR save support or authenticated history. Desktop scene/bootstrap restoration
and long-running history/size limits require separate work. Source/build and
synthetic process/HTTP evidence are recorded in the checkpoint report.


## D-011: Reconstruct native checkpoints with exact-ID primitive presentation

**Date:** 22 October 2026
**Status:** Accepted for desktop native checkpoint startup

The desktop owns the restored catalog/world pair and presents that same
simulation through native-ID building/road bindings and a shared centered X/Z
transform. Use authored primitive geometry and raw inventory units. Do not
fabricate package/source indices or map asset provenance absent from the file.
Four display units per cell is a view policy, not a physical-unit assertion.

Validate the renderer-independent double plan and float precision before window
startup or scene replacement. Reconcile geometry from authoritative snapshots;
keep ordinary camera/origin stable. Frame world explicitly recenters/frames the
current world, preserving selection by native ID without simulation mutation.
On later precision failure retain the last scene, label it stale and disable
picking; an explicit reframe can recover a compact relocated cluster.

**Consequence:** native checkpoint playback does not require original content.
Huge simultaneous spans and floating-origin streaming remain separate renderer
work. Startup restore is distinct from in-place GUI loading. The optional economy
pipe uses the same owned API, and dispatched mutations refresh presentation.
See the [desktop receipt](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md).


## D-012: Compact native command history explicitly and retire session mappings

**Date:** 22 October 2026
**Status:** Accepted for native long-running history maintenance

Use a monotone retained-ID floor to discard only a completed prefix through an
explicit controller/revision-checked operation. Preserve every queued result and
pending payload, minting counters and live simulation state. KeepRecent ranges
from 0..4096. Actual pruning advances revision once without a tick, respecting
pending execution headroom; no-op requests and export queries leave world bytes
and hash unchanged. No automatic core pruning occurs inside stepping/export.

Persist compacted state as native v3 with HISTORY after NEXT; read legacy v1/v2
with floor one. Preserve uncompacted v2 serialization/hash values. Validate the
retained dense interval/queued suffix and lifetime applied-command revision
floor using all minted command IDs, including expired history. Synthetic Expired
lookup states are not persisted results. The checkpoint wrapper remains v1.

Retire API handle maps below the floor, distinguish expired issued handles from
unknown ones, and use native commandId zero when its retired mapping is no longer
available. Do not equate local handles with native IDs or retain growing expiry
tombstones. Existing bounded event records remain readable with cursor limits;
maintenance adds no fake tick or command-outcome event.

**Consequence:** callers must record historical outcomes before discarding them.
The baseline autopilot discovers the method and keeps 1024 completed results
after its outcome polls by default; all explicitly opts out. Live-entity/catalog/
pending payload limits and other journals remain separate contracts. Older
executables do not load v3 compacted state. See the
[command-history receipt](DEVELOPMENT_REPORT_2026-10-22_COMMAND_HISTORY.md).


## D-013: Native construction uses authored cells and authoritative queued commands

**Date:** 22 October 2026. **Status:** accepted for the local native sandbox.

Provide an explicit `--native-economy` empty authored world and use the same tools
for native checkpoint restores. Convert ground points through the current retained
native scene transform; use exact native IDs and integer cells. A draft only shows
intent. The C++ economy validates placement, reservations, roads and orders when
stepped through the typed, revision-bound API. Reject stale scene writes.

Represent straight roads as bounded independent unit commands, preserving partial
admission feedback and the unsubmitted remainder. Do not imply atomicity. Use
explicit catalog vehicle selection and positive storage-capacity intersections
for resource choices rather than guessing from a building's first recipe. Keep
stock/route validation and production authoritative in the core.

Expose existing removal semantics with an inventory-discard warning and retained
haul-endpoint rejection. Do not invent construction costs, refunds, workforce or
original-game behavior. Keep current and pre-update GUI input guards so palette
interaction cannot also issue world placement. Source/tests, GUI observations,
checkpoint replay and unverified keyboard/performance acceptance remain separate.
See the [native construction report](DEVELOPMENT_REPORT_2026-10-22_NATIVE_CONSTRUCTION.md).

## D-014: Utilities are finite tick supply on explicit resource networks

**Date:** 22 October 2026. **Status:** accepted for the independent native economy.

Declare utilities in the immutable catalog; forbid warehouse stock and vehicle cargo.
Keep explicit resource-specific undirected building links in authoritative C++ state,
mutated through queued revision/session-checked API commands shared by GUI and agents.
Generators commit enabled/fuel/material-output checks before offering supply; consumers
receive complete demand in building/recipe order only after all resource/output gates.
Use exact aggregate accounting, discard surplus, and document always-on fuel burn.
Utility recipes require interval one and generators cannot consume utilities in this
first model. Capacity/loss/voltage/startup power and original-game parity remain open.

Persist links, retained utility command identity and stall receipts as native v4 when
required, including isolated utility recipes and no-edge history. Preserve material-only
v2/v3 bytes, exact checkpoint catalogs and fail-closed older readers. Compatibility
utility declarations are explicit opt-in names, never a consumption bypass or automatic
original topology claim. Lua tuning preserves kinds. See [utility contracts](UTILITY_NETWORKS.md)
and [verification](DEVELOPMENT_REPORT_2026-10-22_UTILITIES.md).

## D-015: Population and workforce are opt-in synthetic economy rules

**Date:** 23 October 2026. **Status:** accepted for the independent native economy.

Catalogs may declare housing capacity and worker requirements per building type,
plus an optional per-resident food consumption interval. Keep catalogs without
these declarations on the existing simulation/hash path. Residents fill housing
under the authored housing/food rules; available residents are assigned to
worker-requiring buildings in deterministic building order, and their recipes
scale with staffed fraction. Expose totals through the typed API summary and
protocol v0 using decimal strings for uint64 values while preserving the legacy
summary fields for catalogs without the model.

These declarations and formulas are OpenRepublic synthetic behavior only.
WRSR parity for population, migration, food consumption, worker assignment, and
production scaling is UNKNOWN. See [synthetic population and workforce](POPULATION.md).
