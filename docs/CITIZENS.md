# Synthetic citizen needs, happiness, and migration

**Status:** OpenRepublic-authored synthetic economy rules. **WRSR parity: UNKNOWN.**

The population model remains opt-in. Add the top-level `citizens` declaration to
enable happiness, migration, and happiness-scaled staffing:

```text
resource bread
food bread 1 every 2
utility heat
citizens
building home 1 1
  housing 12
  recipe warmth every 1 in heat:1
end
```

Food is drawn from declared storage across the town, so a market-like building
can receive food through ordinary production and haul orders. Heat and water
count as needs when the matching utility resource is declared. Each occupied
housing building must have a recipe consuming that utility; a utility-starved
housing recipe leaves that need unmet. These rules use authored integer units
and the existing idealized utility links. They do not model physical units,
distribution range, household preferences, or individual citizen records.

Happiness is an integer from 0 to 100. It is the resident-weighted average of
declared needs: food, heat, and water have equal weight. Food counts as met for
residents that the existing food rule supports. Heat and water count as met
when housing has the corresponding consuming recipe and it is not stalled for
that utility. With no residents, declared housing capacity supplies the
weights; food is met only when at least one ration is stocked, and utility
needs are checked against each housing building's current stall state.
`happyResidents` is the population multiplied by happiness,
rounded down to a whole resident.

At each existing food interval, residents immigrate by one when happiness is
at least 75 and food and housing can support another resident. When happiness
is at most 50, one resident emigrates. Food shortage caps residents immediately
to the number of rations available. Worker assignment uses `happyResidents`,
while the published population remains the resident total. These are deliberate
deterministic synthetic rules, not recovered WRSR formulas.

## Interfaces and persistence

World snapshots expose `happiness` and `happyResidents`; the typed summary,
JSONL `summary`, and runtime overview expose the same values additively when
`citizens` is declared. JSONL returns large counts as decimal strings, matching
existing population fields, and returns `null` for both fields without opt-in.
Native world saves include the existing per-building resident records because
the declaration enables population persistence. Happiness is derived from the
saved world and is not stored separately.

`data/scenarios/thriving_town.json` exercises housing, a farm-to-mill-to-bakery
food chain, market storage, and a connected heating utility. Its
`happy_residents` score is the final happy resident count. The baseline
controller may route recipe inputs and warehouse stock through its ordinary
planning rules; the scenario score does not imply original-game outcomes.

## Evidence boundary

**VERIFIED:** source and synthetic tests cover the opt-in catalog, deterministic
happiness, immigration/emigration gates, workforce scaling, summaries, and
save/load state.

**UNKNOWN:** WRSR citizen needs, happiness calculation, thresholds, migration
rate, worker availability, and food/utility delivery semantics. No WRSR parity
is claimed.
