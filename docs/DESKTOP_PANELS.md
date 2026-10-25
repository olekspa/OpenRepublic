# Native desktop panel layout

`src/runtime/panel_layout.*` provides the DPI-scaled, headless-testable placement policy used by native economy desktop panels. Requests carry a preferred rectangle, dock, visibility, priority and scrollable-list hint. The solver lays out higher-priority panels first, clamps rectangles to the logical viewport, moves collisions through the selected dock and hides a lower-priority panel when no free placement is available. Long text areas keep Wicked Engine's widget scroll bar enabled where supported.

The native HUD uses the policy for its status, stored-resource, overview-map control and event-list rectangles. Native economy Game and Overview controls request separate right and left columns. Save entries use readable slot labels, local save date and checkpoint tick when metadata is available; legacy internal widget identifiers are presented as `Save Slot N`.

The policy and collision/clipping behavior are covered at 1280x720, 1920x1080 and 2560x1440, plus a 2x DPI case. These are OpenRepublic UI rules, not WRSR behavior. Graphical acceptance requires the staged desktop captures; no WRSR parity is implied.

**Status:** the placement solver and its unit tests are VERIFIED as OpenRepublic behavior. In the staged Release capture at 1580x948, the Overview panel and resource HUD are readable and save entries use human labels, but Game controls overlap the HUD and obscure part of the save list. Desktop readability acceptance has therefore FAILED for this capture. The requested 1280x720, 1920x1080 and 2560x1440 graphical checks have not been repeated on the integrated build. WRSR panel behavior and parity remain UNKNOWN.
