# Synthetic economy calendar

The economy calendar is an opt-in OpenRepublic rule. Catalogs without a
`calendar` declaration retain their existing behavior and canonical catalog
text. Calendar dates derive from the world's simulation tick, so native world
saves and checkpoints continue to persist the date through their existing tick
field. The calendar is OpenRepublic simulation state; it is not a WRSR save
format or compatibility claim.

## Catalog declarations

Declare the calendar at catalog scope:

```text
calendar ticks_per_day 4 start_year 1980 start_day 1
```

`ticks_per_day` must be from 1 through 100000. `start_year` is a positive 32-bit calendar year.
`start_day` is an ordinal from 1 through 360. The initial date is visible before
any ticks; after each completed `ticks_per_day` ticks the day advances.

Optional recipe lines immediately following a `recipe` set its output
percentage for a season:

```text
  recipe harvest every 1 out grain:10
  season spring 50
  season summer 150
  season autumn 100
  season winter 0
```

The percentage is a multiplier on recipe outputs: 100 preserves the declared
amount, 50 produces half, 150 produces one and a half, and 0 produces none.
Input consumption is unchanged. Percentages must be from 0 through 10000. Integer results round down per output amount.
Unspecified seasons use 100 percent. Seasonal modifiers are only meaningful
with a declared calendar. These are synthetic rules; they do not claim source
game semantics.

## OpenRepublic date and season policy

This module uses a synthetic 360-day year with twelve 30-day months. Days
1-90 are spring, 91-180 summer, 181-270 autumn, and 271-360 winter. Day 1 is
the first day of the year. The date advances deterministically from the
catalog's start date and tick count.

The original game's new-game setup facts are documented in
[`GAME_SETUP.md`](GAME_SETUP.md). **VERIFIED (static review, build context
23935965):** `StartYearFromOption` maps options 0-6 to 1960, 1970, 1980, 1930,
1940, 1950, and 1920, and values above 1000 are direct years. The setup day
depends on terrain type (3→129, 1→26, 2→51, other→53; tutorial map
`tutorial_map7`→285). This implementation accepts explicit year/day values;
it does not infer a terrain type, menu option, or game default.

**UNKNOWN:** original month lengths, season boundaries, whether the observed
setup day is zero-based, and the original game's recipe or crop response to
seasons. WRSR parity for this calendar and seasonal production behavior is
**UNKNOWN**. The 360-day year, month lengths, season ranges, and modifier math
above are OpenRepublic policy only.

## Example

[`data/scenarios/farm_seasons.json`](../data/scenarios/farm_seasons.json)
declares a synthetic farm whose harvest output varies by season. Its recipe
amounts and modifiers are authored demonstration values and not extracted
game data.
