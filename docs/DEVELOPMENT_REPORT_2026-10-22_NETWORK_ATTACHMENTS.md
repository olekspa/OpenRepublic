# Linear-network building reference candidates - 22 October 2026

OBSERVED - read-only static serializer, loader and map-preload tracing for build
23935965 establishes three parallel signed integer arrays in the fifth node
payload section. The first array indexes the building table allocated from the
building file's declared count. The next two arrays remain opaque. Other node
header references resolve to network hubs, fragments or semaphore objects and
must not be treated as building identities. Exact binary identities, offsets,
raw analysis, package names and provenance remain in private research storage.

VERIFIED - the independent content reader now preserves those three arrays as
ordered buildingIndex/opaqueA/opaqueB records before reading the existing four
link arrays. Negative indices remain visible in decoded data. A separate
ExtractNetworkBuildingAttachmentCandidates helper requires the exact caller's
source-table count and rejects invalid indices, excessive counts and invalid
coordinates transactionally. It returns candidates in source order. No inferred
nearest-building association or automatic World connection is added.

VERIFIED - synthetic tests cover parallel-array ordering rather than interleaved
triples, signed preservation, alignment, truncation, candidate bounds, geometry
checks and transactional output. Read-only corpus validation of four electro
files in two packages passes the native decoder. Every retained reference is
within its package's declared building-table bounds; an empty network stays empty.
This proves structural decoding and range checks, not gameplay utility behavior.

UNKNOWN - exact correspondence between scanner placement ordinals and native
building identities, opaque array meaning, port/type semantics, support placement,
voltage conversion and original runtime utility connectivity. The candidate
helper is deliberately insufficient to energize the independent simulation.
Next: verify source identity correspondence and supported port definitions before
adding an explicitly evidenced importer. Original files were read-only and no
original game was launched; the global game-development goal remains incomplete.

VERIFIED - final Release regression passes67/67 client and66/66 headless.

