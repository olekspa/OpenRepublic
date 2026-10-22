# Scene look control API

`OpenRepublicSceneControl` is an engine independent C++17 library for storing and changing the map-region presentation look. It owns presentation settings only; simulation state remains authoritative elsewhere. There are no WRSR format or behavior claims in this API.

## App integration

Include `openrepublic/scene_control.h`, create one `openrepublic::scenectl::SceneControlState`, and route each complete request line to `Apply(std::string_view)`. The app may read `CurrentLook()` and `Revision()` at startup. Each frame, call `ConsumeDirty()`; when it returns true, read `CurrentLook()` and apply the values to the Wicked scene on the app/render thread. The API itself has no Wicked Engine dependency and is safe for concurrent request/read access. `ConsumeDirty()` clears a coalescing flag: several accepted edits before a frame produce one dirty notification, while the newest look and revision remain available.

Revision is a decimal string in the JSON response to avoid JSON integer precision loss in consumers. It starts at zero and increments once per accepted operation that changes at least one value. Failed operations and no-op updates do not change it. Initial constructor state should be a valid `SceneLook`; the default constructed look is valid.

## Request and response

One JSON object per call, protocol version 0:

```json
{"id":"agent-1","protocolVersion":0,"method":"look.set","params":{"exposure":1.2,"camera":{"yawDegrees":45,"distanceFactor":1.1}}}
```

Success uses the Agent API envelope:

```json
{"id":"agent-1","protocolVersion":0,"ok":true,"result":{"revision":"1","exposure":1.2}}
```

Errors use `ok:false` and `error:{code,message}`. Supported typed codes include `invalid_request`, `out_of_range`, and `unknown_field`, plus framing/parser errors such as `request_too_large`, `malformed_json`, `duplicate_json_key`, `json_too_deep`, and `json_too_complex`. Request objects accept only `id`, `protocolVersion`, `method`, and optional `params`; IDs are non-empty strings up to 128 bytes. `params` defaults to `{}`. Maximum request length is 64 KiB; nesting and parser node bounds are 32 and 4096.

## Methods

| Method | Params | Effect |
|---|---|---|
| `capabilities` | `{}` | Returns API version, supported methods, controller role, and request limit. |
| `look.get` | `{}` | Returns the full look and current revision. |
| `look.set` | partial look object | Validates all supplied fields, then applies them atomically. |
| `look.preset` | `{"name":"golden_hour"}` | Applies `golden_hour`, `noon`, `dusk`, or `overcast`. |
| `camera.set` | partial camera object | Atomically updates camera yaw, elevation, or distance. |

`look.set` fields: `sunElevationDegrees` (2..80), `sunYawOffsetDegrees` (-180..180), `sunWarmthKelvin` (2000..8000), `exposure` (0.3..3), `cloudCoverage` and `fogDensity` (0..1), `oceanEnabled`, `shadowsEnabled`, `bloomEnabled`, `ssaoEnabled`, `rtaoEnabled`, and `rayTracedReflectionsEnabled` (booleans), and nested `camera`. Camera fields are `yawDegrees` (-180..180), `elevationDegrees` (5..75), and `distanceFactor` (0.5..2.5). Unknown keys and incorrect types are errors. For an all-or-nothing update, validation is performed against a private copy before state changes.

`sunWarmthKelvin` represents a deterministic OpenRepublic display parameter; mapping it to a Wicked Engine light color is the app adapter's responsibility. `LookAtTimeOfDay(hours)` returns a default look with sun values from this deterministic curve: daylight is 06:00–18:00, elevation is `2 + 68*sin(pi*fraction)` degrees, yaw moves linearly from -90 to +90 degrees, and warmth is `2600 + 3400*sin(pi*fraction)` kelvin. Outside daylight, elevation stays at 2 degrees, yaw holds at the nearest endpoint, and warmth is 2600 K. Hours outside 0..24 or non-finite values return a default look. This is an OpenRepublic presentation curve, not a measured game model.

## Lua integration

No separate Lua adapter is included. A sandbox callback can construct the same bounded JSON request and call `SceneControlState::Apply`, then decode the response. It should expose only the same methods/fields and respect the app's frame-boundary application policy. Lua must not directly mutate Wicked objects or simulation state through this interface.

## Dependency and evidence boundary

The JSON adapter uses the MIT-licensed nlohmann/json header already pinned in `third_party/WickedEngine/Editor`, matching the economy host. CMake omits the library and its tests when that header is unavailable. Source/build/test results for this library establish OpenRepublic API behavior only; they do not establish original-game visual fidelity, hardware acceptance, or Lua runtime integration.

## App integration status (2026-10-22, VERIFIED by a live run)
`OpenRepublic.exe ... --modern --scene-pipe-name or-scene` serves this protocol on `\.\pipe\or-scene` (JSONL, one request per line, requests need `params` as a nested object). Accepted changes are applied on the next frame: sun direction/colour, shadows, ocean, cloud coverage, fog density, exposure, bloom, AO mode, ray-traced reflections and the orbit camera around the region centre (`src/app/OpenRepublicApplication.cpp`: `UpdateSceneControl`, `ApplySceneLook`). Checked by sending `look.preset dusk`, `camera.set` and `look.set sunElevationDegrees 4` to a running app and watching the scene change (a low, warm sun and a new camera angle). Limits: the pipe has no authentication (same trusted-local model as the economy pipe); the camera pose is overwritten on every applied change, so manual fly-camera movement is reset by a look change; `sunWarmthKelvin` is not mapped to colour (colour follows sun elevation).
