# Desktop autoplay and UI smoke scripts

Desktop autoplay is an opt-in convenience for the independent native economy.
It reuses the same `EconomyApi` run/step path as the UI. The coal preset keeps
its authored coal logistics runner. Other objective sets use a deterministic
planner that validates building footprints and access cells, extends roads,
follows catalog recipe inputs, trades declared resources, dispatches idle depot
vehicles, and connects declared utility producers to consumers. The planner
follows the active objective set and catalog; it does not infer WRSR rules.
All behavior, catalogs, and outcomes described here are OpenRepublic synthetic
behavior. WRSR parity remains **UNKNOWN**.

```powershell
OpenRepublic.exe --play coal --autoplay --autoplay-speed 100
```

`--autoplay-speed N` sets the maximum economy ticks submitted per app update
(1 through 10,000; default 1). It changes throughput only.

## UI scripts

`--ui-script <file.json>` runs a bounded ordered list of actions at app update
frames. The parser accepts up to 1 MiB and 4096 actions, requires nondecreasing
frame numbers, and rejects unknown actions or properties.

Actions:

- `button`: invoke a supported button label. Supported labels are `Tools`,
  `Hide tools`, `Game`, `Details`, `Compact`, `Overview`, `Hide overview`,
  `Run`, `Pause`, and `Auto-run logistics`.
- `screenshot`: save the current backbuffer to `path`.
- `select`: select an exact `entry` from the combo widget named by `label`.
  Example widget: `NativeEconomyBuildingType`.
- `place_building`: place a catalog `type` at integer cell `x`,`z`.
- `build_road`: build an axis-aligned path from `x`,`z` to `x2`,`z2`.
- `click_cell`: select a building at its origin/access cell, or confirm the
  current placement tool at that cell.
- `wait_ticks`: step the simulation by positive `ticks`.
- `assert_text`: require a named `widget` text to contain `text`. A failed
  assertion logs the widget and expected text, then exits with code 3.

```json
{
  "actions": [
    { "frame": 1, "action": "select", "label": "NativeEconomyBuildingType", "entry": "home" },
    { "frame": 2, "action": "place_building", "type": "home", "x": 18, "z": 18 },
    { "frame": 3, "action": "build_road", "x": 18, "z": 17, "x2": 19, "z2": 17 },
    { "frame": 4, "action": "wait_ticks", "ticks": 4 },
    { "frame": 5, "action": "click_cell", "x": 18, "z": 18 },
    { "frame": 6, "action": "screenshot", "path": "end.png" },
    { "frame": 7, "action": "assert_text", "widget": "NativeObjectivesChecklist", "text": "complete" }
  ]
}
```

Packaged preset scripts live in `data/ui_scripts/`; campaign scripts cover all
five `republic_rising` missions. Campaign runs accept `--campaign`, `--autoplay`,
`--autoplay-speed`, and `--ui-script`; the first incomplete mission is selected
from its progress file.

## Staged smoke runner

`tools/ui_smoke.ps1` copies a supplied Release folder to
`C:\or-wt-builds\planner-run`, replaces the staged executable with the build to
check, stages the current scripts/campaign definitions, and runs the UI action
smoke, all three presets, and all five campaign missions. It uses isolated
`LOCALAPPDATA` folders for each run, asserts the final objective checklist in
each script, and reports each exit code and screenshot set.

```powershell
.\tools\ui_smoke.ps1 -ReleaseFolder C:\or-wt-builds\main-release `
  -BuiltExe build\client\src\app\Release\OpenRepublic.exe
```

Inspect the resulting PNGs in the staged directory after running the smoke
suite. Build success, script assertions, and these synthetic outcomes do not
establish original-game behavior; WRSR parity remains **UNKNOWN**.
