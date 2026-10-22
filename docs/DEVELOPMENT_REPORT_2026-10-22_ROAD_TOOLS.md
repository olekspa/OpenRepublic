# Native road controls - 22 October 2026

Native fresh/checkpoint desktops now expose an exact-ID road selector and Toggle
road button in the existing resource/vehicle row. Entries show endpoints and
current enabled state. Stable IDs receive in-place label updates; list changes
preserve the selected ID and disappearance clears it. No road picking or original
map coordinate inference was introduced. Native GUI input guards cover both
widgets and the row wraps with the existing layout helper.

The app rechecks the chosen road against the authoritative snapshot and submits
SetRoadEnabled with that revision, exact ID and inverse current enabled state.
The existing stale-scene and queue-cap guards apply. Admission is not execution;
step or Run applies the command and reports terminal feedback. Repeated clicks
before stepping request the same inverse of the current authoritative state;
they do not cancel each other's pending commands. Rendering already reflects
enabled/disabled roads through native scene reconciliation.

VERIFIED: final Windows Release desktop build succeeds; focused client CTest
passes5/5 (native layout/tools/clock, EconomyApi, desktop startup arguments).
These existing tests are regression evidence, not new GUI automation.

OBSERVED: restored the authored GUI-built chain at tick242 in the default window.
Road1 showed(3,1)->(4,1) enabled. GUI Toggle road queued disable; Step1 reported
one applied/zero rejected, selector showed disabled and haul1 showed no route.
Actual pipe query at243 confirms only road1 disabled, hash731174226860829370.
GUI Toggle road/Step1 re-enabled it, again one applied/zero rejected. Haul1/truck1
resumed toward its source. Query at244 confirms all12 roads enabled, same3 orders
and truck1 at(5,1), hash3379502173887916597. A fresh headless host restored the
same checkpoint and replayed disable/step/enable/step; final hash matched exactly.
The failure field can retain no_route on the first resumed moving snapshot;
this is existing core state and was not rewritten by presentation.

Task-owned client/reference host closed. No original game, proprietary input or
installation/save changes. Original-game behavior, large-map list usability and
removed-road selection have no new acceptance claim. Changes remain local and
uncommitted with prior work preserved; the global goal remains incomplete.

Next: finish remaining order-control acceptance and in-app checkpoint workflow,
then integrate explicit diagnostics/controls with supported authentic world
presentation without inventing map construction transforms. Review and commit
the accumulated native integration as a coherent source milestone.
