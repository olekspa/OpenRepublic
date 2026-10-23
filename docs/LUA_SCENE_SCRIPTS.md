# Lua scene scripts

`OpenRepublicScripting` provides `SceneScript`, a persistent sandbox for authored presentation sequences. It communicates only with `SceneControlState`; it has no Wicked Engine or simulation access. This is an OpenRepublic presentation API and makes no claims about WRSR script formats or behavior.

## Script contract

A script defines `function on_frame(dt)`. The host loads the source once and calls `SceneScript::Tick(dt, scene)` on its frame/update thread. `or.time` is elapsed seconds since the script was loaded, advanced by each accepted finite non-negative `dt` immediately before `on_frame` runs. The script may retain ordinary Lua locals between frames.

The empty `_ENV` exposes only the `or` table:

Lua reserves `or` as a keyword, so scripts bind the API table from the environment with `_ENV["or"]`:

```lua
local api = _ENV["or"]
function on_frame(dt)
    local look = api.look.get()
    api.look.set { exposure = look.exposure, sunElevationDegrees = 35 }
    api.look.preset("dusk")
    api.camera.set { yawDegrees = 30, elevationDegrees = 25, distanceFactor = 1.1 }
    api.log("showcase frame")
end
```

The environment has exactly one global binding, the API table under key `"or"`. Its `look.get()` returns the current look table (`sunElevationDegrees`, `sunYawOffsetDegrees`, `sunWarmthKelvin`, `exposure`, `cloudCoverage`, `fogDensity`, feature booleans, and nested `camera`). Setters accept partial objects and use the exact `SceneControlState` field names and validation limits. `look.preset` accepts `golden_hour`, `noon`, `dusk`, and `overcast`. `camera.set` accepts camera fields. Invalid types, unknown fields, and out-of-range values raise Lua errors containing the typed scene API message; any runtime failure disables the script and is retained in `LastError()`.

`api.log(string)` silently drops messages once either 64 messages or the aggregate 16 KiB bound is reached; individual messages are limited to 256 bytes. The host retrieves and clears collected messages with `TakeLogs()`.

## Limits and host use

Source is limited to 64 KiB; the Lua allocator is capped at 8 MiB. There are no standard libraries, including `os`, `io`, `require`, `debug`, or `math`. Initialization has a bounded budget of approximately 10 million Lua VM instructions; each frame has a separate budget of approximately 100,000 instructions. Parameter table conversion is also bounded to 32 levels and 4096 nodes. `Tick` returns false after a runtime/instruction error and permanently disables that script instance until it is loaded again. A negative or non-finite `dt` is rejected without running the callback.

```cpp
openrepublic::scripting::SceneScript script;
std::string error;
if (!script.LoadFile("data/scripts/day_cycle.lua", &error)) { /* report error */ }
// On the app's update/frame thread:
if (!script.Tick(dt, sceneControl)) { /* report script.LastError() */ }
```

The app remains responsible for applying `CurrentLook()` at its chosen frame boundary. `SceneScript` itself does not touch rendered objects, launch a game, or mutate economy state.

Examples: [day_cycle.lua](../data/scripts/day_cycle.lua) and [orbit_camera.lua](../data/scripts/orbit_camera.lua).

## App integration (2026-10-23, VERIFIED by a live run)
`OpenRepublic.exe ... --modern --scene-script data/scripts/day_cycle.lua` loads the script in the sandbox (log: "Scene script loaded") and calls `on_frame(dt)` every frame; look/camera changes go through the same `SceneControlState` as the pipe, so `--scene-pipe-name` and `--scene-script` can be combined. A script error stops the script (logged) without affecting the app. Checked: the example day-cycle script loaded and ran for 50 s with no stop message; the sun motion itself was not inspected frame by frame.
