# Native desktop synthetic terrain

`--native-terrain` adds an opt-in procedural terrain presentation to the native economy scene. Start a fresh sandbox with:

```text
OpenRepublic.exe --native-economy --native-terrain
```

It can also be combined with `--native-coal-demo`, or used with `--economy-checkpoint`. It requires one of those native economy scene modes. Without the flag, the existing flat ground plane and native scene geometry remain in use.

## OpenRepublic synthetic rules

- The deterministic default seed is `20261025`. Five smoothed value-noise octaves plus a second detail field produce rolling terrain in the native display coordinate system (4 display units per economy cell).
- A level starting district occupies a radius of 18 display units, followed by a smooth transition to low hills. A fixed oval basin centered at `(38, -27)` has a renderer water surface at height `0`.
- Terrain shading uses the existing showcase `ShadeShowcaseTerrain()` and procedural grass detail. The lake uses a native synthetic water material. The app's existing sky/weather initialization is shared by both modes.
- Buildings are rendered on a horizontal pad at the sampled footprint-center height. Roads sample the terrain at their segment center; vehicles follow the sampled terrain while moving.
- Native building and road placement checks the sampled footprint gradient. The OpenRepublic presentation threshold is `0.22` rise/run; footprints crossing the lake are also rejected. The construction preview turns red when this policy rejects the footprint.
- This terrain is presentation-only. It does not affect economy state, cell coordinates, pathfinding, persistence, or deterministic world hashes. The slope threshold is not used by the authoritative economy API or remote controllers.

## Evidence boundary

The seed, noise, height units, lake location/level, visual scale, and `0.22` placement limit are OpenRepublic-authored synthetic policy. **WRSR terrain, water, slope/buildability behavior, units, and visual parity remain UNKNOWN.** Existing map heightmap and water evidence is build-scoped and does not validate these synthetic rules. No original-game assets are used by this mode.

Source and headless unit coverage verifies deterministic heights, seed variation, a level build area, finite slope samples, invalid-footprint rejection, and lake exclusion. Rendering and interaction still require graphical acceptance; no original-game comparison is implied.
