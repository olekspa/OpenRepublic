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

# Region import into the economy

`compat::ImportRegionIntoWorld` adapts plain building type names and horizontal
world coordinates into an `economy::World`. It resolves catalog types exactly
after removing a leading `MIRRORZ_` prefix. Treating that prefix as the base
building type is **INFERRED** from the reviewed building-type loader evidence;
the mirrored geometry itself is not represented in the economy.

The importer selects the requested radius around a center placement, maps
coordinates with `floor((coordinate - origin) / cellSizeMeters)`, and creates a
bounded grid with a four-cell margin. Grid dimensions are capped at 512 cells;
when needed, the selected radius is reduced and recorded in the report. Unknown
types, out-of-region positions, and rejected footprint/access-cell collisions
are counted separately. Placements are submitted in source order and accepted
through one economy tick. Roads are omitted because road data is not decoded.

This adapter does not parse map packages or establish original-game economic
behavior. The input data is synthetic/plain data in its tests; map placement
format evidence and limits remain as recorded in the reverse-engineering ledger.
