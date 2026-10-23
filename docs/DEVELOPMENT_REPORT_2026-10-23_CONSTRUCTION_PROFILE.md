# Ordered construction profiles and consumer evidence

Prepared 23 October 2026 (America/New_York). Full WRSR parity remains active
and incomplete. This increment addresses source-declaration compatibility and
evidence; it does not implement native construction gameplay.

## Scope and coordination

The current source is `OpenRepublic`, initially at `28a29dc` for this increment.
Claude continued its independent road/header and construction-cost diagnostic
work. Shared main advanced to `84a6cb1` during validation. Its source and branch
were preserved; no external branch was merged, reset or stashed. New parser
files, tests and the probe were kept separate from native economy construction.

The prior workforce increment is recorded in
[its receipt](DEVELOPMENT_REPORT_2026-10-23_WORKFORCE.md). Workforce changes are
recorded in local commit `70f551a`; the profile increment is recorded in a
separate scoped local commit with this receipt to leave the shared checkout
available for integration. No push or publication is part of this increment.

## Implemented contract

**VERIFIED (OpenRepublic source and synthetic tests):**
`ParseBuildingConstructionProfile` retains all `$COST_*` operations in order,
argument spelling, duplicates, source lines and opaque following rows. Work,
explicit resource, automatic recipe and multiplier operations remain distinct.
Malformed work clears the current context; malformed resources keep source
phase provenance without a usable numeric value. Multipliers remain source
events, not permanent factors. Known opaque node/station selectors keep current
phase context. Unsupported spelling and automatic recipes stay diagnostic.

Existing definition input limits and the explicit 65,536-row retention cap fail
transactionally. Optional declaration defects return inspectable profiles with
diagnostics. Numeric values are finite lexical doubles, not original float or
gameplay acceptance. Directive-line whitespace/comments are not retained;
following rows preserve their text excluding line endings. No final costs,
geometry formulas, phase identifiers or game-unit conversions are invented.

The explicit `OpenRepublicConstructionProfileProbe` target provides bounded
read-only recursive `.ini` inspection with aggregate JSON. It prints no file
paths, content names, raw records or fingerprints. Output explicitly contains
`costImportSupported:false`. The target is excluded from default builds and
original content is never a CTest input.

## Verification receipt

- Full Windows Release headless build passed; **79/79 CTest** passed (11.02 s).
- Full Windows Release client build passed; **80/80 CTest** passed (11.27 s).
- Focused reader/legacy-definition tests passed **2/2**. Coverage includes
  ordering, duplicates, global multiplier placement, malformed/orphan context,
  actual station spelling and opaque rows, interrupted work context, nonfinite
  values, bounded input failures and unchanged output on failure.
- Independent review found two gaps (malformed-resource provenance and actual
  station fixture spelling); both were corrected and tested. The initial probe
  member-name compile mismatch was corrected before the final full builds.
- An authored probe fixture verified aggregate JSON counts, unresolved-recipe
  reporting, unchanged input hash, and unavailable-directory exit/privacy behavior.
- `git diff --check` passed. No original-game execution or modification, GUI
  acceptance or graphics/hardware validation is claimed.

**OBSERVED (read-only installed definition corpus, build 23935965 context):**

| Inventory | Count |
|---|---:|
| Named definitions / profiles | 488 / 488 |
| Profiles with construction declarations | 474 |
| Hard read/definition/profile failures | 0 |
| Retained operations | 6,737 |
| Work / explicit resource / automatic recipe / multiplier | 1,132 / 12 / 1,386 / 7 |
| Opaque selectors / retained following rows | 4,200 / 5,348 |
| Malformed argument / orphan context diagnostics | 0 / 0 |
| Unsupported selector / unresolved recipe diagnostics | 4,200 / 1,386 |
| Conservative unexpected-row diagnostics | 539 |

All 474 construction profiles have diagnostics. This is extraction coverage,
not validation of original construction schedules; unexpected-row diagnostics
do not establish malformed original content. Before/after SHA-256 checks proved
all 488 inputs unchanged. Exact inputs and fingerprints remain private. A second
directory contained two `.ini` files with no `$NAME`; they were skipped and the
probe correctly returned its documented no-named-definitions status.

## New evidence and remaining gates

**VERIFIED statically for one build-23935965 pathway:** definition work entries
are copied into runtime records; declared numeric values initialize reference
fields while running fields clear. Entries independently identified as `workers`
contribute to cumulative phase thresholds. A selector and ordinary completion
path use a dimensionless progress scalar; completion tests near 0.9999 and writes
1.0. Other modes differ. See [construction evidence](CONSTRUCTION_EVIDENCE.md)
for provenance and limits. No decompiled implementation or proprietary payload
was copied into this repository.

**UNKNOWN:** the raw `$COST_WORK` argument's consumer, physical work/material
units, inbound versus consumed stock, complete automatic geometry formulas,
all initialization modes and original operational activation. The next source
evidence task is a non-worker material's delivery/removal path. Native authored
site lifecycle, cost-adapter integration and original runtime comparison remain
separate outstanding work. Claude's independent diagnostics branch is not merged
or evaluated as native construction completion by this receipt.
