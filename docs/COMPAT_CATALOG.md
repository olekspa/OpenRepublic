# Experimental local economy catalog adapter

`OpenRepublicCompat` converts parsed `content::BuildingDefinition` values into
an `economy::Catalog`. It is an experimental local compatibility adapter; it
does not load game files itself and makes no claim of complete WRSR economy
compatibility. Tests use synthetic definitions only.

## Conversion rules and evidence

- **OBSERVED, one reviewed game build:** the loader scales some production and
  consumption values by workers when `$WORKERS_NEEDED` was already read, and
  multiplies `$CONSUMPTION_PER_SECOND` values by 60 before storing them.
- **INFERRED:** source values represent per-game-day rates. The adapter applies
  a worker multiplier only when `workersDeclaredBefore` is true; per-second
  consumption receives the observed 60 multiplier. Building-level amounts are
  divided by `ticksPerDay` and rounded to integer micro-units per tick. A
  positive rate never becomes zero after rounding. One unit is 1,000,000
  micro-units.
- Resource names are sorted before catalog insertion. A building with an
  invalid catalog resource name is skipped with a reason. Duplicate rate rows
  for the same resource are combined by direction.
- Only buildings with nonzero converted production or consumption get an
  `operate` recipe. Each used recipe resource receives a storage slot.
- **INFERRED:** capacity uses `ceil(storageDaysFallback * larger daily rate)`
  in micro-units, with a minimum of four times the larger per-tick amount.
  Explicit storage rows are not mapped because their transport-class resource
  relationship is **UNKNOWN**; resource-bearing row count is reported.
- **UNKNOWN:** real building footprints. The configured square footprint and
  the 20-unit, 2-cells-per-tick `truck` are OpenRepublic defaults, not WRSR
  data.

`ImportReport` records the options, counts, ordered skipped buildings,
warnings, and errors. `DescribeImport` produces a deterministic human-readable
summary. The adapter does not read or write game files, saves, or assets.
