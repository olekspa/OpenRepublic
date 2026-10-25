# Play now

## Quick start

1. Build the Windows `OpenRepublic` client with the pinned Wicked Engine submodule initialized.
2. From the repository root, run `./tools/play.ps1 coal` (or `sites` or `town`).
3. The launcher changes to the executable directory so the engine can find its `Content` folder.
4. You can also run `OpenRepublic.exe --play coal` directly from that directory.
5. Use the in-game **Game** panel to save or load OpenRepublic checkpoints.

`--play coal` composes the native economy, authored coal catalog/demo, coal objectives,
synthetic terrain, and ambient traffic. `sites` selects the construction catalog and
objectives. `town` selects the generic sandbox catalog and growth objectives. Native
HUD and save controls are part of the native economy desktop UI. The option resolver
preserves unrelated arguments and leaves every command line without `--play`
unchanged. A preset rejects additional flags that it owns to avoid duplicate/conflicting
mode selection.

## Controls

Use **Run / Pause** and the tick-step controls to advance the simulation. Select
buildings and use the building chooser to place them; **Build road** connects access
cells. The Dispatch panel sends depot trucks between buildings. The objectives panel
shows the current preset's checklist and hints. **Frame world** recenters the camera;
**Follow truck** tracks the selected dispatch truck. Controls reflow with window size.

## What to expect

These are small, deterministic OpenRepublic synthetic sandboxes. Coal starts with a
border post, depot, storage and heating plant. Sites starts with construction material
producers; place sites, deliver materials, then allow work to advance. Town starts with
the generic production sandbox and presents the authored growth checklist, although
its objective types are not currently in that catalog. Ambient traffic is presentation
activity and does not carry simulation cargo. Terrain, prices, recipes, citizen rules,
vehicle timing and objectives are project-authored.

## Known gaps

- `town` currently uses the generic native catalog, which has neither the
  `residential_building` type nor the synthetic citizen needs/migration rule. Its growth
  objectives are not reachable from the desktop preset yet.
- Headless scenario fixtures exercise related but not identical worlds. The smoke
  harness reports only objective fields exposed by `scenario_score.py`; unmeasured
  objective steps are called out rather than inferred.
- Desktop HUD, saves, controls, graphics, and the complete preset startup path have not
  been accepted in a live GUI session for this branch.
- All synthetic mechanics and all WRSR gameplay/visual parity remain **UNKNOWN** with
  respect to the original game.

See [Playing OpenRepublic](PLAYING.md), [desktop HUD](DESKTOP_HUD.md),
[desktop saves](DESKTOP_SAVES.md), and [desktop terrain](DESKTOP_TERRAIN.md).
