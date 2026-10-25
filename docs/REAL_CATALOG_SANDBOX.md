# Real-content economy sandbox

The native desktop economy has an opt-in local catalog mode:

```powershell
OpenRepublic.exe --native-economy --native-catalog-from-content "C:\path\to\media_soviet"
```

Pass a directory containing `buildings_types`, a `media_soviet` directory
containing `buildings_types`, or the `buildings_types` directory itself. The
loader reads regular files directly inside that directory, using each filename
stem as the exact economy type name. Inputs stay local and are opened read-only.
Definition files are bounded to 4 MiB each and 8,192 files total. Duplicate
stems, unreadable files, and malformed definitions fail startup before world
creation.

The catalog currently allows only these exact names: `bakery`, `coal_mine`,
`farm`, `heating_plant`, `power_plant`, and `storage`. Other definitions appear
in the startup import summary as skipped with an allowlist reason. Type names
vary between content installs; a partial imported catalog is possible. Startup
fails after printing the report when no supported allowlisted buildings import.
The mode cannot be combined with the
synthetic coal demo or construction-site catalog because those bootstraps depend
on synthetic type names.

Parsed production and consumption declarations use the existing experimental
converter. Rate time base and fallback capacity are inferred as described in
[COMPAT_CATALOG.md](COMPAT_CATALOG.md). Resources remain exact source spellings.
The current source parser does not expose a supported real footprint field, so
the catalog uses the importer's configured 4 by 4 OpenRepublic fallback
footprint. Storage-only definitions are included only when the importer can
map their storage declarations. Unsupported and malformed economic entries are
reported or skipped with reasons.

This is a local OpenRepublic sandbox, not a WRSR compatibility claim. Rate
units, fallback storage capacities, building footprints, production outcomes,
and gameplay parity remain **UNKNOWN**. `VERIFIED` source/headless coverage
uses only synthetic definition text created by the test. Do not commit local
content, extracted files, saves, or captured import output.
