# Construction sites

**OpenRepublic synthetic rules. WRSR parity: UNKNOWN.** This lifecycle is authored for the independent economy and does not interpret imported `$COST_*` directives. Their units and runtime semantics remain unsupported; see [construction evidence](CONSTRUCTION_EVIDENCE.md).

Catalogs may add one optional declaration inside a building definition:

```text
build planks:20 bricks:12 work 8
```

Amounts are integer native resource units. `work` is a positive count of simulation ticks. Resource names must resolve to non-utility catalog resources; duplicate resource names and zero quantities are rejected. Catalogs without this declaration preserve the existing completed-building placement path and serialization/hash behavior.

Controllers may submit `place_construction_site` with the same `type`, `x`, and `z` fields as `place_building`. A site reserves the normal building footprint and access cell. Haul orders can deliver only declared resources to the site's bounded construction store. Once all required materials are present, one unit of work advances on each simulation tick. Materials are consumed on completion; the same building ID becomes operational in place with its ordinary inventory initialized empty. Cancellation/removal refunds nothing. There is no partial construction-material refund.

Snapshots report `constructionSite`, state (`waiting_materials`, `building`, or `done`) and progress from 0 to 1. The compact summary counts active sites. Native world saves use version 7 only while a site or queued site-placement command exists; prior save versions remain readable. Site progress/store fields participate in the canonical state hash only while a site is active.

These are OpenRepublic policy choices, not claims about WRSR delivery, work, material usage, progress, completion, or cancellation behavior.
