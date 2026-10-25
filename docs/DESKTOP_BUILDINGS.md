# Native desktop building presentation

The native economy scene uses OpenRepublic procedural meshes selected from the
catalog building name. Rules recognize names containing mine/quarry, power or
heating plant, mill/bakery/factory, farm, storage/warehouse, housing/apartment,
depot/garage, border/customs, or market/shop terms. Unknown names use a generic
hall. These templates are visual policy and do not decode original building
assets or establish WRSR geometry, dimensions, materials, or behavior.

The horizontal model extent is bounded by the native footprint in display
units. Its vertical proportions and deterministic palette variation are
OpenRepublic-authored. Construction-site snapshots use a partial shell and
scaffold whose height follows the synthetic `constructionProgress` field.
Dark scene mode selects warm window colors. A small number of production
emitters are shown for heating/power/boiler names only while a recipe reports
`remainingTicks > 0` in the authoritative snapshot.

`VERIFIED` source and procgeo tests cover mesh validity, index bounds,
footprint bounds, deterministic output, and scaffold growth. Synthetic
construction semantics and all WRSR parity remain **UNKNOWN**. Desktop
appearance, selection feedback, lighting/emission, particle quality, and
comparison to the original game require separate graphical acceptance.
