# Explicit command-history compaction - 22 October 2026

Development remains active over source base `d409d91`. This increment addresses
native save growth caused by retaining every completed command result. The core
now supports explicit history compaction, the typed API retires matching handle
storage, JSONL exposes a versioned maintenance contract, and the baseline
controller performs maintenance after polling its round's outcomes. Changes are
local and uncommitted; earlier dirty work is preserved.

## Result and policy

Before this change, every issued native command remained in the results map and
every API handle remained in its session map. A small world with a long edit or
controller history could exceed the 1 MiB native save limit. Raising the byte
limit would only postpone that growth. Implicit pruning during a tick or export
would also change result availability without an explicit maintenance boundary;
tick-time pruning could remove a rejection before the API records its event.

`World::CompactCommandHistory(keepRecent)` explicitly keeps the newest requested
number of terminal results and every queued command/result. The accepted range
is 0..4096; requesting more rejects without mutation. No automatic core pruning
occurs, and save/checkpoint queries remain immutable. A request that removes no
results preserves the exact world bytes, revision and hash. Actual pruning
increments revision once, without advancing a tick or modifying live entities,
inventory, production progress, cargo, queued payloads or ID allocation counters.
Revision headroom reserved for pending applications and their tick is protected.

The retained command-ID floor distinguishes intentionally discarded results from
never-issued IDs. Core lookup returns `Expired`, failure `None`, below that
floor. Zero and future IDs remain `Unknown`. An expired result does not recover
or reinterpret its former accepted/rejected outcome; controllers must record any
outcomes they need before maintenance.

The typed API requires query plus control permission and an exact expected world
revision. On success it removes handle mappings and rejection markers associated
with discarded native IDs, and reports both removed-result and expired-handle
counts. An issued, retired session handle polls as `command_expired` / `expired`;
a never-issued handle polls as `unknown_command`. When the native-ID mapping has
been retired, the C++ result's commandId is zero, explicitly unavailable. API
handles and native command IDs are independent sequences and must not be
substituted for each other. An attached API can still recognize native expiry
caused directly by a core caller; a successful no-op API maintenance call also
cleans up those mappings. New process/API sessions inherit no old handles.

The bounded event journal is retained with its existing cursor semantics. Old
events may reference now-expired handles. Maintenance does not fabricate a
command rejection or tick event, and existing rejected commands remain recorded
once. It also leaves an already-frozen checkpoint cache readable until a new
successful begin replaces that snapshot.

## Versioned native persistence

The writer preserves canonical world v2 bytes and the previous hash algorithm
while the retained floor is one. After history has been discarded, it writes
world v3 with one additional line immediately after NEXT:

```text
OPENREPUBLIC_ECONOMY_WORLD 3
GRID <width> <depth>
TICK <tick>
REVISION <revision>
NEXT <building> <road> <order> <vehicle> <command>
HISTORY <first-retained-command-id>
BUILDINGS <count>
...
```

Readers accept v1/v2 with an implicit floor of one and v3 with an explicit floor
from one through nextCommand. Results must be exactly the dense interval
`[floor, nextCommand)`. Pending IDs must be the complete queued suffix
`[nextCommand - pendingCount, nextCommand)` and cannot be compacted. Earlier
retained results must be terminal; synthetic Expired states are never valid
persisted results. An all-expired history may have floor equal to nextCommand
and no result records.

Load validation counts lifetime applied commands as
`nextCommand - 1 - pendingCount`, including discarded history, and requires that
count to fit `revision - tick`. Pruning cannot weaken that existing invariant.
Native IDs remain monotonically allocated. A domain-tagged retained floor joins
the state hash only when the floor exceeds one; an intentionally compacted world
therefore has a different full-state hash from an uncompacted world. Restoring
the same compacted state preserves its exact hash and continuation.

The checkpoint wrapper remains v1 because its existing length-framed native
world payload already carries its own version. Catalog/world and total byte
limits remain unchanged. Older executables cannot load v3 compacted worlds.

## Controller usage

Discover `commandHistoryVersion: 1`, `commandHistoryPolicy: "explicit"`, the
controller action `compact_command_history`, method `compact_history` and
`limits.maxCommandHistoryRetention: 4096` through hello. World responses add:

```json
{"commandHistory":{"version":1,"firstRetainedCommandId":"42","nextCommandId":"77","retainedTerminalResults":"32","pendingCommands":"3"}}
```

All four 64-bit values use decimal strings. After polling required outcomes,
use the current world revision:

```json
{"id":"maintenance-1","protocolVersion":0,"sessionId":"<current-session>","method":"compact_history","revision":"<current-revision>","keepRecent":1024}
```

The result contains errorCode, decimal-string removedResults, expiredHandles and
revision, plus commandHistory. Permissions, strict fields, session binding and
stale-revision handling apply. Invalid retention is a typed `invalid_argument`
or wire `invalid_request`; insufficient revision headroom is
`revision_exhausted`. Refresh the world after successful maintenance before
using its new revision. Export does not compact for the caller.

The baseline autopilot discovers the method and, after polling all placement,
retarget and create outcomes in a round, retains 1024 completed results by
default. `--retain-command-results 0..4096` selects another bounded count;
`--retain-command-results all` disables this maintenance. Older hosts without the
method remain supported. A stale maintenance request triggers at most one
refresh/retry. Text and JSON results report retired-result counts and the JSON
benchmark includes the selected retention policy. This controller choice does
not change the core's explicit-only policy.

## Verification receipt

**VERIFIED - source/build/automated tests:** both complete Windows Release builds
succeed. Concurrent full CTest passes **54/54 headless** and **55/55 client**.
New entries are EconomyCommandHistory, EconomyCommandHistoryApi,
EconomyCommandHistoryProtocol and EconomyCommandHistoryExternalController. The
autopilot unit entry now contains **16** cases.

Core tests cover accepted/rejected prefix pruning, queued preservation, zero and
maximum retention, no-op immutability, monotonically minted commands, v1/v2
compatibility, v3 round-trip with loaded cargo and pending retarget, all-expired
history, malformed/signed/overflowing floors, gaps, persisted Expired rejection,
lifetime revision floors and exact exhaustion/reservation boundaries. A large
pending queue remains unsaveable after terminal compaction, with its queued work
still present. Three cycles of 75,000 completed commands each exceed the save
bound before pruning, then save/reload and continue canonically after keeping 32.

API/protocol tests cover permission/revision/count failures, no-op behavior,
expired versus unknown handles, offset native/API IDs, direct-core expiry and
no-op API cleanup, retained queued lifecycle, rejection-event stability,
session binding, role discovery and frozen checkpoint cache behavior. Independent
review caught the original expired-handle/native-ID substitution; it was fixed
and covered by a fixture where API handle one maps to native command three. An
initial protocol assertion expected observer hello to omit all control-method
descriptions; it was corrected to the established contract, which lists methods
with allowed roles while omitting the action from observer capabilities.

**VERIFIED - actual separate processes:** a synthetic loaded-truck fixture
accumulates **75,008** additional commands and applies them to terminal results.
Checkpoint begin correctly fails as oversized without changing the state hash.
With another retarget queued, keeping 32 terminal results removes **74,996** core
results and **74,996** API mappings. Entities, roads, vehicles and orders compare
equal immediately before/after maintenance; the tick is unchanged and revision
advances once. Old/recent/pending handles respectively expire/remain terminal/
remain queued. The resulting checkpoint is **1,395 bytes**, embeds world v3,
and resumes with identical history metadata and state hash in a fresh session.
The two hosts match every hash for **24** more ticks, conserve all **12** grain
units and apply the queued retarget. Final tick is **38**, hash
`7497992024173021514`.

The test also invokes the actual autopilot CLI twice against that compacted
checkpoint. Two one-tick rounds with retention zero retire **33** results; the
same invocation with `all` retires **0**. Both exit cleanly. These subprocess
cases run in both complete suites; all helpers use isolated synthetic temporary
files and close their host processes.

**VERIFIED - prior persisted state compatibility:** the synthetic checkpoint
exported after the earlier desktop GUI run still loads at tick 44 and exact
pre-change hash `6564545351195520907`, with floor one and all 53 results retained.
This is an existing native checkpoint compatibility check, separate from a new
graphical run.

```powershell
cmake --build build/headless-host --config Release --parallel 6
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
ctest --test-dir build/headless-host -C Release --output-on-failure
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
```

## Limits and next useful work

Compaction intentionally discards old command outcomes. Record them externally
before retiring them if audit/replay workflows need them. Existing event and Lua
history facilities retain their own contracts; this does not supply authenticated
history, new world reset semantics or complete historical replay.

The current world can still exceed 1 MiB through live entities or queued work,
and catalog payload/round-trip bounds still apply. Explicit maintenance addresses
completed-result accumulation rather than promising arbitrary world sizes. API
storage is retired only through successful API maintenance; a core-only caller
does not automatically clean every attached API instance. A long round may also
accumulate work before the autopilot's end-of-round maintenance.

No GUI or original-game comparison, cross-platform replay or performance benchmark
ran for this increment. The desktop compiles the shared v3 reader and dispatcher;
new graphical acceptance of v3 maintenance remains **UNKNOWN**. No original game
files, assets or saves were used or modified. Changes remain uncommitted and the
broader playable-alpha goal remains active.

Next useful development is native construction/resource-flow UI and complete
Farm-to-Mill-to-Bakery-to-Market acceptance, followed by matched route/snapshot/
scene profiling and an explicit larger-world persistence strategy. Earlier
[desktop](DEVELOPMENT_REPORT_2026-10-22_DESKTOP_CHECKPOINTS.md) and
[checkpoint](DEVELOPMENT_REPORT_2026-10-22_CHECKPOINTS.md) reports retain their
separate graphics and format/export receipts.
