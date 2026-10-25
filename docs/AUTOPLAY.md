# Desktop autoplay and UI smoke scripts

OpenRepublic's desktop autoplay is an opt-in convenience for observing the
independent native economy. It reuses the existing `EconomyApi` run/step path.
For the coal preset it enables the authored baseline coal logistics planner,
which imports coal when funded and dispatches the existing trucks. Other
objective sets use an additional bounded planner for unmet building-count,
housing-count, and construction-site-count goals: it proposes one deterministic
free cell at a time and submits placement through `EconomyApi`. It does not yet
plan resource chains, trade, truck dispatch, or utility topology for arbitrary
objectives, so some objectives can remain incomplete. All rules,
catalogs, planner decisions and outcomes are OpenRepublic synthetic behavior;
WRSR parity remains **UNKNOWN**.

```powershell
OpenRepublic.exe --play coal --autoplay --autoplay-speed 100
```

`--autoplay-speed N` sets the maximum number of economy ticks submitted per
app update (1 through 10,000; default 1). The speed changes throughput only; it
does not add simulation rules. Planning is limited to the objective types above;
it is not a general campaign solver. Check objective completion against the
visible checklist or a scenario-specific result.

`--ui-script <file.json>` runs a bounded ordered list of actions at app update
frames. The first supported actions are button-label invocation and PNG
screenshots. Button labels currently supported by the script dispatcher are
`Tools`, `Hide tools`, `Game`, `Details`, `Compact`, `Overview`, `Hide overview`,
`Run`, `Pause`, and `Auto-run logistics`. Button actions call the same app
handlers as the corresponding GUI callbacks. Scripts are limited to 1 MiB and
4096 actions, and frames must be nondecreasing.

```json
{
  "actions": [
    { "frame": 1, "action": "button", "label": "Overview" },
    { "frame": 2, "action": "screenshot", "path": "start.png" },
    { "frame": 3, "action": "button", "label": "Run" },
    { "frame": 180, "action": "screenshot", "path": "middle.png" }
  ]
}
```

Packaged examples live in `data/ui_scripts/`. For example, run the coal script
with:

```powershell
OpenRepublic.exe --play coal --autoplay --autoplay-speed 10 --ui-script data/ui_scripts/coal_smoke.json
```

The coal script captures at startup, after 120 updates, and after 242 updates
to allow a 10-tick per update run to reach the 2400-tick milestone before its
final capture. Run the same command
with `sites` or `town` and the matching script for those presets. Script screenshot paths
are relative to the app working directory (the executable/resource directory).
The app exits after its final
script action. `--screenshot <png> [--screenshot-after-frames N]` is a shortcut
for a one-shot capture, defaulting to the first update frame; the current
backbuffer is written to the requested PNG path.

The current parser deliberately rejects unknown actions/properties. Dropdown
selection, building placement, road drawing, click-on-details controls,
objective assertions, and a staged smoke runner are not implemented yet.
Startup, build and script-parser checks do not by themselves prove WRSR
behavior. Visual acceptance requires reviewing the generated PNGs; traffic
movement and objective progress are synthetic observations only.

