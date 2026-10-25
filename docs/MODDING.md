# Modding OpenRepublic

OpenRepublic supports its own text catalog, Lua tuning files, sandboxed Lua scene scripts, and JSON scenario fixtures. These interfaces are project-authored formats. They are not WRSR mod formats and do not establish WRSR gameplay parity. Synthetic mechanics documented here have **UNKNOWN WRSR parity**.

## Economy catalog text, version 1

Catalog text starts with `openrepublic-economy 1`. `#` begins a comment through the end of a line. Names are tokens without spaces. Resource declarations must precede declarations that refer to them; vehicle types referenced by a depot fleet must also be declared. A building definition starts with `building <name> <width> <depth>` and ends with `end`.

```text
openrepublic-economy 1

resource grain
resource flour
resource bread
resource coal
utility power

vehicle truck capacity 4 speed 1

building farm 2 2
  store grain 100
  recipe harvest every 2 out grain:10
end

building mill 2 2
  store grain 100
  store flour 100
  recipe grind every 1 in grain:2 out flour:1
end

building bakery 2 2
  store flour 100
  store bread 100
  recipe bake every 1 in flour:1 out bread:1
end

building border_post 1 1
  store grain 100
  store coal 100
  tradepost
end
trade grain buy 5 sell 3
trade coal buy 7 sell 2
```

### Declarations

| Declaration | Scope and meaning |
|---|---|
| `resource <name>` | Catalog scope; a storable, haulable resource. |
| `utility <name>` | Catalog scope; declares a utility resource. Utility quantities are not stored, hauled, or traded. Connections and utility recipes are opt-in. |
| `vehicle <name> capacity <units> speed <cells-per-tick>` | Catalog scope; positive cargo capacity and movement speed. |
| `vehicle <name> capacity <units> speed <cells-per-tick> mode rail` | Catalog scope; declares a rail-only vehicle. |
| `building <name> <width> <depth>` | Starts a building type definition with integer footprint dimensions. Must end with `end`. |
| `store <resource> <capacity>` | Building scope; storage capacity for a declared non-utility resource. A storage-only building needs no recipe. |
| `recipe <name> every <ticks> in <resource:amount> ... out <resource:amount> ...` | Building scope; recurring recipe interval and optional `in` and/or `out` sections with positive integer resource amounts. Resources must be declared; stored materials need matching `store` slots. Utility recipes must run every tick, and one recipe cannot both consume and produce utility resources. |
| `trade <resource> buy <price> sell <price>` | Catalog scope or inside a building definition; declares per-unit buy/sell prices for a non-utility resource. A trade post must also store the traded resource. |
| `housing <places>` | Building scope; opts into synthetic housing capacity. |
| `workers <count>` | Building scope; declares workers required for a building's full recipe rate. |
| `food <resource> <amount> every <ticks>` | Catalog scope; optional synthetic food consumption rule. Resource must be declared and storable. |
| `build <resource:amount> ... work <ticks>` | Building scope; synthetic construction materials and positive work duration. The resources must be declared non-utility resources. Buildings with this declaration can be placed as construction sites in the supported modes. |
| `calendar ticks_per_day <n> start_year <year> start_day <day>` | Catalog scope; optional synthetic calendar. Day is 1 through 360. |
| `season <spring\|summer\|autumn\|winter> <percent>` | Immediately after a recipe inside a building; scales that recipe's outputs for that season. Requires a catalog calendar; unspecified seasons use 100 percent. |
| `tradepost` | Building scope; marks this type as an eligible explicit trade station. |
| `depot` | Building scope; marks the type as a depot for owned vehicles. |
| `fleet <vehicle-type> <count>` | Depot building scope; positive number of vehicles to spawn when the depot is placed. Declare vehicle types before the building. |
| `handling <load-ticks> <unload-ticks>` | Depot building scope; positive synthetic loading and unloading durations. Defaults to one tick each if omitted. |
| `station` | Building scope; marks the type as a rail station for synthetic rail haul routes. |
| `end` | Closes the current building definition. |

Population, staffing, trade, construction, calendar, utilities, depots, and rail are optional extensions. Keep their original-game meanings and parity **UNKNOWN**; see [population](POPULATION.md), [trade](TRADE.md), [construction sites](CONSTRUCTION_SITES.md), [calendar](CALENDAR.md), [utilities](UTILITY_NETWORKS.md), [depots](DEPOTS.md), and [rail](RAIL.md).

### Lua economy rules

The optional `--rules <file.lua>` economy-host argument applies constrained Lua tuning to a catalog. The file returns a table with `schema_version = 1`, plus optional `buildings` and `vehicles` tables keyed by catalog names:

```lua
return {
  schema_version = 1,
  buildings = {
    farm = { speed_percent = 150, storage_percent = 100 },
  },
  vehicles = {
    truck = { capacity_percent = 200, speed_percent = 125 },
  },
}
```

Supported building fields are `speed_percent` and `storage_percent`; supported vehicle fields are `capacity_percent` and `speed_percent`. Percent values are bounded from 10 through 1000. Building speed percent adjusts recipe intervals; vehicle speed percent scales cells-per-tick; storage and capacity percentages scale the corresponding authored values. Names and structural declarations must match the catalog; rule reload cannot add resources, buildings, recipes, or feature declarations. The implementation is sandboxed and rejects unknown fields or invalid values. `data/rules/economy_rules.lua` is an example. See [the Lua rules loader](../src/scripting/LuaRulesLoader.cpp) and the controller host's `reload_rules`/`rules_history` methods for its bounded use.

### Lua scene scripts

Scene scripts control OpenRepublic presentation through the sandboxed `SceneControlState` API. They do not access the economy or Wicked Engine objects directly. A script defines `function on_frame(dt)` and gets the API from `_ENV["or"]` (because `or` is a Lua keyword):

```lua
local api = _ENV["or"]
function on_frame(dt)
    local look = api.look.get()
    api.look.set { exposure = look.exposure, sunElevationDegrees = 35 }
    api.look.preset("dusk")
    api.camera.set { yawDegrees = 30, elevationDegrees = 25, distanceFactor = 1.1 }
end
```

The client can load a script with `--scene-script <file.lua>` in a compatible showcase/region view. `look.get`, `look.set`, `look.preset`, `camera.set`, `api.log`, and elapsed `or.time` are exposed under the validated scene contract. There are no Lua standard libraries; memory, source size, nesting, and instruction budgets are bounded. Runtime errors disable that script instance. Read [Lua scene scripts](LUA_SCENE_SCRIPTS.md) for the exact exposed fields and limits; examples are [day_cycle.lua](../data/scripts/day_cycle.lua) and [orbit_camera.lua](../data/scripts/orbit_camera.lua).

## Scenario JSON, schema version 1

Scenario files are OpenRepublic-authored fixtures used by the economy host and scoring/controller tools. Required top-level fields are `schemaVersion`, `id`, `name`, `description`, `roads`, `buildings`, and `score`. The supplied examples in [`data/scenarios`](../data/scenarios) are the schema reference.

```json
{
  "schemaVersion": 1,
  "id": "small_chain",
  "name": "Small chain",
  "description": "An OpenRepublic synthetic fixture.",
  "roads": [
    { "a": { "x": 1, "z": 2 }, "b": { "x": 2, "z": 2 } }
  ],
  "buildings": [
    { "type": "farm", "x": 1, "z": 3 },
    { "type": "mill", "x": 2, "z": 3 }
  ],
  "score": { "metric": "stored_non_raw_units" }
}
```

- `roads` is an array of adjacent, axis-aligned unit edges, each with integer `a` and `b` coordinates of shape `{ "x": int, "z": int }`. Endpoint coordinates are limited to ±100,000.
- Optional `rails` uses the same one-cell edge shape and coordinate bounds for synthetic rail tracks.
- `buildings` must contain at least one entry. Entries have a catalog `type`, integer origin coordinates `x` (0 through 100,000) and `z` (1 through 100,000), and optional Boolean `constructionSite`.
- Optional `catalog` is non-empty catalog DSL text of at most 16,384 characters for a custom synthetic fixture. The scenario validator requires at least one building and one declared utility in it.
- Optional `utilityConnections` is an array of at most 512 `{ "resource": name, "aBuilding": index, "bBuilding": index }` entries; the endpoint indexes refer to distinct buildings in the initial `buildings` array.
- Optional `controllerScript` is an array (up to 32 entries) of `{ "method": "submit"|"trade_import"|"trade_export", "command": object }` requests applied by scenario tooling.
- `score.metric` supports `stored_non_raw_units`, `utility_multi_objective`, `completed_construction_sites`, `combined_multi_objective`, and `heat_units`; the `rail_vs_road` fixture has dedicated comparison fields. Combined objectives use `fedResidents`, `completedSites`, `treasury`, `throughputUnits`, `unmetDemandUnits`, and `storedNonRawUnits` as separately reported quantities. Do not add unlike units into one invented aggregate.

The combined roads, rails, buildings, and utility links share a 512-command setup limit. IDs use lowercase letters, digits, and underscore-separated segments; `name` and `description` are non-empty strings up to 512 characters. Unknown or malformed fields are rejected by the scorer.

For `rail_vs_road`, `score` requires `amount`, `roadSource`, `roadDestination`, `railSource`, and `railDestination` (building-array indexes), and accepts optional `comparisonTicks`. The fixture must declare rails, stations, and a `train` vehicle. For `combined_multi_objective`, `score.objectives` is a non-empty list of unique supported objective names. The scorer has further fixture-specific requirements for construction, utility, and population objectives; see [scenario validation and scoring](SCENARIOS.md).

Run an example with `python tools/scenario_score.py --host <OpenRepublicEconomyHost.exe> --scenario data/scenarios/full_chain.json --ticks 1000`. The scorer rejects unsupported or malformed input with diagnostics. Scenario metrics, rules, and hashes describe the authored OpenRepublic fixture and are not WRSR results. See [scenario guide](SCENARIOS.md) for metric definitions and controller horizon details.
