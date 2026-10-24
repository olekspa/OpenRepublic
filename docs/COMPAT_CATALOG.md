# Experimental local economy catalog adapter

**Utility extension:** repeatable `--utility-resource <exact-name>` options mark
existing definition resources as native utilities in app-owned desktop/host
regions. Their recipe rates remain, but they receive no inventory/storage or
truck transport. Supply requires an actual connected generator. This explicit
authored policy does not restore original stock/topology or establish original
units. See [utility networks](UTILITY_NETWORKS.md).

`OpenRepublicCompat` converts parsed `content::BuildingDefinition` values into
an `economy::Catalog`. It is an experimental local compatibility adapter; it
does not load game files itself and makes no claim of complete WRSR economy
compatibility. Tests use synthetic definitions only.

## Conversion rules and evidence

### Construction cost diagnostics (not a conversion)

The economy currently has no construction-site lifecycle: `PlaceBuilding`
creates a completed building, and haul orders deliver only to ordinary building
inventories. The opt-in economy-host flag `--construction-costs` asks the
catalog importer to report each retained `$COST_*` declaration as unsupported.
It does not change catalog fields or hashes. The flag is diagnostic only and
must not be read as construction-cost import support.

All source-to-economy conversion factors remain **UNKNOWN**: `$COST_WORK` raw
units to simulation ticks; `$COST_RESOURCE_AUTO` resource identity/amount rules;
`$COST_RESOURCE` argument roles, resource identity, and amount units; and
`$COST_ALL_MULTIPLY`, `$COST_WORK_BUILDING_NODE`,
`$COST_WORK_BUILDING_KEYWORD`, `$COST_WORK_BUILDING_ALL`, and related
node/keyword multipliers and selection rules. These fields remain raw and are
reported in `ImportReport::warnings`; no resource names, costs, multipliers, or
work durations are guessed. A native construction site, material-delivery
target, work progression, persistence, and completion transition are required
before an economy conversion can be added. The scenario economy API does not
currently support that lifecycle, so no `build_a_mill` scored construction
scenario is claimed.

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
- Buildings with nonzero converted production or consumption get an `operate`
  recipe. Each used recipe resource receives a storage slot. An explicitly mapped
  storage-only definition imports with storage and no recipe.
- Multiple production and consumption rows are accumulated independently by
  exact resource spelling and direction, so a recipe can have multiple inputs
  and outputs. `$CONSUMPTION_PER_SECOND` rows enter the same consumption totals
  after the verified factor of 60. A row is worker-scaled only when the parser
  observed a positive `$WORKERS_NEEDED` earlier in that definition; static
  evidence also identifies a separate type-code rule that rescales earlier
  declarations, which this adapter cannot identify and leaves unsupported.
- Resource names are not normalized, aliased, lowercased, or trimmed by the
  catalog importer. Spelling and case remain exact; for example, `eletric`,
  `electric`, and `Electric` are distinct synthetic catalog names. This avoids
  silently repairing source tokens without evidence.
- **INFERRED:** capacity uses `ceil(storageDaysFallback * larger daily rate)`
  in micro-units, with a minimum of four times the larger per-tick amount.
  This remains the default when no explicit capacity profile is supplied or a
  resource cannot be mapped unambiguously.
- **UNKNOWN:** real building footprints. The configured square footprint and
  the 20-unit, 2-cells-per-tick `truck` are OpenRepublic defaults, not WRSR
  data.

`ImportReport::directiveCoverage` counts definitions containing each directive
across the
provided definitions, grouped by directive spelling and classified as parsed
typed, retained raw, or ignored by the economy catalog. Each definition counts
once per directive in its strongest available category. “Ignored” means no
catalog rule consumes it; the content parser may still retain it in an opaque
directive record. `DescribeImport` prints this table in deterministic order.
It also records counts, ordered skipped buildings, warnings, and errors.

For an aggregate local install measurement, configure the headless build and
build the opt-in read-only target `OpenRepublicCatalogCoverageProbe`. Then run
`tools/measure_catalog_coverage.ps1 <install-or-definitions-directory>` (or pass
its build directory as the second argument). It resolves the conventional
`media_soviet/buildings_types` or `buildings_types` subdirectory when given an
install/content root, and otherwise treats the argument as the definition
directory. The probe reports parsed definition
count, valid imported economy building-type count, top skip reasons, and
directive coverage. Parse/read failures, unnamed files, and duplicate stems
are summarized separately. It writes no files and does not print source paths,
definition names, or contents. Keep install data and any captured output local;
tests use synthetic definition values only.

The adapter does not write game files, saves, or assets. Real building
footprints remain **UNKNOWN** in the evidence ledger, so the economy import
continues to use its configured square default.

## Optional authored storage capacity profiles

Both the desktop imported-region mode and the headless region host accept
`--storage-profile <file>`. The desktop requires `--map-package`, `--map-region`
and `--content-root`, using its app-owned economy. The host requires `--region`
and `--content-root`. Native checkpoint/catalog modes do not accept a profile:
checkpoints already contain their exact converted catalog.

The versioned format starts with this exact header. This example is synthetic
OpenRepublic policy; `BOX` and `grain` do not assert original-game semantics:

```text
OPENREPUBLIC_STORAGE_PROFILE 1
# micro-units per raw storage amount
FACTOR BOX grain 1000000
```

For a definition row `$STORAGE_IMPORT_SPECIAL BOX 3 grain`, that profile produces
a 3,000,000-micro-unit storage slot. `$STORAGE_SPECIAL` and
`$STORAGE_EXPORT_SPECIAL` are also eligible. Pair matching is exact and case
sensitive. Factors use positive finite decimal notation, without exponent syntax.
Blank lines and whole-line comments are allowed after the header. Files are
bounded to 64 KiB and 4096 factor rows; duplicate pairs and malformed files fail
before world creation. Windows paths retain Unicode support.

Capacity uses the binary64 product of raw amount and factor, rounded upward to a
whole micro-unit, with finite/positive/uint64 range checks. Explicit capacity
overrides the inferred fallback without a hidden minimum increase. A capacity
below one recipe tick skips the definition with a reason. A valid mapped
storage-only definition can now be placed by region import and used by existing
human/API haul controls. Capacity does not seed inventory.

Any general resource-empty storage row disables explicit mapping for that entire
definition. Multiple rows for the same resource are ambiguous and remain
unmapped. Unknown pairs/directives retain fallback for recipe resources;
storage-only definitions without a usable mapping remain excluded. Separate
import/export pools, shared capacity, allowed cargo modes and utility networks
remain **UNKNOWN** or unsupported. Profiles do not establish those behaviors.

**OBSERVED, static trace in one reviewed build:** special storage row quantities
are multiplied by a resource/class coefficient. This justifies a profile input;
the project bundles no original coefficient table. **VERIFIED, synthetic tests:**
profile parsing, conversion, storage-only import, authoritative hauling and exact
checkpoint continuation. Original-game stock, units and runtime parity remain
**UNKNOWN**. See the [development receipt](DEVELOPMENT_REPORT_2026-10-22_STORAGE_PROFILES.md).

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
through one economy tick. Road import is a separate subsequent step described below.

This adapter does not parse map packages or establish original-game economic
behavior. The input data is synthetic/plain data in its tests; map placement
format evidence and limits remain as recorded in the reverse-engineering ledger.

## Region roads

The economy now reserves each accepted building's access cell against later
footprints and rejects an access cell inside another footprint. Footprints also
cannot cover enabled or disabled roads. Region import reports access conflicts
separately (`skippedAccessConflict`), preserves accepted-only source-key mappings,
and caps notes with `maxDiagnosticNotes` (default 32). These checks operate on
provisional catalog footprints; they do not establish authentic building access.

`ImportRoadsIntoWorld` (`region_roads.h`) rasterizes straight road links (the decoded game road graph flattened to chords) onto the economy's unit-step road grid with a 4-connected walk, drops duplicates, then joins each imported building's access cell to the nearest road cell by a breadth-first path around other buildings (default limit 40 cells). Verified on one real save region (local only): 1,471 of 1,591 unit segments accepted, all 10 catalog-known buildings connected, and a haul order from the real iron mine to the real iron processing plant delivered until the destination store was full. Curved roads are approximated by chords; lane/width data is not used.
