# Transfer map from Rival Republics research

**Reviewed:** 21 October 2026
**Scope:** Read-only review of the local Rival Republics repository and its linked research. This note records reusable methods and evidence boundaries, not permission to copy game data or introduce the original game as a runtime dependency.

## What the existing project establishes

Rival Republics is an AI/modding and research system that interacts with the installed WRSR game. It is not a standalone renderer or an authoritative replacement simulation. Its interfaces, native addresses, and generated local datasets must not be treated as OpenRepublic APIs.

The strongest reusable lessons are:

- **Data ingestion:** a deterministic catalogue pipeline can preserve source-relative paths, source lines, raw directive tokens, normalized values, and parser diagnostics. Static configuration values can seed OpenRepublic content schemas when each field retains its provenance and confidence.
- **Geometry:** `.bbox` bounds can describe model envelopes, but do not establish the placement footprint, zoning grid, or construction-office clearance used by WRSR. Keep render bounds and game placement geometry as separate fields.
- **Terrain:** existing work has DDS readers and static terrain-layer extraction. Successful byte decoding is separate from proving each texture channel's runtime meaning, world orientation, or height transform. Validate coordinate transforms against independent landmarks.
- **Roads and utilities:** parsers, terrain-aware planners, validators, renderers, and comparison reports are useful patterns. Geometric proximity and planner edges remain hypotheses until native node/path or building-connectivity evidence supports them. OpenRepublic should use its own graph schema and preserve candidate provenance.
- **Simulation design:** the existing Python system provides examples of structured directives, validation, deterministic event logs, checkpoints, and measured optimization. Its AI strategy loop and Python performance results are not a native C++ economy model or a rendering benchmark.
- **Reverse engineering:** injected plugins and AgentBridge work reveal selected behavior in particular game builds. The evidence is version-sensitive; native addresses, process pointers, hooks, and input recipes do not belong in the standalone OpenRepublic runtime.

## OpenRepublic application

1. Keep game files read-only. Put exact executable fingerprints, private asset paths, Ghidra databases, and REA reports in the private research folder.
2. For each supported reader, retain the source-relative file identity, game build/context, raw inputs needed to audit normalization, normalized schema version, and structured errors. Never silently turn a missing or guessed field into a verified value.
3. Keep **model envelope**, **render transform**, **placement footprint**, **network connection**, and **simulation footprint** distinct. A value proven for one layer does not prove another.
4. Maintain an offline synthetic/open content path. Use redistributable fixtures for parser and simulation regression checks; do not commit extracted WRSR payloads or generated catalogues containing original game data.
5. Make the simulation authoritative and independent of Wicked Engine. Use stable OpenRepublic identifiers, a fixed-step update, validated versioned commands, and reproducible event/save data. Treat the existing project's action or strategy schemas as examples only.
6. Measure the OpenRepublic renderer and simulation separately with fixed, documented workloads. A third-party template's FPS and the prior Python simulator timings do not establish OpenRepublic performance.

## Findings that must remain bounded

| Area | Reusable result | Limit |
|---|---|---|
| Building and vehicle definitions | A local static reader can inventory selected definition fields and emit a production graph with diagnostics. | This is not a complete game schema. Resource prices, automatic construction materials, exact placement footprints, and some directive semantics remain unknown. |
| Save parsing | Bounds-checked parsing and one-field-at-a-time comparisons are good methods. | Building/save candidates are partial; opaque bytes remain unresolved. No complete writer, round-trip, or save compatibility contract has been established. |
| Road network | Read-only snapshots, native identity preservation, terrain-aware candidate planning, and before/after reports are useful. | `complete: true` can mean only that pagination finished. Proximity-based links and planner connectivity do not prove the original game's native topology. |
| Terrain | DDS decoding and static height/layer extraction can guide an OpenRepublic reader and test fixture design. | Height transform, axis orientation, water/resource channels, buildability, and some compressed formats remain inferred, unresolved, or unsupported. |
| Economy | Definition files and production/consumption fields can seed a small source-backed industry experiment. | Staffing linearity, calendar conversion, prices, construction costs, utilities, and transport behavior must not be guessed into authoritative simulation rules. |
| Native hooks | Version-specific observations can suggest experiments and identify counters to inspect. | The injected loader and native calls are not stable public APIs, and its GPL-licensed code is not a dependency for this MIT project. |

## Next experiments

1. Confirm the ModelViewer UI's NMF load, OBJ import, and export behavior on one local model while retaining all input files in the game installation. Static command labels alone are not a successful import.
2. Build a private asset-dependency manifest for one building definition → model → material → texture chain, retaining unknown references and source provenance.
3. Use the existing Python DDS reader as a reference for a small local-only OpenRepublic terrain reader; compare format, dimensions, and selected values before rendering a sample.
4. Select one source-backed production chain and keep unit conversion, staffing, transport, and pricing assumptions explicit in the simulation schema.
5. Convert any road geometry to a new OpenRepublic graph using stable IDs and explicit confidence; keep save-derived topology optional until a full record and relationship are established.
