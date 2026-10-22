# Native continuous Run/Pause - 22 October 2026

The next selected integration removes repeated step clicks from the authored native
world while preserving the shared authoritative C++ EconomyApi. This is a desktop
pacing policy, not an original-game timing formula or a second simulation.

The fresh `--native-economy` and restored `--economy-checkpoint` toolbars now include
Run/Pause. Startup remains paused. Run requests one tick at a time, targeting four
ticks per second. A pure NativeEconomyClock accumulates fractional elapsed time,
clamps each frame to one quarter second and emits at most one tick per frame.
Long suspension debt is discarded, so slow rendering can slow simulated time.
NaN, infinite, zero and negative deltas do not contaminate the accumulator.

Manual one/24/240-tick controls pause continuous running and clear its remainder
before advancing. Run refuses an unavailable API or stale scene, and pauses on
step failure or later stale presentation. Step success comes from the typed API
result rather than HUD text. Automatic ticks preserve command outcome feedback.
The existing trusted-local pipe still addresses the same world; independent pipe
clients may explicitly step while desktop Run is paused. Paused is a desktop
pacing state and is not persisted in checkpoints.

VERIFIED: focused clock tests pass in headless and client Release builds, covering
fractional timing, reset, invalid input, long suspension and sustained cadence.
Client focused CTest passes 4/4: native layout, native tools, native clock and
desktop checkpoint arguments. Full-suite 55/55 and56/56 counts in the prior
construction report are historical; no new full-suite run is claimed here.

UNKNOWN: live GUI Run/Pause cadence, manual-step interaction, stale-scene recovery
and window layout have not yet been exercised for this increment. Source/build
acceptance does not prove those runtime behaviors. No original game process or
proprietary input was used. Prior dirty changes are preserved and this integration
remains uncommitted pending runtime acceptance and review of the combined work.

Next: live acceptance on the existing GUI-built resource-chain checkpoint, then
native order inspection/retarget/cancellation, road controls and in-app checkpoint
workflows. The global game goal remains incomplete.

VERIFIED: final desktop OpenRepublic Release target build passes after the feedback fix. Independent source review found no further actionable defect; no GUI review was performed.

## Live acceptance follow-up

OBSERVED: restored the existing GUI-built four-building/three-haul chain at tick242
with the new Run/Pause toolbar wrapping within the default 900-pixel window. Run
changes to Pause and advances the displayed production world and truck markers.
VERIFIED by the actual named-pipe world query: exactly eight ticks elapsed from
288 to296 over a two-second interval. Clicking Pause stopped at tick320, hash
369290647084794390; tick/hash remained unchanged across another two seconds.

OBSERVED: resumed Run, then clicked Step24. The button reverted to Run and the
HUD reported a 24-tick advance. VERIFIED: the resulting tick381/hash
6372372613752748602 remained stable for two seconds. A fresh headless host loaded
the original tick242 checkpoint and advanced139 ticks; its hash matched exactly.
This verifies same-world deterministic evolution across the mixed GUI run/manual
step sequence. Task-owned client and reference host closed cleanly.

The earlier UNKNOWN paragraph describes pre-acceptance status. Run/Pause, default
window layout and the 24-tick manual-step pause now have live evidence. Stale-scene
recovery, API-error auto-pause and one/240-tick interactions remain source/test
evidence only. No original-game input or performance/graphics fidelity comparison
was used. Next: native haul-order inspection/retarget/cancel and road controls.
