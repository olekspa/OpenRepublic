# Synthetic population and workforce

**Status:** OpenRepublic-authored synthetic economy rules. **WRSR parity: UNKNOWN.**

This optional model gives catalog authors a small population and staffing system.
It does not claim to reproduce original-game population, food, work, migration,
or production behavior. Catalogs that declare none of these rules retain the
existing economy behavior and state hashes.

## Catalog declarations

The catalog DSL accepts these declarations:

```text
resource food
food food 1 every 1
building housing 1 1
  housing 100
end
building mine 1 1
  workers 20
  store ore 100
  recipe extract every 1 out ore:10
end
```

`housing <n>` belongs inside a building definition and provides capacity for
`n` residents. `workers <n>` belongs inside a building definition and declares
the number of workers required for full recipe rate. Both values must be
positive. The top-level optional declaration `food <resource> <n> every <ticks>`
sets the resource consumed per resident at each interval. The food resource
must be a declared non-utility catalog resource. A food-consuming catalog must
provide storage for that resource wherever it expects food stock to be held.

When no food rule is declared, residents fill declared housing capacity. When a
food rule is declared, residents can move into housing up to capacity based on
available food, and the configured quantity is consumed at each food interval.
Housing and worker totals are accumulated across placed buildings. Buildings
with worker requirements receive available residents in deterministic building
order; production at a staffed building scales with assigned workers up to the
declared requirement. Buildings with no worker declaration retain their prior
production behavior.

These are deliberately simple synthetic rules. They do not model commuting,
household assignment, job qualifications, wages, education, health, age,
unemployment policy, or any other unimplemented mechanic.

## API summary

The typed API summary reports population, assigned workers, and required
workers from the authoritative world snapshot. The protocol v0 `summary`
response keeps the existing `population` and `workers` fields, returning
unsigned totals as decimal strings when a housing, worker, or food declaration
enables population reporting and `null` otherwise. `workers` is the assigned
count when a worker requirement is declared and remains `null` otherwise;
`workersRequired` is the
declared requirement total and is also a decimal string when workforce rules
are active. Summary text uses the same opt-in availability distinction.

## Scenario

`data/scenarios/understaffed_mine.json` is an OpenRepublic-authored fixture with
five housing places and a mine requiring ten workers. It demonstrates a
half-rate recipe under synthetic rules; its output is not a WRSR measurement or
compatibility claim.

## Evidence boundary

**VERIFIED:** the model is implemented in the OpenRepublic economy and covered
by source-level API/host tests and synthetic fixtures.

**UNKNOWN:** whether WRSR uses these declarations, quantities, assignment
rules, food timing, or production scaling. No original-game parity is claimed.
