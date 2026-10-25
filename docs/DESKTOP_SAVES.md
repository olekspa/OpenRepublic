# Native desktop save slots

The native OpenRepublic economy screen has a **Game** panel with New sandbox,
New coal demo, and New construction game actions, named save slots, and load
slots. Starting a new catalog or loading a slot starts a fresh OpenRepublic
process with the selected native mode/checkpoint; it does not rebind widgets or
controllers to a different catalog in place. Unsaved changes prompt before the
current session closes.

Named slots live under `%LOCALAPPDATA%/OpenRepublic/saves` on Windows. Names are
restricted to ASCII letters, digits, `-`, and `_`; the file uses `.orsave`. The
Save dialog is rooted in that directory, and paths outside it are rejected.
Saving a name that already exists uses an atomic replace after the common dialog
confirms overwrite. Original checkpoint files using `OPENREPUBLIC_ECONOMY_CHECKPOINT
1` remain loadable directly.

## Slot envelope and listing

New slot files wrap the unchanged native checkpoint bytes in a bounded
`OPENREPUBLIC_SAVE_SLOT 1` envelope containing the Unix save time, simulation
tick, building count, and exact payload length. The envelope is additive and
versioned; the old checkpoint format remains the payload and legacy raw files
remain loadable. Listing reads the bounded envelope and payload length only. It
does not parse the catalog or restore the world until a slot is selected. A
truncated envelope is omitted; a slot with a valid envelope but damaged world
payload can appear in the list and is rejected when selected for loading.

The load panel displays eight recent slots per page, including date, tick, and
building count. Previous/More controls page through the remaining slots.

## Autosave and evidence boundary

Autosaves run when simulation time crosses each 10-minute boundary (2,400
ticks at the native clock's four ticks per second), whether time advances with
Run or an explicit step. Three `autosave-1` through
`autosave-3` files rotate with atomic replacement. Wall-clock time while paused
without simulation ticks does not trigger an autosave. A successful named save
or autosave clears the unsaved-change marker at that world revision.

**VERIFIED (source and headless runtime tests):** safe slot names, atomic named
slot writes, exact metadata, legacy raw checkpoint load, truncated/corrupt
rejection, and three-slot autosave rotation. **UNKNOWN:** interactive Windows
dialog behavior, restart handoff and quit confirmation at runtime. These files
contain OpenRepublic native state; they do not read, write, or interpret WRSR
saves. WRSR gameplay parity is UNKNOWN for the synthetic catalogs and rules.
