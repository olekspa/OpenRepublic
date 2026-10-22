# Native in-app checkpoint saving - 22 October 2026

Fresh native and restored-checkpoint desktops now have Save checkpoint in the
wrapped top toolbar. The action pauses desktop Run, resets clock debt, queries
the authoritative API and exports a frozen catalog-bound checkpoint at its exact
revision. The UI does not rebuild serialization or mutate the world to save.

The Windows Save dialog defaults to republic-<tick>.checkpoint and permits a
user-chosen new path. Cancel leaves the desktop paused. Existing destinations
are never replaced: a validated payload is written to an exclusively created
sibling temporary, flushed, then published without replacement. Errors report
failure rather than truncating a destination. RestoreCheckpoint validates data
before filesystem changes. This helper accepts native Unicode filesystem paths.

The dialog is synchronous on the application thread. Native simulation and
app-owned pipe request pumping are suspended while it is open; a controller can
time out during that modal interval. The exported snapshot precedes the dialog.
No in-place load or world/API replacement was introduced; restore remains through
--economy-checkpoint <file>. Cross-platform file publication has source support
but only Windows was built/tested. No power-loss fault injection was performed.

VERIFIED: final Windows Release desktop and helper build succeed. RuntimeCheckpointFile
passes in headless and client trees. Client focused CTest passes3/3 (file writer,
checkpoint API and desktop arguments). Tests prove exact payload bytes/restored
hash, refusal of a different valid checkpoint at an existing path, invalid data
leaving no output, missing-parent failure, Unicode filenames and temporary cleanup.
A preexisting temporary-path collision remains untouched and causes safe failure;
the helper does not retry that collision automatically.

OBSERVED: default-window Save checkpoint button opens the native dialog with the
expected filename/filter. End-to-end GUI file publication remains UNKNOWN:
automation returned only the disabled owner window, and filename focus/set-value
recovery did not reliably address the modal field. No text was typed into an
unverified field and no GUI-save success is claimed. The task-owned test client
was closed by its exact launch command identity; no original game input was used.

Source/UI and file-publication helpers are implemented, not a completed in-app
save/load menu milestone. Next: verify dialog Save/Cancel/duplicate-path behavior,
then provide a load workflow that safely rebinds sessions/handles/render identity.
Review and commit the accumulated native integration after combined acceptance.
The global goal remains incomplete; changes remain local/uncommitted.
