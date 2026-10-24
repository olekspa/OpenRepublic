# Native desktop traffic and production effects

This document describes OpenRepublic presentation behavior for the native economy
scene. It is not a statement about Workers & Resources: Soviet Republic behavior.

## Vehicle presentation

Each live native `VehicleSnapshot` ID owns a procedural truck body, four separate
wheel entities, and a separate cargo block. The truck body and wheels are
OpenRepublic-authored geometry from `src/procgeo/vehicles.cpp`. The cargo block
is visible when the snapshot cargo amount is nonzero and hidden when it is zero;
its size does not encode cargo quantity or resource type. Idle, waiting, and
no-route vehicles receive a subdued body tint.

The native economy reports cell-quantized positions. `InterpolateNativeVehicleMotion`
interpolates between the prior and current cell centers using the render alpha from
the four-tick-per-second native clock. It retains the last heading while stationary
and advances wheel rotation in proportion to interpolated display distance. A move
of more than one cell is treated as a teleport/respawn and snaps directly to the
current cell without interpolating or rotating the wheels. Four display units per
cell and the truck's scale are scene presentation choices. Vehicle nodes are
removed when their snapshot IDs disappear.

Current native snapshots do not include route-leg progress, so this view uses the
available cell snapshots only. It does not synthesize a path between cells or claim
that a rendered truck follows a particular road. Vehicle Y is placed at the native
road surface plane. Camera follow-selected-vehicle is not currently implemented.

## Production smoke policy

At most eight low-count smoke emitters are active at once. An emitter is shown
while one or more `BuildingSnapshot::recipes` entries have `remainingTicks > 0` and
the catalog building name contains `heating_plant`, `heatingplant`, or `bakery`.
These exact tokens are an OpenRepublic presentation policy for heating-plant and
bakery-like producers. They are not recovered classifications or claims about
original-game emissions. Other catalog names receive no production emitter.

## Evidence boundary

`VERIFIED` source and headless tests cover interpolation math, alpha clamping,
stationary yaw, teleport snapping, and wheel-distance calculation. A successful
desktop build establishes compilation only. `OBSERVED` desktop appearance,
animation pacing, smoke quality, camera behavior, graphics performance, and
hardware acceptance require a separately authorized app run; this task does not
launch the app. WRSR visual fidelity and gameplay parity remain `UNKNOWN`.
