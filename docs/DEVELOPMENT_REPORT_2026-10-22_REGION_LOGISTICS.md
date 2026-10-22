# Shared app-owned region logistics controls - 22 October 2026

The explicit logistics tools now work with the app-owned imported region world,
not just the native authored sandbox. Human choices and external controllers
continue to reach the same C++ EconomyApi. Legacy remote economy-url behavior
is unchanged. This is an integration of native simulation controls into supported
world presentation, not recovered original-game simulation behavior.

ActiveOwnedEconomyWorld chooses the owned checkpoint world or the region world.
The region toolbar uses logistics-only mode: resource and vehicle palettes plus
haul-order inspection/retarget/cancel. Construction, demolition, native roads,
Frame world, Run and native Save remain native-only. Imported building selection
uses the existing exact accepted-placement bindings. No new map-to-construction
transform or approximate identity matching was introduced.

Resource eligibility comes from both endpoints' positive catalog storage rather
than first-recipe guesses. Choices submit exact names and native IDs with current
revision and queue-cap checks. Order selection remains identity-based. Only native
mode uses its stale primitive-scene guard. Shared toolbar input guards protect
world clicks. Imported regions now have compact bottom status and Details, keeping
existing imported inventory formatting. The remote bridge retains its old adapter.

VERIFIED: Windows Release desktop build succeeds; the full client CTest suite
passes58/58. The first GUI inspection exposed Details omitted from the nonnative
layout; removing that skip repaired its position, followed by a successful final
Release build and live check. The58-test receipt precedes that one-line layout
repair; no second full run is claimed.

OBSERVED: --synthetic-region-fixture starts with three bound buildings and no
native construction/road widgets. FarmOR1 and MillOR2 select through existing
scene bindings. Set haul source then select mill exposes grain and the truck
palette. GUI Haul to selected queues that explicit choice. Step24 reaches tick26,
one applied/zero rejected, with haul1/truck visible and mill output_full. Details
shows3 exact bindings, grain8/8 and flour8/8. Compact/Details is correctly positioned
in the default window and the compact world remains visible. Test client closed.

No original game files, packages, saves or installations were used or modified.
Real-content GUI acceptance, imported retarget/cancel acceptance, large catalog
menu scale and original-game simulation parity remain UNKNOWN. Vehicle palette
capacity uses catalog quantities, not a newly inferred original-game unit formula.
Native road/placement behavior is not asserted for imported coordinates.

Next: verify supported real-content presentation with these controls, finish
remaining order/save acceptance and safely integrate loading. Review and commit
the accumulated implementation as a coherent milestone. Prior dirty work is
preserved; changes remain uncommitted. Global game goal remains incomplete.
