# Native desktop traffic polish

This guide describes opt-in OpenRepublic presentation policy selected with
`--native-traffic-polish`. It does not change economy snapshots, movement,
commands, persistence, or state hashes. Road widths, lanes, speed limits, route
progress, and intersection rules in Workers & Resources: Soviet Republic remain
**UNKNOWN**; the behavior here is synthetic and is not a parity claim.

The option applies to `--native-economy` and `--economy-checkpoint` scenes. With
the option omitted, the existing cell-center interpolation and retained heading
remain in use. The polished mode uses right-hand lateral placement and a
rate-limited heading as view policies. Its following and yielding decisions are
calculated from the current presentation snapshot only and never feed back into
the simulation. The runtime also provides a tested rounded-polyline sampler,
but current native vehicle snapshots have no route progress, so the desktop
scene cannot yet use that sampler on real vehicle motion without inventing a
path.

## Evidence and limits

**VERIFIED** source and headless tests cover the pure runtime calculations.
Source wiring adds the opt-in switch to native scene startup. This does not show that vehicles follow authoritative
road routes: current vehicle snapshots are cell-quantized and do not expose
route-leg progress. Any path assembled for display is therefore an
OpenRepublic presentation approximation. Road class, width, lanes, speeds,
intersection priority, and WRSR appearance remain **UNKNOWN**. The desktop
application was not launched for graphical acceptance.
