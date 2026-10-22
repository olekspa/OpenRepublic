# Desktop native checkpoint continuation - 22 October 2026

Development remains active over source base `d409d91`. This local increment adds
desktop startup from a catalog-bound native checkpoint, exact native scene
bindings, geometry reconciliation and live pipe export acceptance. Changes remain
uncommitted and unpublished. Earlier dirty work is preserved.

## Result and architecture

`OpenRepublic.exe --economy-checkpoint <file>` restores the embedded catalog and
authoritative C++ economy before creating the window or API session. Startup uses
bounded binary reads and rejects malformed, oversized, conflicting or
undisplayable input with exit 2. The restored catalog/world are owned together;
the API and pipe dispatcher borrow that same world. Startup does not advance a
tick, reimport placements or mint replacement IDs. Pending native commands apply
on the next simulation step. Sessions, handles and event journals are new.

The native view uses OpenRepublic-authored primitive geometry. A headless runtime
planner maps native footprint/access/road/vehicle coordinates into one centered
X/Z transform, using four display units per cell as presentation policy. These
units do not assert physical dimensions. Building and road primitives are keyed
by native IDs; picking resolves renderer entities through exact native bindings.
Gapped IDs and repeated building types therefore remain distinct. No placement
source index, original package identity, asset provenance or historical Lua
source is invented for a native checkpoint.

The Wicked adapter retains unchanged entities and reconciles additions, removals
and changed geometry from current snapshots. Roads include both X and Z segments
and disabled-state presentation. Truck positions use the same transform in both
axes. Inventories display native integer amounts because a checkpoint does not
carry imported display-unit metadata. Human controls and the optional local pipe
use the existing typed API over the restored world.

Pipe dispatch now refreshes the desktop snapshot when requests change the world
revision. Controller steps and queued structure changes become visible without a
manual refresh. This also fixes stale presentation for the existing app-owned
region endpoint. Ordinary synchronization preserves the camera and native view
origin. The native-only **Frame world** button explicitly recenters geometry and
frames its current bounds while preserving selection by native ID.

The double-precision planner is separate from the float-renderer precision gate.
The gate checks bounds, centers, corners, dimensions and framing values before
scene mutation. Compact clusters near large native origins can be recentered;
worlds whose simultaneous occupied span cannot preserve cell precision in this
renderer reject at startup. If later edits exceed the fixed-origin precision
budget, the last valid scene is retained with a stale-view message and picking
disabled. Frame world can recover a compact cluster at a new origin. The
authoritative world is not altered by these presentation checks.

## Usage

Run from the client build directory so Wicked Engine's copied Content is found:

```powershell
.\src\app\Release\OpenRepublic.exe --economy-checkpoint <file.checkpoint> --economy-pipe-name OpenRepublicController
```

The pipe is optional; graphics selection and `--modern` remain available.
Checkpoint startup excludes map/content import, model/scene probes, capture
options, the synthetic fixture, a separate economy URL and the scene-control
pipe. It does not combine independent world sources. The economy pipe retains
its existing trusted-local session and access policy, with no remote clients.

Export the live desktop world to a new file, then resume it in either executable:

```powershell
python -B tools/economy_checkpoint.py --pipe OpenRepublicController --output <new-file.checkpoint>
OpenRepublicEconomyHost.exe --checkpoint <new-file.checkpoint>
```

The existing export client publishes only completed validated data, refuses an
existing file by default, and accepts explicit `--force`. The host receives no
arbitrary filesystem output path. Checkpoints remain bounded to 1 MiB per section
and 2 MiB + 128 bytes total.

## Verification receipt

**VERIFIED - source/build/automated tests:** full Windows Release builds succeed.
Concurrent full CTest runs pass **50/50 headless** and **51/51 client**. The added
`RuntimeNativeEconomyLayout` entry exercises exact/gapped/reordered IDs, repeated
types, X/Z roads, fixed-origin structure changes, empty/maximal grids, large
origins, precision rejection/recentering and transactional malformed-input
handling. The client-only `EconomyDesktopCheckpointArguments` entry rejects
missing/empty/duplicate options, missing/oversized files and exclusive-mode
conflicts before any graphics window. Automatic CTest does not perform GPU UI
acceptance.

```powershell
cmake --build build/headless-host --config Release --parallel 6
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
ctest --test-dir build/headless-host -C Release --output-on-failure
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
```

**VERIFIED - live desktop/pipe process comparison:** the manual helper
`tests/economy_host/desktop_checkpoint_controller.py` creates a synthetic native
world at nonzero coordinates with gapped building IDs 2, 3, 4, 5, X/Z roads, four
units aboard a truck and pending retarget/removal/placement/road changes. The
desktop starts at tick 14, revision 53, state hash `16367608665814369290`, equal to
the headless reference. Four subsequent steps match its hash at every tick and
conserve all 24 stored/onboard units. At tick 18, revision 69, the hash is
`16699341338584550715`, building IDs are 2, 3, 5, 6 and the original loaded order
targets mill 3. A live named-pipe export resumes in another host at the same hash.
The first manual fixture placed the new market outside the 64-cell grid; command
validation correctly rejected it. The helper was corrected to x=60 and the full
acceptance run repeated from a fresh checkpoint.

**OBSERVED - owner-authorized Windows GUI:** the restored primitives and loaded
truck render. Pipe-driven steps visibly remove the old bakery, add the market,
extend roads and display the closed segment without manual refresh. Clicking
the new market selects **OR-6**. Frame world reframes the view and retains OR-6;
a pipe query confirms tick/hash unchanged. GUI toggle and single-step actions
disable that market and move the truck through the road turn. At tick 20, a
pipe query verifies truck cell (46,44), cargo four and market disabled. Selecting
restored mill **OR-3**, enabling it and stepping 24 ticks reaches tick 44 with
the mill running, grain 2/8 and flour 6/8, also visible in its selected status.

**VERIFIED - actual live pipe export CLI after GUI actions:** exporting the tick
44 world produces a 2,335-byte checkpoint at revision 97. A new headless host
resumes at exact hash `6564545351195520907`, retaining the running mill's grain
two/flour six and disabled market. The task-owned desktop and helper hosts were
closed after acceptance. Synthetic test artifacts stay in ignored build output;
no screenshot file was exported for this receipt.

## Limits and next useful goals

These receipts establish native OpenRepublic persistence and primitive desktop
integration on this Windows build. WRSR behavior, content fidelity, cross-platform
replay and graphics/performance scaling remain **UNKNOWN**. No original game
process, asset or save was used or modified. Genuine imported checkpoints retain
the existing private-research and distribution boundaries.

1. Define versioned long-running persistence semantics: retained command history
   can still exceed the 1 MiB world limit. Specify compaction and result lookup
   behavior before pruning history or changing the native schema.
2. Extend native human construction and resource-flow controls, including a
   complete Farm-to-Mill-to-Bakery-to-Market acceptance case. Current startup
   restore and external export do not add in-place GUI load/save dialogs.
3. Profile route search, snapshots and scene reconciliation with matched larger
   worlds. Explicit framing is available; floating-origin streaming and huge-span
   scene presentation remain separate work.

Related receipts: [checkpoint format/export](DEVELOPMENT_REPORT_2026-10-22_CHECKPOINTS.md),
[autonomous replanning](DEVELOPMENT_REPORT_2026-10-22_REPLANNING.md) and
[access/UI](DEVELOPMENT_REPORT_2026-10-22_ACCESS_AND_UI.md).
