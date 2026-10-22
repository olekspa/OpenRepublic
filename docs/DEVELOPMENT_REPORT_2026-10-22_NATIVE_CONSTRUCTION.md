# Native construction and resource-flow controls - 22 October 2026

Development remains active over source base `d409d91`. This local increment adds
a fresh native economy sandbox and desktop construction/haul controls over the
authoritative economy. A complete production chain was built and connected
through the GUI, then exported and resumed through the native checkpoint path.
Changes remain uncommitted and unpublished; earlier dirty work is preserved.

## Result and implementation

`OpenRepublic.exe --native-economy` creates an empty 64 x 64 native world using
the existing OpenRepublic-authored synthetic catalog. The same owned catalog/world
pair and exact-ID primitive view serve this startup and `--economy-checkpoint`.
The new option excludes checkpoint/map/content/probe/capture/remote-economy and
scene-control modes; graphics selection, --modern and the optional economy pipe
remain available. Duplicate/conflicting flags fail before the graphics window.
No original installation or prebuilt checkpoint is needed to begin construction.

The native toolbar has catalog building, haul-resource and vehicle selectors,
Place building, Build road, Cancel and Remove selected. Building and vehicle
palettes select their first entries initially. Existing one/24-tick controls,
toggle/source/destination actions and Frame world remain available, plus a
native 240-tick advance. Controls wrap at the viewport width; status moves below
the wrapped rows and retains its overflow scrollbar. Native selected status also
shows each recipe's completed cycles and remaining ticks.

Placement maps the analytic y=0 pointer-ray intersection through the scene's
retained transform. The inverse uses double precision and half-open cell areas,
with finite/range/grid checks before integer conversion. It does not use legacy
demo rounding, rebuild the origin or require hitting the current ground mesh.
The clicked cell is the footprint minimum X/Z cell. A gold footprint/cell marker
and native hover coordinates identify the draft, without asserting that it will
pass validation. A queued building returns to inspection mode. Cancel clears
the draft without cancelling commands already submitted.

Road drawing takes two distinct cells on one row or column, at most 64 segments
apart. It submits bounded native unit-road commands under the current revision,
within the remaining 64-handle human-command budget. Each segment validates at
the next step independently; this is not an atomic multi-segment construction
transaction. Invalid geometry preserves the start. If admission stops partway,
the UI preserves the first error and advances the draft start to the last queued
endpoint so a retry does not resubmit its prefix. Terminal feedback reports
applied/rejected/unavailable counts and the last issue, surviving the step notice.

Haul resource choices intersect the positive-capacity storage slots of the exact
source/destination native IDs, in catalog resource-ID order. Recipes and current
inventory do not define eligibility: storage-only endpoints and temporarily empty
sources remain usable. The selected resource and vehicle names go to the typed
CreateHaulOrder API. No recipe-based guess or renderer identity substitutes for
native identity in this mode. Other imported/remote presentation modes retain
their existing adapter behavior.

GUI widgets own no simulation. Placement, unit roads, demolition and hauls all
use EconomyApi and existing queued-result polling. Demolition retains the core's
existing behavior: it discards the removed building's inventory and rejects
buildings referenced by haul orders. There are no construction costs/refunds,
workers, delayed construction sites or recovered WRSR construction rules.

Dropdown/typing input gets priority over the passive status-label exception,
including pre-update active state so dismissal cannot also click the world.
The native preview stops over controls and stale/unrepresentable scenes. The
existing precision/framing contract remains in force. Initial scene creation
now retains Sync's calculated camera target instead of overwriting it with zero.

## Usage

Run from the client build directory, where the copied Content folder is present:

```powershell
.\src\app\Release\OpenRepublic.exe --native-economy --economy-pipe-name OpenRepublicController
```

1. Choose a building type, click Place building, then click its footprint origin.
2. Draw straight roads between cells below the footprints' access edges.
3. Step to apply construction; inspect any rejected-command explanation.
4. Select a producer and Set haul source, select its destination, inspect/change
   the resource and vehicle palettes, then Haul to selected.
5. Repeat for the next chain links and step to apply/advance production.

The synthetic acceptance layout places farm/mill/bakery/market at (2,2), (6,2),
(10,2), (14,2), with twelve unit roads connecting (2,1) through (14,1).
It uses grain 1->2, flour 2->3 and bread 3->4 truck orders. These are authored
example definitions, not original-game economy evidence.

## Verification receipt

**VERIFIED - source/build/automated tests:** full Windows Release builds succeed.
Complete CTest passes **55/55 headless** and **56/56 client**. New
RuntimeNativeEconomyTools coverage includes fractional/negative display origins,
cell centers and half-open boundaries, maximum grids/origins, nonfinite/out-of-grid
transactional rejection, gapped/ambiguous IDs, storage-only/multiple-recipe resource
eligibility and sorted choices. A core Farm-to-Mill-to-Bakery-to-Market fixture
completes production/consumption, uses both road axes and matches 24 save/reload
continuation hashes. Client argument tests cover duplicate/native-checkpoint and
all existing incompatible native startup options without launching a valid GPU
scene in automatic CTest.

The first test catalog incorrectly omitted storage for a second recipe output;
the core correctly rejected it. The fixture gained that flour slot, and its
disjoint-resource check was adjusted to endpoints that remain disjoint. No core
validation was relaxed. Independent review found a partial-road retry/error loss
and initial camera-target overwrite; both are repaired. Final source review
found no further actionable issue in the requested paths.

**OBSERVED - owner-authorized Windows GUI:** fresh native startup shows zero
buildings at tick zero. The default 900-pixel window wraps the action row, and
the expanded window displays the same controls. The first inspection exposed
unset initial palette selections; explicit defaults were added and the rebuilt
client repeated the test. Four catalog building placements and twelve road
segments were then submitted through GUI clicks. Step 1 reports **16 applied,
0 rejected**, and four primitives appear with the road connecting their access
cells. Exact native picks select OR-1 farm, OR-2 mill, OR-3 bakery and OR-4 market.

GUI source/destination actions display grain, flour and bread respectively in the
resource palette and queue all three truck orders. The 240-tick button advances
from tick 1 to **241**, reporting all **3** applied and no rejection. The selected
market is running with **bread 1/16 and 108 consume cycles**. Three small truck
markers appear along the road. A deliberately overlapping market placement at
the farm origin queues normally, then rejects on the next single step: the HUD
retains **0 applied, 1 rejected** and the footprint-overlap explanation. At tick
**242**, four buildings remain and the market shows **109** consume cycles.

**VERIFIED - actual pipe export and process continuation:** the real
tools/economy_checkpoint.py CLI exports that GUI-controlled world to a new ignored
build artifact. It is **1,378 bytes**, tick **242**, revision **262**, hash
`5904435322412779883`. Pipe queries confirm the four exact cells, twelve roads,
and three exact resource/order endpoint tuples. The serialized native recipe
progress retains the market's **109** cycles. A fresh headless host starts at
that exact hash, and desktop/reference hashes match at each of **24** subsequent
ticks, ending at tick **266**, hash `13674903108919557831`. The helper host closes
cleanly; desktop observations then show **119** completed market cycles.

**OBSERVED input isolation, with query verification:** while a building draft is
armed, a palette opens over the status label. Clicking the world dismisses the
palette without placing underneath it; the draft remains armed. Cancel clears
it. A pipe query verifies the exact unchanged tick-266 hash and zero pending
commands. Injected Escape presses did not visibly dismiss the palette in this
automation pass, so keyboard dismissal/cancellation acceptance remains
**UNKNOWN**; the source guard and Cancel button do not establish it. The removal
button, multiple vehicle choices and storage-only hauling were not exercised in
the live GUI pass; their source/helper/core evidence remains separate.

The task-owned desktop was closed after acceptance. No screenshot file was
exported; synthetic checkpoint artifacts remain in ignored build output.

```powershell
cmake --build build/headless-host --config Release --parallel 6
cmake --build build/openrepublic-vs2022 --config Release --parallel 6
ctest --test-dir build/headless-host -C Release --output-on-failure
ctest --test-dir build/openrepublic-vs2022 -C Release --output-on-failure
python -B tools/economy_checkpoint.py --pipe OpenRepublicController --output <new.checkpoint>
```

## Remaining scope and next useful goals

This establishes a native authored sandbox and one complete GUI resource flow.
Original-game behavior/fidelity, genuine-content construction, cross-platform
graphics and large-world performance remain **UNKNOWN**. No original game process,
asset, installation or save was used or modified. The broader playable-alpha goal
remains active.

Next useful work is native order inspection/retarget/cancellation and road
enable/disable controls, followed by continuous run/pause and in-app checkpoint
file workflows. Also resolve keyboard acceptance with a visible physical/runtime
check, improve grid/access and draft-road presentation, and profile snapshot/
reconciliation cost before making larger-world performance claims. Construction
costs, supply/workforce simulation and terrain-aware connectivity remain separate
gameplay milestones.
