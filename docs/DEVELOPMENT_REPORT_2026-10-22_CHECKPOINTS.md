# Development continuation: native economy checkpoints

**Date:** 22 October 2026 (America/New_York)
**Source base:** `d409d91`, retaining the local logistics, save-integrity, access/UI and replanning increments.
**Status:** checkpoint implementation and synthetic verification complete for this increment. Full client/headless Release builds and CTest each pass **49/49**. Changes remain local and uncommitted. The broader playable-alpha objective remains incomplete.

The latest reports identified persistence and catalog identity as useful development goals. Native `SaveWorld` already preserved simulation state, but the host had no restart path and callers supplied its catalog separately. A wrong catalog could silently give numeric resource/type IDs different meanings. The new checkpoint bundles the exact active catalog with the native world and provides a usable export/restart workflow.

## Native ownership and format

`economy/checkpoint.h` exposes `SaveCheckpoint`, `RestoreCheckpoint` and `LoadCheckpoint`. `RestoreCheckpoint` returns an owning `RestoredCheckpoint`: its heap catalog remains alive for the world, including move construction, replacement and self-move handling. Restore failures preserve the previous owned pair. `LoadCheckpoint` is the expected-catalog variant: it requires exact canonical catalog equality before replacing the destination world transactionally. Names, ID order, recipes, amounts, intervals, storage and vehicle definitions all participate in that equality.

Checkpoint v1 has three exact LF-delimited header lines:

```text
OPENREPUBLIC_ECONOMY_CHECKPOINT 1
CATALOG_BYTES <N>
WORLD_BYTES <M>
```

The header is followed immediately by N bytes of canonical catalog text, then M bytes of native world text. There is no additional delimiter or trailing data. Lengths are nonzero canonical unsigned decimals; signs, leading zeroes, overflow, truncation and trailing bytes reject. Each section is limited to 1 MiB and the total to 2 MiB + 128 bytes (2,097,280). The catalog must parse and reproduce exactly through `WriteCatalogText`. Saving explicitly fails if a programmatically valid catalog cannot round-trip through the text parser, such as an overlong recipe line. A failed save leaves the caller's output unchanged.

The existing native world v1/v2 parser and v2 writer remain unchanged. Checkpoints retain tick/revision and ID counters, inventories, progress, loaded trucks, queued commands and retained native results. The wrapper carries exact catalog identity; legacy raw world saves still depend on a separately supplied catalog. Neither format authenticates data or proves an economic history.

## API and immutable export

The typed API adds read-only, revision-checked `ExportCheckpoint` and advertises `export_checkpoint` to query-capable callers. It returns `stale_revision` for the wrong revision and `persistence_failed` with a diagnostic if serialization is unsupported or exceeds limits. Export does not advance ticks, apply pending commands, change handles, or append events.

The JSONL dispatcher adds two query methods, available to observers and controllers:

- `begin_checkpoint` requires the decimal-string `revision`. It caches one successful immutable checkpoint and returns `checkpointId`, `totalBytes`, frozen `revision`/`tick`, and `maxChunkBytes`.
- `read_checkpoint` requires `checkpointId`, integer raw-byte `offset`, and integer `limit` from 1 through 16,384. It returns `dataBase64`, offset/nextOffset, totalBytes, hasMore and the frozen metadata. Reading exactly at EOF is valid and returns an empty slice.

A new successful begin invalidates the previous ID. A failed begin, including size failure, preserves the old cache. World steps, rule reloads and newly queued commands do not change an existing captured snapshot. The cache is bounded to one payload per dispatcher. Base64 preserves arbitrary catalog bytes and avoids corruption when a byte boundary splits UTF-8. Session checks and permission handling use the existing trusted-local protocol; neither checkpoint IDs nor session IDs are authentication.

## Files and restart workflow

`tools/economy_checkpoint.py` exports from the live HTTP bridge or a Windows economy named pipe. It binds to the hello session, validates chunk identity/metadata/lengths, retries a stale begin once, and writes the assembled bytes to a sibling temporary file. It flushes/fsyncs the file before publication. Default publication uses an exclusive hard link and refuses an existing destination; `--force` explicitly selects atomic replacement. The output parent must exist, and default publication requires filesystem hard-link support. This is atomic publication, not a power-loss durability guarantee. No filesystem path is passed to the host's JSONL API.

```powershell
python tools/economy_checkpoint.py --url http://127.0.0.1:8950 --output <new-file.checkpoint>
python tools/economy_checkpoint.py --pipe OpenRepublicController --output <new-file.checkpoint>
OpenRepublicEconomyHost.exe --role controller --checkpoint <file.checkpoint>
```

The host reads at most the configured total bound plus a one-byte oversize probe. Restore occurs before creating the dispatcher/API. `--checkpoint` excludes any explicitly supplied catalog, rules, region, content-root, center or radius option; a role may still be selected. Invalid, missing, oversized and conflicting inputs exit with code 2 on stderr before protocol output. No original installation is needed for a synthetic checkpoint.

A restarted host has a new session, API command handles and event journal. Old handles cannot be polled in that session. Native pending commands remain queued and execute on the next tick; callers inspect the resumed world and create new handles for subsequent work. The checkpoint carries the active tuned catalog, so future Lua tuning is relative to that embedded catalog. Prior Lua source/history and region package/render provenance are not reconstructed. Genuine imported-world state and normalized definitions remain local under D-003/D-005; checkpoint export does not grant distribution rights.

## Verification receipt

**VERIFIED — Windows Release builds/tests:** full client and headless builds succeeded. Concurrent full CTest runs pass **49/49** each. Five added entries are `EconomyCheckpoint`, `EconomyCheckpointApi`, `EconomyCheckpointProtocol`, `EconomyCheckpointClient`, and `EconomyCheckpointExternalController`.

Native tests cover naturally loaded cargo and a pending retarget, exact state/canonical resave, per-tick continuation, fresh counters, owned lifetime/moves, exact expected-catalog mismatches, malformed framing/payloads, limits, oversized catalogs and long-line round-trip failure. API tests cover permission/revision checks, unchanged handles/events, queued replay and explicit oversized-save failure. Protocol tests cover binary/NUL/invalid-UTF-8 bytes in one-byte slices, multiple full chunks, frozen reads across mutation, stale/oversized begin preserving the prior cache, replacement invalidation, strict fields/bounds and observer/no-query permissions. The initial multichunk fixture was too short to cross 16 KiB; it now uses 1,600 retained results and verifies that its payload actually crosses that boundary.

The client entry runs **8** Python cases for byte reconstruction, malformed chunks, bounded stale retry, no-overwrite/force behavior, cleanup, missing parents, session/response identity and pipe-name validation.

**VERIFIED — separate processes:** the original host produces and freezes 12 grain units, loads four aboard a truck and queues a destination retarget. After active rule tuning and checkpoint export, a second host starts from the file at the same tick/revision/hash with the cargo still aboard and retarget still pending. Their hashes match after every one of **32** further ticks and stored plus onboard grain remains **12**. The retarget applies after restart. Old session IDs/handles reject, the event journal and rule history start empty, and a 100% Lua reload uses the embedded tuned truck capacity of eight as its base. Both processes exit 0 with no stderr.

**VERIFIED — live HTTP CLI:** an ephemeral loopback bridge fronts a checkpoint-restored observer host. The actual CLI exports byte-identical checkpoint data, refuses an existing file by default, and succeeds with explicit `--force`. The bridge and host are closed after the test. Startup tests reject missing/empty/truncated/trailing/oversized files and every excluded explicit flag before protocol startup.

```powershell
cmake --build build/headless-host --config Release --parallel 6
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
ctest --test-dir build/headless-host -C Release --output-on-failure
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
python -B tests/economy_host/checkpoint_controller.py build/headless-host/src/economy_host/Release/OpenRepublicEconomyHost.exe
```

No GUI run, original-game comparison, live named-pipe export test, performance measurement or cross-platform replay test ran for this increment. The desktop client compiles the shared exporter, but desktop checkpoint restore and restoration of scene/source bindings remain future work. Earlier synthetic GUI evidence retains its own receipt. No original game process was launched, no game/save installation was modified, and no proprietary fixture was added.

## Next useful development goals

1. Add desktop checkpoint bootstrap with explicit reconstruction of the rendered scene and stable building selection bindings from the restored authoritative world. Keep genuine content/package identity separate from native economy/catalog identity.
2. Address long-running native persistence limits: retained command history and the 1 MiB world bound can still make a world unsaveable. Define an explicit versioned compaction/history contract before enlarging or pruning state.
3. Profile route search, API snapshots and event capture with matched haul-heavy worlds before changing algorithms or adding parallelism.
4. Complete the synthetic GUI Farm-to-Mill-to-Bakery/market flow and separate genuine-content fidelity/original-game economic comparisons.


## Later continuation: desktop restore accepted

The [desktop checkpoint report](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md)
supersedes this report's desktop/live-pipe next-target statements. Native startup,
exact native-ID primitive bindings, structure reconciliation and explicit framing
are implemented. Full Release suites now pass 50/50 headless and 51/51 client.
The live desktop matches a reference host through queued structural changes;
GUI picking, framing, truck turning and mill production are observed, and actual
pipe CLI export resumes the resulting world with the same hash. Historical
49/49, no-GUI and no-pipe statements above describe this earlier increment only.
Long-running persistence and platform/WRSR comparison remain open.
