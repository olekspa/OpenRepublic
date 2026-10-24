# Construction sites

**OpenRepublic synthetic rules. WRSR parity: UNKNOWN.** This lifecycle is authored for the independent economy and does not interpret imported `$COST_*` directives. Their units and runtime semantics remain unsupported; see [construction evidence](CONSTRUCTION_EVIDENCE.md).

Catalogs may add one optional declaration inside a building definition:

```text
build planks:20 bricks:12 work 8
```

Amounts are integer native resource units. `work` is a positive count of simulation ticks. Resource names must resolve to non-utility catalog resources; duplicate resource names and zero quantities are rejected. Catalogs without this declaration preserve the existing completed-building placement path and serialization/hash behavior.

Controllers may submit `place_construction_site` with the same `type`, `x`, and `z` fields as `place_building`. A site reserves the normal building footprint and access cell. Haul orders can deliver only declared resources to the site's bounded construction store. Once all required materials are present, one unit of work advances on each simulation tick. Materials are consumed on completion; the same building ID becomes operational in place with its ordinary inventory initialized empty. Cancellation/removal refunds nothing. There is no partial construction-material refund.

World snapshots report `constructionSite`, state (`waiting_materials`, `building`, or `done`) and progress from 0 to 1. Active site entries also expose additive `constructionMaterials` records with `resource`, `required`, `held`, and `missing` decimal strings. This lets controllers distinguish a partially supplied site from an empty one. The existing `catalog` response declares each type's `constructionMaterials` and `constructionWorkTicks`; `advise` still reports ordinary recipe inputs only. The compact summary counts active sites. Native world saves use version 7 only while a site or queued site-placement command exists; prior save versions remain readable. Site progress/store fields participate in the canonical state hash only while a site is active; the added query fields do not change state or hashes.

The baseline autopilot adds a haul for each waiting site's missing material when an ordinary building currently has that resource in stock. It skips an already ordered source/site/resource route and chooses the nearest stocked source with stable ID tie-breaking. Delivery still requires host-accepted stock, vehicle capacity, a valid road route and destination room. No source stock means the baseline cannot serve the site.

Construction-site scoring is an OpenRepublic scenario-tool policy: `completed_construction_sites` gives one score unit per completed site and includes final site IDs, types, states, and progress in the score receipt. It does not assign a cost or economic value to construction.

These are OpenRepublic policy choices, not claims about WRSR delivery, work, material usage, progress, completion, or cancellation behavior.
