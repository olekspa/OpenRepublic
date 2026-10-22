# Utility flow diagnostics - 22 October 2026

VERIFIED - the authoritative C++ economy now records the most recent utility tick:
component membership, committed generation, material/output-eligible demand,
actual consumption, unused supply and per-resource starved recipe counts.
Exact two-limb amounts format as decimal strings without overflow or floating
point rounding. Multi-utility allocation remains atomic and the first short
resource remains the stall explanation while every short network is counted.

Measurements are observations outside canonical world state: querying them does
not alter revision, hash or save bytes. Restore and catalog replacement discard
them until a subsequent measured tick. The typed API and strict JSONL query page
networks and optionally filter by a building against full membership. Each row
returns at most 128 member IDs with full memberCount and membersComplete fields.
The graphical building HUD uses the same API and displays the measurement tick,
flow, shortages, component root and full membership count.

OBSERVED - an independently authored two-building GUI fixture restores at tick2
with unavailable measurements. GUI Step1 reaches tick3; compact and expanded
factory HUDs show generated3, used2, eligible demand2, unused1, short recipes0,
component OR1 and2 buildings. VERIFIED - actual client pipe equals a fresh
headless continuation, including complete diagnostics and hash5515297699003679218.
GUI disconnect followed by Step1 reaches tick4, where the factory reports missing
power, generated0/used0/demand2/unused0/short1, component OR2 and1 building.
GUI reconnect followed by Step1 reaches tick5 and resumes production, with
flow3/2/2/1 and zero starved recipes. These are authored OpenRepublic quantities.

Independent review found and repaired a measurement bug that reported remaining
budget as generation and a shortage-loop change that replaced the original first
missing resource. Tests cover exact large totals, eligibility gates, isolated
networks, observation invalidation, hash/save stability, strict protocol inputs,
permissions, bounded membership and filtering beyond its returned sample.

Validation and final receipt are recorded below. Original utility formulas,
physical units, original generation and automatic imported topology remain
UNKNOWN. No original game was executed. The broader development goal remains
incomplete. Next: verify exact source building identities and attachment/port
semantics before binding imported networks to the independent simulation.

VERIFIED - the reconnected tick5 pipe state and complete diagnostics equal a fresh
headless replay of both GUI commands; hash10523497293467897552, revision10.
Tick4 shortage figures were observed in the GUI; no tick4 pipe receipt was retained.


VERIFIED - final Release regression: client67/67 and headless66/66 pass.
Both full builds succeeded. The client content test was rebuilt after a synthetic
fixture correction; parse-failure guards now stop before dependent test accesses.
Independent final source review found no remaining blocker in these changes.

