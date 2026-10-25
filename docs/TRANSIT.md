# Synthetic passenger transit and commuting

**Status:** OpenRepublic synthetic economy rules. **WRSR parity: UNKNOWN.**

This opt-in model supports a road bus loop, bus stop buildings, seat-limited
passenger counts, stop dwell ticks, and deterministic workforce access for
longer commutes. It is independent project behavior; no WRSR passenger,
citizen, route, timetable, or staffing formula is claimed.

## Catalog declarations

Transit is disabled unless the catalog declares both transit settings and its
own station and bus types:

```text
transit threshold 4 transfer 1
building bus_stop 1 1
  station
end
vehicle bus capacity 4 speed 2
```

`threshold` is the maximum direct road-path length eligible for a synthetic
commute without transit. A home farther away requires two distinct route stops
that are each road-reachable from the home and workplace within `transfer` road
cells. Disabled roads are excluded. The route must be an ordered loop whose
successive stops, including the closing pair, have enabled-road paths.

## Route command and bus cycle

The typed API `CreateTransitRoute(revision, orderedStopBuildingIds, busType)`
queues one route command. JSONL host requests submit
`{"kind":"create_transit_route","stops":[1,2],"busType":"bus"}` through
`submit`. The command is checked at the tick boundary: transit must be declared,
each ID must name a station building, the vehicle type must be a road vehicle,
and the stop loop must be connected. A bus begins at the first stop. Vehicle
capacity is its seat count; vehicle speed is its cells-per-tick rate. The bus
unloads its current passengers, waits the unload dwell, boards waiting synthetic
passengers up to capacity, waits the load dwell, then follows the shortest road
path to the next ordered stop and repeats.

World snapshots expose route stop IDs, bus type, current stop, bus cell,
occupancy/seats, phase, and remaining dwell. Route creation and bus movement are
included in the deterministic transit state hash. A commuter is eligible only
if a direct road path exists; distant commutes also need two distinct stops as
described above. Worker allocation consumes available home residents and job
capacity in stable building order. This first model does not retain individual
citizen identities, job assignments, passenger destinations, fares, schedules,
or multiple bus dispatch policies.

## Scenario

`data/scenarios/commuter_town.json` is an OpenRepublic-authored fixture scored
by `tools/scenario_score.py`. It declares a home, two stops, a distant mine,
the bus route, and the combined objectives `fedResidents`, `workersAssigned`,
and `workersRequired`. The score records authored fixture values only.

## Evidence boundary

**VERIFIED** means source behavior and synthetic tests in this repository.
Scenario results are **OBSERVED** only for the named OpenRepublic build, fixture,
and tick horizon. Every transit rule and quantity remains synthetic;
compatibility and parity with WRSR are **UNKNOWN**.
