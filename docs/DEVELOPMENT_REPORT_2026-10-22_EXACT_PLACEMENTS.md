# Exact placement identities - 22 October 2026

The previous placement scanner could recover plausible buildings and transforms,
but could not prove that its vector positions were native building-table slots.
A fully plausible record inside an opaque tail could replace a later real record
while still satisfying the declared count. That prevents safely joining network
building references to imported placements.

OBSERVED - read-only static reader/writer tracing on WRSR build 23935965 establishes
ascending building-table order for serialized records. The fixed record and its
extension are followed by count-controlled arrays and seven nested helper layouts.
The package header supplies the format version. Exact binary fingerprints, raw
analysis, offsets into original code and source identities remain private.

VERIFIED - OpenRepublic now has a bounded sequential record walker for the two
observed format versions 121 and 124. It reads signed little-endian counts, bounds
input at 512 MiB and record count at 65,536, validates supported fields and nested
lengths, and requires the declared records to consume the file exactly. It never
recovers a failed strict walk by scanning for a plausible name.

The public ExtractPlacedBuildingsExact entry point validates the existing name
and affine-transform predicates at each exact boundary, then sets optional
sourceBuildingTableIndex to that record's source-table slot. This identifies a
slot in the exact loaded package, not a persistent ID across edits or packages.
The legacy ExtractPlacedBuildings scanner leaves this optional identity unset.
LoadMapPlacements reads header.bin, falling back to headerE.bin only when absent.
Versions 121/124 select strict extraction. A malformed supported profile or a short
present package header fails transactionally. Unsupported/missing metadata retains
the legacy candidate path without claiming a native source identity.

VERIFIED - the independent native exact reader accepts both original packages:
736 records for version 121 and 3,284 for version 124, each through exact EOF. All 4,020
transforms satisfy the existing placement predicates. This is offline decoding,
not original-game execution or gameplay acceptance. A separate private join maps
all retained references in four electro files into those exact tables; electrical
ports and opaque reference fields remain UNKNOWN.

Synthetic tests exercise all seven helper families, both variable helper branches,
optional blocks, UTF-16 strings, packed bytes, count bounds, reserved bytes,
truncation, exact EOF and transactional failure. A complete plausible name/matrix
decoy inside an authored tail demonstrates the old scanner's identity limit;
the strict reader skips that tail and identifies the actual second record.
Review found two weak test fixtures: an incomplete four-helper branch and a decoy
truncated before its matrix. Both are corrected and the focused tests rerun.

VERIFIED - both full Release builds succeed; final CTest passes 68/68 client and
67/67 headless. Independent source review found no reader correctness blocker.
New GUI acceptance was not attempted after the owner's Escape interruption.

Next: trace the remaining electrical attachment/port fields and voltage boundaries
before constructing automatic imported utility connections. The exact identity
reader closes the record-boundary gap; it does not establish energized networks,
physical units, original production state, or broad gameplay parity. No original
files were modified or original game launched. The global goal remains incomplete.

VERIFIED - broader read-only inventory: all 44 packages with supported headers
pass the native exact reader (22 format 121, 22 format 124), with zero failures.
Of 65 inventoried files, 15 have other versions and six lack either package header;
those retain candidate extraction. This check uses header.bin precedence followed
by headerE.bin, matching the loader. Full private identities and fingerprints are
retained outside Git. Acceptance does not extend the supported format list.
