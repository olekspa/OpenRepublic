# Authored storage capacity profiles - 22 October 2026

Imported definitions can now use explicit storage capacities through a bounded,
optional authored profile. Both the graphical app-owned region economy and the
headless region host load the same profile and call the same compatibility
adapter. Supported storage-only definitions enter the authoritative C++ world
without invented recipes and participate in existing logistics controls.

The profile maps exact transport-class/resource pairs to micro-units per raw
storage amount. Eligible resource-bearing directives are STORAGE_SPECIAL,
STORAGE_IMPORT_SPECIAL and STORAGE_EXPORT_SPECIAL. Conversion uses a specified
binary64 product followed by upward integer rounding. Explicit capacity replaces
the rate fallback; a capacity below one recipe tick skips the definition rather
than silently increasing capacity. Factors and amounts must be finite, positive
and representable. Invalid options leave the output catalog unchanged.

Unknown pairs/directives preserve fallback for recipe resources. Multiple rows
for one resource remain ambiguous. Any general resource-empty storage row
disables explicit mapping for that definition, avoiding an assumed shared-pool
conversion. Storage-only definitions without usable mappings remain excluded.
Mapped counts and diagnostics include only accepted buildings. Reports expose
unmapped/ambiguous rows and accepted storage-only building counts.

Use `--storage-profile <file>` with desktop --map-package/--map-region/--content-root
or host --region/--content-root. Native checkpoint/catalog modes reject the
profile because checkpoint catalogs already carry their converted capacities.
Remote desktop economy-url also rejects it: that process owns its own catalog.
Profiles have an exact versioned header, positive decimal factors, CRLF support,
Unicode file paths, duplicate-pair rejection and 64 KiB/4096-row bounds. See the
[format and example](COMPAT_CATALOG.md#optional-authored-storage-capacity-profiles).

VERIFIED - source/build/tests: full Windows Release client and headless builds
succeed. Full CTest passes **60/60 client** and **59/59 headless**. Parser tests
cover malformed/oversized inputs, transactionality and file loading. Adapter
tests cover explicit precedence, ceil, exact-product decimal boundaries,
unknown/duplicate/general rows, invalid/overflowing/undersized capacities,
storage-only inclusion and deterministic import. Desktop startup tests reject
missing/duplicate/invalid profiles and incompatible modes before creating a GPU
window. Independent review found rounding ambiguity and premature mapped-count
publication; both were repaired and regression tested.

VERIFIED - separate-process synthetic integration: authored definitions and a
profile import a producer with 2,000,000 capacity and a recipe-free depot with
3,000,000 capacity. Without a profile the depot stays excluded. The native host
accepts roads and a grain haul, then fills the depot to its exact capacity.
Exported checkpoint restores at an identical hash and matches eight subsequent
ticks, preserving the profile-derived catalog. All files are synthetic and
temporary; no original content fixture is required. The first fixture run placed
roads one cell off the access point; correcting the fixture established delivery.

OBSERVED - static research: one reviewed installed build multiplies special
storage scalar amounts by a resource/class coefficient. Private evidence and
the closed analysis snapshot remain outside this repository. The original
executable fingerprint was unchanged after analysis; it was never executed.
No original coefficient table or decompiled source is bundled. Profile units and
conversion remain authored OpenRepublic policy, not original-game runtime proof.

UNKNOWN - graphics/hardware/original-game comparison: no new live GUI profile
or real-content delivery acceptance was performed. Original inventory restoration,
shared capacity, separate import/export pools, vehicle cargo classes, utilities,
workforce and economy parity remain incomplete. This increment does not seed
stock or remove utility recipe inputs. Next useful work is a supported production
and utility boundary plus evidence-backed capacity profiles, followed by a real
delivery check. The global game goal remains active and incomplete.
