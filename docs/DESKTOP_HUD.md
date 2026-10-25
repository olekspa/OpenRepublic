# Native desktop HUD

The native economy desktop mode now keeps a compact HUD visible while playing.
It presents treasury, simulation tick, optional declared calendar/season and
citizen totals, running/paused state, and the selected simulation speed. The
resource ticker totals stored non-utility inventory from the current snapshot.
When the economy has last-tick utility diagnostics, it also shows generated,
requested, consumed, and unused quantities for each measured component.

The event log retains the latest eight transitions recognized from successive
world snapshots: construction completion, a newly observed input/utility
shortage, a haul whose cargo reaches zero, treasury changes, and population
changes. The economy changes treasury through border-trade commands, so those
transitions are shown as border trade. A population increase is shown as
immigration only when the synthetic citizens catalog rule is enabled; decreases
stay generic because removing housing can also reduce the total. These labels
describe OpenRepublic behavior and do not imply WRSR parity. Loading a world or
rewinding the tick clears the log history. These UI summaries do not change
simulation state or hashes.

Hovering a native building shows its declared name, ID, enabled state, any
construction progress, stall code, and stored inventory/capacity. The Overview
map button frames the current world and switches to a top-down camera view; a
second click restores the normal framed camera. This is a camera view, not a
separate renderer minimap.

Only fields exposed by the OpenRepublic economy are displayed. Utility totals
are last-tick diagnostics and may be unavailable before a utility phase or
after loading. Synthetic economy behavior remains OpenRepublic-authored;
WRSR parity is **UNKNOWN**. No original-game comparison or GUI acceptance is
claimed by source/build verification.
