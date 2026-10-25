# Native vehicle presentation

This is an OpenRepublic-authored presentation layer. It does not reproduce
original vehicle assets, dimensions, handling, lights, smoke, or road rules;
WRSR visual and gameplay parity remain **UNKNOWN**.

## Model selection policy

The native scene first maps the catalog vehicle `name` to a procedural kind by
case-insensitive substring, in this order: rail-only or `locomotive`/`train`
(wagon when the name contains `wagon`, otherwise locomotive), `bus`/`coach`,
`car`/`passenger`, `tanker`/`cistern`, `dump`/`tipper`, `flatbed`/`trailer`,
`van`/`box`, then the generic box truck. For non-rail truck kinds with an
active haul order, the order's resource name selects the cargo presentation:
`fuel`, `oil`, `water`, `milk`, or `alcohol` selects tanker; `coal`, `ore`,
`gravel`, `sand`, or `stone` selects dump truck; `steel`, `plank`, `board`, or
`vehicle` selects flatbed; all other resources use a box truck. This explicit
name/category policy is a deterministic fallback, not a catalog schema or a
claim about any original game's vehicle definitions.

Procedural builders provide car, bus, box/flatbed/dump/tanker truck, locomotive,
and freight wagon geometry with wheel geometry and stable paint variants. The
native scene uses separate wheel nodes for the displayed body and rotates them
from the existing interpolated cell motion. Stopped vehicles brighten a separate
brake-light pair. Vehicle objects cast shadows. Headlights use an emissive
material and appear when the OpenRepublic scene look's sun elevation is at or
below 3 degrees. A moving vehicle gets one small, reusable-mesh exhaust puff
proxy; it disappears while stationary. These thresholds and effects are
presentation choices.

## Ambient traffic

`--ambient-traffic` enables up to eight decorative cars on enabled native road
segments. It requires `--native-economy`; without it the feature is off. Positions
are a pure deterministic function of the tick, the supplied road polylines, and
OpenRepublic presentation constants. The current scene supplies each enabled
unit road edge as a short polyline. Ambient cars do not create orders, consume
resources, block roads, or participate in the economy. Their motion is not a
traffic simulation and does not establish WRSR routing behavior.

## Verification boundary

`VERIFIED` unit coverage checks procedural mesh index/color validity, dimensions,
ground contact, deterministic variants and model selection, plus deterministic
ambient positions and invalid/empty road handling. Client compilation checks
the Wicked scene integration. The application is not launched for this task;
legibility, shadows, lighting, and performance need a separate desktop review.
