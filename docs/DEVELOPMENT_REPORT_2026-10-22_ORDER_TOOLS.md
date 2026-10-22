# Native haul-order inspection and repair - 22 October 2026

The native desktop now exposes the existing authoritative haul-order repair API.
Fresh native worlds and checkpoint restores share an order dropdown and Retarget
to selected / Cancel haul buttons. No simulation, serialization or API contract
changed. Existing dirty work is preserved.

The list shows exact order identity, endpoint building names/IDs, resource, cargo
and state. HUD details add exact truck ID/cell and named failure reason. Selection
follows the order ID across list changes; removal invalidates selection instead
of substituting a different order. An initial populated list selects its first
entry. Existing IDs get label updates in place so running cargo/state changes do
not destroy an open dropdown. Structural changes rebuild it. The new row wraps
with viewport width and participates in active/pointer input guards.

Choose the haul, select a new destination building in the scene and click Retarget
to selected. Cancel haul needs only the chosen current order. Both actions validate
current selection, stale-scene state and human queue capacity, then submit using
the authoritative snapshot revision and exact native IDs. Queue admission remains
separate from validation on stepping. Core rules preserve cargo during retarget,
reject cancellation with cargo in transit, and validate endpoint storage/route
semantics. No UI heuristic overrides those rules. Run can apply commands on its
next tick; otherwise use a manual step and inspect the applied/rejected notice.

VERIFIED: final OpenRepublic Windows Release build succeeds. Focused client CTest
passes7/7: EconomyHaulRetarget, RuntimeNativeEconomyLayout/Tools/Clock,
OpenRepublicEconomyApiTests, EconomyHaulRetargetProtocol and desktop checkpoint
arguments. These cover existing core/API conservation and validation behavior;
they do not automate the new graphical widgets. No redundant full suite was run.

UNKNOWN: live dropdown usability, GUI retarget/cancellation and selected-order
invalidation have not yet been exercised in this increment. Runtime acceptance
is the next action, preferably on the existing authored chain checkpoint with
pipe queries confirming exact order/truck/cargo state. No original game process,
assets or saves were used. Changes remain uncommitted pending combined integration
acceptance. The global game goal remains incomplete.

Review: no confirmed identity or clickthrough defect. The reviewer flagged stale-scene rejection for data-only order commands; retained the existing conservative native-write gate because destination selection comes from the scene. The guard notice now refers to native controls generally. Source review does not establish live acceptance.

## Live retarget follow-up

OBSERVED: order dropdown and detailed truck/cargo/failure status rendered in the
restored chain. Synthetic setup through the actual app-owned pipe added farmOR5
at(18,2) and four connecting roads, then advanced to247 with haul1/truck1 carrying
three grain. This extension was API-authored, not GUI construction evidence.
GUI selection picked OR5; Retarget to selected queued haul1; Step1 reported
one applied and zero rejected. Actual pipe queries at248 verify destination5,
order1, truck1 and cargo3 unchanged; hash2357299413069524112.

Cancellation, removed-order selection invalidation and open-menu usability while
running remain UNKNOWN live. A default-window usability issue was observed:
top-docked status obscured the centered small scene. The pending correction is a
bottom-docked compact native status with expandable details. The test client was
closed before relinking. No original inputs were used.

## Default-window layout correction

The rebuilt native client now defaults to a bottom-docked compact status, with
latest command feedback first and tick/world counts plus selected building status.
Details/Compact toggles up to230 pixels of full scrollable status; compact caps
at80. The button participates in wrapped top controls. Other presentation modes
retain their existing status layout.

VERIFIED: Windows Release app rebuild passed. OBSERVED on a fresh restore in the
default approximately900x540 logical client: world/roads/trucks remain visible
between toolbar and compact panel; clicking the visible mill selects OR2 and
shows output_full. Details expands to expose haul1/truck1/cargo4 and mill grain4/8,
flour8/8 and126 grind cycles. Test client closed. Very small windows and alternate
DPI layouts remain untested. No original-game comparison or performance claim.
