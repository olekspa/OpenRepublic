# Persistent planning guidance

Start from the latest world observation every round. If any building is stalled or any order is waiting, blocked, or carrying cargo to a full destination, return at least one useful action unless the observation proves no legal action can help. Do not stop just because a prior action failed.

Use `advice.unservedInputs` and each building's `inputAdvice` first. Use an offered haul command only when its source, destination, resource, and vehicle remain valid. Review `advice.retargets` for full-destination waits. Never cancel an order with cargo in transit. If an order is waiting for capacity, inspect inventory and available alternatives before changing it. If an input has no producer, consider placing a catalog building that produces it near an accessible road; do not guess footprints or place on occupied cells. For haul routing, use each building's supplied `accessCell`, `onEnabledRoad`, `roadComponent`, and `nearestRoadCells`; if the source and destination access cells are not on the same road component, connect a nearest road endpoint to the access cell with clear single-segment orthogonal `build_road` actions. Do not infer access locations from a footprint edge or use cells that overlap buildings.

Only connect resources whose catalog entry has `utility: true`. Utility flow is not cargo and cannot use haul orders. A resource without that flag must use ordinary inventories and haul orders when it is transportable.

Construction sites appear in `constructionSites` and on their building entry with the current state, progress, and per-resource required/held/missing material amounts. When a site is `waiting_materials`, use `missingMaterials` and its `availableSources` to create a schema-valid `create_haul_order` from a stocked building to that site, using an available catalog vehicle. Do not send construction materials to an ordinary completed building or guess requirements. A `place_construction_site` action is valid only for a type whose catalog declares `constructionWorkTicks` and `constructionMaterials`; placement still needs an available footprint/access cell and connected roads for delivery. After material delivery, check the refreshed site state before acting again. These are synthetic OpenRepublic rules only.

Trade is available only where the catalog marks a resource `tradeDeclared: true`
and a building type is a trade post. The observation includes current treasury,
buy/sell prices, and the catalog storage/recipe information. Use
`trade_export` or `trade_import` with exact fields `buildingId`, `resource`,
`amount`, and `rateCap`. Export only stock above a reserve; import only a missing
recipe input when free capacity and treasury cover `buyPrice * amount`. Trade
commands go through their dedicated host methods and are validated locally
before submission. These rules describe the synthetic OpenRepublic model only.

Use the score's current value, since-start change, and previous-round change to evaluate the plan. Explain the action's intended bottleneck in `notes`. Return an empty action list only when all stall and order summaries are clear, or name the exact blocking constraints that prevent every available action.

## Depot-owned trucks

When `depotFleet` is present, it lists depot ownership, parked/idle status,
current leg and remaining handling ticks, order, cargo and cell. Pair it with
`vehicles`, `orders`, each building's inventory, and `advice.unservedInputs` /
`inputAdvice` to re-plan on every observation. Use `dispatch_truck` with
`vehicle`, `source`, `destination`, and `resource` (optional unsigned integer
`amount`) only for a parked truck whose `ownerDepotId` names a catalog depot.
Choose a stocked source that has the resource and a destination with storage
room that currently lacks a recipe input. Sources may be producers, ordinary
storage, or a trade post after a valid import. Never dispatch a busy truck, and
do not issue another delivery for a destination/resource already covered by an
active order. If a trade post can import the missing input, ensure a declared
price, free post capacity, and enough treasury; the missing recipe input may
belong to another building. A later round can dispatch the newly imported stock.
These are OpenRepublic synthetic controller rules, not WRSR behavior.
