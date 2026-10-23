# Construction declarations and the next lifecycle gate

Prepared 23 October 2026. Original-game evidence is scoped to Steam build
`23935965`. OpenRepublic construction behavior and WRSR behavior have separate
acceptance gates. No original game process was run for this investigation.

## Provenance and evidence scope

**VERIFIED (static identity and operations):** read-only inspection matched the
installed executable to the input identity stored in two existing analyzed
Ghidra projects. The investigation reused their existing analysis with
`-noanalysis -readOnly`; it did not import another database or modify executable
bytes. Targeted string references, instruction ranges and selected helper
decompilations establish the operations below. A whole-loader decompilation did
not complete and supplies no evidence for these conclusions.

Private receipt identifier: `construction-loader-20261023`. Its retained evidence
includes the executable identity comparison, string-reference inventory, focused
instruction listings and helper analysis. Exact local paths, fingerprints and
raw analysis remain outside this repository under [D-003](DECISIONS.md).

**OBSERVED (definition inventory):** local text declarations use a phase token
and numeric argument for `$COST_WORK`, a text token and numeric argument for
`$COST_RESOURCE` and `$COST_RESOURCE_AUTO`, and a single numeric argument for
`$COST_ALL_MULTIPLY`. Text occurrence shapes alone do not establish loader error
handling, semantic units or supported ranges.

## Operations established by the inspected loader

| Declaration | VERIFIED static operation | Boundary |
|---|---|---|
| `$COST_WORK` | Reads phase text and a numeric value, resolves the phase text through a dedicated comparison routine, and appends a work record. | The numeric value's units, timing and gameplay meaning remain UNKNOWN. The runtime meaning of internal phase identifiers and their mapping to an independent simulation remain UNKNOWN. |
| `$COST_RESOURCE` | Reads a text token and float, calls a resource lookup, and appends an entry to the most recently appended work record. | The numeric value's units, validation and subsequent consumption rules remain UNKNOWN. Appending does not establish how explicit duplicate declarations behave later. |
| `$COST_RESOURCE_AUTO` | Reads a recipe token and float, then calls a helper with the most recent work-record index and model-related context. The helper selects node records, dispatches named recipes, creates resource entries and combines matching resource entries. | This is a recipe expansion path. The token cannot generally be treated as a resource identifier, and the supplied float cannot be treated as a final material quantity. Complete geometry measures, coefficients, units and end-to-end formula parity remain UNKNOWN. |
| `$COST_ALL_MULTIPLY` | Immediately traverses all work records accumulated at that point and multiplies two numeric fields of each existing cost entry by the parsed scalar. | The two fields' semantics remain UNKNOWN. This branch does not establish a permanent multiplier for declarations encountered later. Preserve ordering. |
| `$COST_WORK_BUILDING_NODE` | Associates a node name with the latest work record; the inspected insertion path searches a bounded set of 32 name slots. | Geometry selection and node-identity compatibility require separate validation. This observed bound is not a general limit on all construction records. |
| `$COST_WORK_BUILDING_ALL` and `$COST_WORK_BUILDING_KEYWORD` | Target selector information on the latest work record. | Retaining the declarations does not establish their full geometric or runtime effect. |
| `$COST_WORK_VEHICLE_STATION` and `$COST_WORK_VEHICLE_STATION_ACCORDING_NODE` | Target the latest work record and follow distinct geometric paths. The numeric station path reads two groups of three floats; the node path reads a node token. | Coordinate orientation, derived station geometry, missing-node behavior and runtime vehicle use remain UNKNOWN. |

**OBSERVED (helper tokens):** the inspected automatic-resource helper contains
branches for names including `ground_asphalt`, `wall_concrete`, `wall_panels`,
`wall_brick`, `wall_steel`, `wall_wood`, `tech_steel`, `techelectro_steel`,
`electro_steel`, `roof_steel` and `roof_woodsteel`. It emits concrete resource
lookups including `workers`. These names establish a distinction between recipe
selectors and output resources. They do not establish a complete public recipe
catalog or a units conversion.

## Compatibility gate

An original-definition adapter may retain ordered construction declarations,
source-line provenance, phase text and raw numeric values. It must preserve the
latest-work context and distinguish explicit resource entries from automatic
recipe declarations. Orphan declarations, malformed arguments, excessive input
and unsupported selectors need explicit diagnostics and bounded handling.
Rejecting unsupported input is an OpenRepublic adapter policy; the inspected
branches do not establish that WRSR rejects the same input.

**UNKNOWN / unsupported:** automatic conversion of these declarations into a
playable WRSR construction schedule; original worker-days or material units;
phase transition conditions; vehicle productivity; geometry-derived quantities;
material delivery/consumption timing; cancellation/refund rules; and original
construction-state import. The current evidence does not authorize treating
zero as instantaneous completion, interpreting a numeric scalar as work-days,
or flattening the declarations into unordered building-wide costs.

Before promoting an adapter to simulation behavior, trace the numeric field
consumers and phase transitions, establish the automatic recipe's geometric
inputs and constants, and check ordering effects with build-matched evidence.
Controlled original-game comparisons, when explicitly authorized, are a separate
gate from static analysis and OpenRepublic tests.

## Implementation and follow-up

### Ordered source profile implemented (23 October 2026)

**VERIFIED (OpenRepublic implementation and synthetic tests):**
`content::ParseBuildingConstructionProfile` now extracts a bounded ordered
profile from original definition text. It preserves directive argument tokens,
including numeric spelling and duplicate entries, line provenance, and following
non-directive rows. Only those following rows preserve their original whitespace
and comments; this is not a lossless copy of complete source text.

The four primary declarations have distinct operation kinds: work declaration,
explicit resource, automatic recipe request, and immediate multiplier event.
Multipliers remain at their source position without evaluating either original
numeric field. Resource declarations retain the latest work context even when
their own numeric token is malformed. Malformed work declarations or unexpected
non-comment work rows clear that context, preventing fallback to an earlier phase.
Known phase-scoped node/station selectors retain context and opaque following
rows with explicit unsupported diagnostics. Unknown `$COST_*` spellings remain
visible rather than disappearing.

Finite `double` conversion is only a lexical inspection convenience. It neither
validates the original float representation nor establishes gameplay ranges,
units, phase identifiers or resource lookup. Automatic recipes remain unresolved;
no phase lookup, geometry expansion, material summation, multiplier arithmetic,
or native cost mapping occurs. Explicit-only profiles also have no supported
gameplay cost import. Diagnostics must be inspected alongside retained operations.

The reader first validates the input with `ParseBuildingDefinition`, inheriting
its 4 MiB input, 4096-byte line, 2048-byte token, 128-token line and 65,536-directive
limits. It additionally caps retained following rows at 65,536. Hard validation
or resource-limit failures leave output unchanged. Optional construction defects
produce a profile with diagnostics; they are not claims that WRSR rejects the
same declarations. Blank/comment rows are retained without invalidating numeric
inspection. Other following rows on the four primary operations get conservative
`UnexpectedContinuation` diagnostics and remain available for investigation.

**OBSERVED (read-only corpus, build 23935965 context):** all 488 named definition
files produced profiles with zero read, definition-parse or profile-parse failures.
474 contain construction declarations. The reader retained 6,737 operations:
1,132 work declarations, 12 explicit resource declarations, 1,386 automatic
recipes, seven multipliers and 4,200 opaque selectors; it retained 5,348 following
rows. There were no malformed numeric-form or orphan-phase diagnostics in this
sample. All 474 construction profiles have diagnostics: 4,200 unsupported-selector
and 1,386 unresolved-recipe diagnostics, plus 539 conservative unexpected-row
diagnostics. These counts establish extraction coverage, not original runtime
behavior or invalid original content. Before/after SHA-256 checks confirmed all
488 inputs unchanged; their identities and fingerprints remain private.

The opt-in `OpenRepublicConstructionProfileProbe` target prints aggregate JSON
only, with `costImportSupported:false`, no source paths, names or payloads:

```powershell
cmake --build build/openrepublic-headless-vs2022 --config Release --target OpenRepublicConstructionProfileProbe
.\build\openrepublic-headless-vs2022\Release\OpenRepublicConstructionProfileProbe.exe --root <definition-directory>
```

It recursively inspects `.ini` files and selects definitions with `$NAME`.
Unsupported declarations are inspection results, not process failures. Read or
parse failures return 1; a directory with no named definitions also returns 1.
Unavailable input or usage errors return 2. No game content is a CTest fixture.

### Consumer trace follow-up (23 October 2026)

**VERIFIED (static, build 23935965, one inspected pathway):** a reinitialization
branch copies ordered definition work records into per-building state, copies
declared resource values into reference fields and clears running fields. It
derives cumulative phase thresholds from entries associated with the resource
independently resolved as `workers`. The threshold normalization uses a total
that passes through float-to-integer truncation. No arithmetic policy is adopted
from that observation; behavior for fractional, zero or extreme totals is UNKNOWN.

An inspected selector chooses the first ordered threshold greater than the
effective progress scalar. An ordinary progress path increases that scalar,
uses a completion gate near 0.9999 and writes 1.0 on its completion branch. Other
modes transform the scalar or call a different helper. This supports a
dimensionless progress interpretation for that path, not a universal schedule.
Current/reference ratios and reference-scaled updates use the two resource
fields in the inspected consumers.

Private receipt identifier: `construction-loader-20261023/consumer-receipt`.
The trace reused existing matched analysis read-only. **UNKNOWN:** worker-days,
material tons, delivered-versus-consumed quantities, all initialization modes,
operational activation, the raw `$COST_WORK` numeric argument and runtime parity.
The next original-game evidence gate is one non-worker material's inbound
delivery/stock-removal path, plus that raw work argument's consumers.

### Native lifecycle remains a separate implementation

Implement an **OpenRepublic-authored construction lifecycle** independently of
the original-definition adapter. This is a proposed increment, not a completed
feature or a recovered WRSR rule set:

1. Define explicit authored ordered phases, resource requirements and work units
   in the native catalog. Version the contract and document its units.
2. Keep site inventory, current phase, accumulated work and completion state in
   authoritative C++ state. A building under construction becomes operational
   only after its authored completion conditions pass.
3. Deliver materials through the existing native logistics authority. Specify
   deterministic material reservation/consumption and work admission before
   implementing them; keep cancellation and refunds out of the first increment
   unless their rules are explicitly authored.
4. Expose phase, shortages, progress and completion through the shared typed API
   and persist them with the world's versioned save/hash representation.
5. Test a small authored multi-phase site for missing materials, insufficient
   work, exact completion, inactive production before completion, and identical
   uninterrupted versus checkpoint-restored evolution. Route future UI controls
   through that same API.

Acceptance would establish an independently specified native lifecycle only.
Original construction parity remains open until the compatibility gate above
passes. See the [project plan](PROJECT_PLAN.md), [parity roadmap](PARITY_ROADMAP.md)
and [evidence ledger](REVERSE_ENGINEERING_LEDGER.md) for the wider objective.
