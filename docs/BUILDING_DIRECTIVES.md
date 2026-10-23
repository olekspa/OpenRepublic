# Building definition directives

**Corpus scope (OBSERVED):** 490 text candidates were scanned under `buildings/`, `buildings_types/`, plus root `buildingtypes.ini`; 488 building definitions contained `$NAME`. There were 359 distinct `$DIRECTIVE` spellings. Counts below are definitions containing the directive; occurrence count includes repeated rows. Argument shapes use `N` for a numeric-looking token and `T` for other text; this is a lexical survey, not a declaration schema. Private paths, file names, examples, and payloads are excluded.

**Parser coverage:** ?Yes? means the current `ParseBuildingDefinition` extracts a typed field or retains a typed raw economic declaration. ?No? means the line is preserved only as opaque `other` arguments (and does not imply the directive is ignored by WRSR). Electrical declarations are extracted as source geometry/diagnostics, not native port semantics. Raw economic fields are only retained; no economy rules consume them.

**Meaning evidence:** `$WORKERS_NEEDED`, `$PRODUCTION`, `$CONSUMPTION`, and `$CONSUMPTION_PER_SECOND` have loader evidence in the ledger. The construction follow-up also establishes ordered static operations for four cost declarations; units and gameplay formulas remain UNKNOWN. See [construction evidence](CONSTRUCTION_EVIDENCE.md). Other labels are lexical inferences or unknowns; names/shapes are not loader evidence. The opt-in `--construction-costs` host diagnostic reports the retained cost declarations as unsupported (it changes nothing by default).

## Complete directive inventory

| Directive | Definitions | Occurrences | Argument shapes | Parsed by OpenRepublic | Meaning / evidence |
|---|---:|---:|---|:---:|---|
| `$AIRPLANE_STATION_30M` | 1 | 1 | 6 args (1 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$AIRPLANE_STATION_40M` | 1 | 1 | 6 args (1 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$AIRPLANE_STATION_50M` | 2 | 4 | 6 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$AIRPLANE_STATION_75M` | 1 | 1 | 6 args (1 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$AMBIENT_SFX` | 169 | 171 | 1 args (171 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ANIMATION_MESH` | 5 | 5 | 1 args (5 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ANIMATION_SPEED_FPS` | 2 | 2 | 1 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_FACTOR_NATURE` | 7 | 7 | 1 args (7 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_FACTOR_NATURE_ADD` | 20 | 20 | 1 args (20 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_FACTOR_POLLUTION` | 18 | 18 | 1 args (18 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ATTRACTIVE_FACTOR_POLLUTION_ADD` | 5 | 5 | 1 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ATTRACTIVE_FACTOR_SIGHT` | 13 | 13 | 1 args (13 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_FACTOR_SIGHT_ADD` | 6 | 6 | 1 args (6 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_FACTOR_WATER` | 1 | 1 | 1 args (1 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ATTRACTIVE_FACTOR_WATER_ADD` | 10 | 10 | 1 args (10 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ATTRACTIVE_SCORE` | 23 | 23 | 1 args (23 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_SCORE_ALCOHOL` | 2 | 2 | 1 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_SCORE_CULTURE` | 6 | 6 | 1 args (6 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_SCORE_SPORT` | 2 | 2 | 1 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_TYPE_CARUSEL` | 4 | 4 | 1 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_TYPE_GALLERY` | 1 | 1 | 1 args (1 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_TYPE_MUSEUM` | 4 | 4 | 1 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_TYPE_SIGHT` | 2 | 2 | 1 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_TYPE_SWIM` | 2 | 2 | 1 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_TYPE_ZOO` | 1 | 1 | 1 args (1 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ATTRACTIVE_USE_FORGOT_EVEN_MATCH` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$BORDER_BUILDING` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$CABLEWAY_HEAVY` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$CABLEWAY_LIGHT` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$CITIZEN_ABLE_SERVE` | 53 | 53 | 1 args (53 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$CIVIL_BUILDING` | 36 | 36 | 0 args (36 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$COALFUEL_WOKRPLACE` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$CONNECTIONS_AIRPORT_DEAD_SQUARE` | 39 | 46 | 0 args (46 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTIONS_DEAD_MAKE_FROM_CONNECTIONS` | 9 | 9 | 2 args (9 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTIONS_DEAD_MAKE_FROM_ROADS` | 24 | 26 | 2 args (26 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTIONS_PEDESTRIAN_DEAD_SQUARE` | 6 | 6 | 0 args (6 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTIONS_ROAD_DEAD` | 7 | 14 | 0 args (14 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTIONS_ROAD_DEAD_SQUARE` | 277 | 475 | 0 args (475 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTIONS_SPACE` | 6 | 10 | 0 args (10 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTIONS_WATER_DEAD_SQUARE` | 8 | 16 | 0 args (16 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ADVANCED_POINT` | 158 | 2180 | 3 args (2180 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_AIRPORT_DEAD` | 14 | 94 | 0 args (94 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_AIRROAD` | 6 | 8 | 0 args (8 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_BULK_INPUT` | 18 | 38 | 0 args (38 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_BULK_OUTPUT` | 17 | 43 | 0 args (43 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_CABLEWAY` | 14 | 22 | 0 args (22 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_CONNECTION` | 114 | 454 | 0 args (454 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_CONVEYOR_INPUT` | 51 | 116 | 0 args (116 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_CONVEYOR_OUTPUT` | 35 | 91 | 0 args (91 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_HIGH_INPUT` | 7 | 16 | 0 args (16 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_HIGH_INPUT_ONLY` | 1 | 3 | 0 args (3 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_HIGH_OUTPUT` | 13 | 30 | 0 args (30 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY` | 1 | 3 | 0 args (3 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_LOW_INPUT` | 11 | 38 | 0 args (38 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_LOW_INPUT_ONLY` | 1 | 2 | 0 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_LOW_OUTPUT` | 11 | 49 | 0 args (49 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ELETRIC_LOW_OUTPUT_ONLY` | 1 | 2 | 0 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_FENCE` | 7 | 14 | 0 args (14 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_HEATING_BIG` | 4 | 16 | 0 args (16 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_HEATING_SMALL` | 3 | 8 | 0 args (8 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_MUDROAD_DEAD` | 11 | 132 | 0 args (132 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_PEDESTRIAN` | 301 | 899 | 0 args (899 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_PEDESTRIAN_ALLOWPASS` | 4 | 8 | 0 args (8 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_PEDESTRIAN_DEAD` | 31 | 134 | 0 args (134 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_PEDESTRIAN_NOTPICK` | 17 | 52 | 0 args (52 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_PIPE_INPUT` | 37 | 61 | 0 args (61 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_PIPE_OUTPUT` | 17 | 33 | 0 args (33 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL` | 9 | 40 | 0 args (40 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL_ALLOWPASS` | 28 | 108 | 0 args (108 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL_ALLOWPASS_INPUT` | 10 | 20 | 0 args (20 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL_ALLOWPASS_OUTPUT` | 10 | 20 | 0 args (20 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL_BORDER` | 2 | 4 | 0 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL_DEAD` | 1 | 10 | 0 args (10 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL_DEADEND` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_RAIL_HEIGHT` | 2 | 8 | 0 args (8 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ROAD` | 362 | 397 | 0 args (378 occurrences), 3 args (19 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ROAD_ALLOWPASS` | 19 | 40 | 0 args (40 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ROAD_BORDER` | 3 | 5 | 0 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ROAD_DEAD` | 282 | 1451 | 0 args (1417 occurrences), 3 args (34 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ROAD_INPUT` | 5 | 5 | 0 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_ROAD_OUTPUT` | 5 | 5 | 0 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_SEWAGE_INPUT` | 11 | 21 | 0 args (21 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_SEWAGE_OUTPUT` | 17 | 28 | 0 args (28 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_STEAM_INPUT` | 3 | 5 | 0 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_STEAM_OUTPUT` | 3 | 5 | 0 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_TRAMROAD_DEAD` | 8 | 168 | 0 args (168 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_TRAMTROLLEYS_DEAD` | 7 | 84 | 0 args (84 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_WATERPIPE_INPUT` | 22 | 39 | 0 args (39 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_WATERPIPE_OUTPUT` | 15 | 34 | 0 args (34 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONNECTION_WATER_DEAD` | 6 | 218 | 0 args (218 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$CONSUMPTION` | 70 | 146 | 2 args (146 occurrences) | Yes | VERIFIED - loader behavior documented in ledger |
| `$CONSUMPTION_INCREASE_ACCORDING_YEAR` | 2 | 2 | 3 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$CONSUMPTION_PER_SECOND` | 38 | 38 | 2 args (38 occurrences) | Yes | VERIFIED - loader behavior documented in ledger |
| `$CONSUMPTION_WATER_REQUIRED_QUALITY` | 13 | 13 | 1 args (13 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$COST_ALL_MULTIPLY` | 7 | 7 | 1 args (7 occurrences) | Yes | VERIFIED static operation: scales two numeric fields of existing cost entries immediately; field meanings UNKNOWN. |
| `$COST_RESOURCE` | 3 | 12 | 2 args (12 occurrences) | Yes | VERIFIED static operation: resource lookup and append to latest work record; units/consumption UNKNOWN. |
| `$COST_RESOURCE_AUTO` | 474 | 1386 | 2 args (1386 occurrences) | Yes | VERIFIED static operation: geometry-dependent recipe expansion for latest work record; complete formulas/units UNKNOWN. |
| `$COST_WORK` | 474 | 1132 | 2 args (1132 occurrences) | Yes | VERIFIED static operation: resolves phase text and appends ordered work record; timing/units UNKNOWN. |
| `$COST_WORK_BUILDING_ALL` | 44 | 81 | 0 args (81 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$COST_WORK_BUILDING_KEYWORD` | 132 | 229 | 1 args (229 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$COST_WORK_BUILDING_NODE` | 418 | 1751 | 1 args (1751 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$COST_WORK_VEHICLE_STATION` | 375 | 1246 | 0 args (21 occurrences), 5 args (10 occurrences), 6 args (1215 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$COST_WORK_VEHICLE_STATION_ACCORDING_NODE` | 427 | 893 | 1 args (893 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$COUNT_LIMIT` | 8 | 8 | 1 args (8 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$DISABLE_MIRROR_TYPE` | 14 | 14 | 0 args (14 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$DISABLE_PEDESTRIAN_ROAD_ENTRY` | 3 | 3 | 0 args (3 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$ELECTRIC_DISABLE` | 21 | 21 | 0 args (21 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$ELETRIC_CONSUMPTION_HEATING_WORKER_FACTOR` | 4 | 4 | 1 args (4 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$ELETRIC_CONSUMPTION_LIGHTING_WORKER_FACTOR` | 9 | 9 | 1 args (9 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$ELETRIC_CONSUMPTION_LIVING_WORKER_FACTOR` | 22 | 22 | 1 args (22 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$ELETRIC_CONSUMPTION_LOADING_FIXED` | 19 | 19 | 1 args (19 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$ELETRIC_CONSUMPTION_UNLOADING_FIXED` | 19 | 19 | 1 args (19 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$ELETRIC_WITHOUT_LIGHTING_FACTOR` | 68 | 70 | 1 args (70 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ELETRIC_WITHOUT_WORKING_FACTOR` | 65 | 66 | 1 args (66 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$END` | 2 | 2 | 0 args (2 occurrences) | No | UNKNOWN - token/marker role is not established here |
| `$ENGINE_SPEED` | 49 | 49 | 1 args (49 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$FIRESTATION` | 1 | 3 | 6 args (3 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$FIRESTATION_POINT_NODE` | 1 | 2 | 1 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$HARBOR_EXTEND_AREA_WHEN_BULDING` | 10 | 10 | 1 args (10 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$HARBOR_OVER_TERRAIN_FROM` | 15 | 15 | 1 args (15 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$HARBOR_OVER_WATER_FROM` | 15 | 15 | 1 args (15 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$HEATING_DISABLE` | 101 | 101 | 0 args (101 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$HEATING_ENABLE` | 3 | 3 | 0 args (3 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$HEATING_WITHOUT_WORKING_FACTOR` | 2 | 2 | 1 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$HELIPORT_AREA` | 7 | 7 | 1 args (7 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$HELIPORT_STATION` | 7 | 15 | 6 args (15 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$IGNORE_COUNT_LIMIT` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$LONG_TRAINS` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$MENU_SFX` | 147 | 147 | 1 args (147 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$MONUMENT_ENABLE_TRESPASSING` | 6 | 6 | 0 args (6 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$MONUMENT_GOVERNMENT_LOYALTY_RADIUS` | 6 | 7 | 1 args (7 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$MONUMENT_GOVERNMENT_LOYALTY_STRENGTH` | 6 | 7 | 1 args (7 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$MOVEABLE_DOOR` | 3 | 36 | 8 args (36 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$MOVEABLE_DOOR_PASS` | 1 | 4 | 8 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$NAME` | 488 | 488 | 1 args (488 occurrences) | Yes | UNKNOWN - token/marker role is not established here |
| `$NO_LIFESPAN` | 14 | 14 | 0 args (14 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$OBSOLETE` | 17 | 17 | 0 args (17 occurrences) | No | UNKNOWN - token/marker role is not established here |
| `$OFFSET_CONNECTION_XYZW` | 14 | 22 | 0 args (22 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$OUTWATER_MAX_QUALITY` | 4 | 4 | 1 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$PARKING_NOT_BLOCK` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$PARKING_NOT_BLOCK_DETOUR_POINT` | 3 | 6 | 3 args (6 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$PARKING_NOT_BLOCK_DETOUR_POINT_PID` | 27 | 1126 | 4 args (1126 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$PARTICLE` | 103 | 342 | 5 args (2 occurrences), 6 args (340 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$PARTICLE_REACTOR` | 2 | 3 | 3 args (3 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$PARTICLE_SNOWREMOVE` | 4 | 4 | 5 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$POLLUTION_HIGH` | 4 | 6 | 0 args (6 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$POLLUTION_MEDIUM` | 6 | 9 | 0 args (9 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$POLLUTION_SMALL` | 6 | 6 | 0 args (6 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$PRODUCTION` | 81 | 89 | 2 args (89 occurrences) | Yes | VERIFIED - loader behavior documented in ledger |
| `$PRODUCTION_CONNECT_TO_SUN` | 1 | 1 | 1 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$PRODUCTION_CONNECT_TO_WIND` | 2 | 2 | 1 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$PRODUCTION_DECREASE_ACCORDING_YEAR` | 2 | 2 | 3 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$PRODUCTION_SEWAGE_POLLUTION` | 9 | 9 | 1 args (9 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$PROFESORS_NEEDED` | 34 | 34 | 1 args (34 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$QUALITY_OF_LIVING` | 33 | 33 | 1 args (33 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$RAIL_SECONDARY_NOT_RENDER_ELETRIC` | 3 | 3 | 0 args (3 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$REPAIR_AREA` | 4 | 4 | 1 args (4 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$RESOURCE_FILLING_CONVEYOR_POINT` | 1 | 2 | 0 args (2 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_FILLING_POINT` | 1 | 2 | 3 args (2 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_INCREASE_CONVEYOR_POINT` | 6 | 15 | 1 args (15 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_INCREASE_POINT` | 19 | 36 | 3 args (3 occurrences), 4 args (33 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_PRICE_CALCULATION_INGORE` | 12 | 12 | 0 args (12 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_PRICE_CALCULATION_USE_REVERSE` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_ASPHALT` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_CONCRETE` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_COVERED` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_COVERED_ELECTRO` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_EXPLOSIVES` | 5 | 5 | 0 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_GRAVEL` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_OPEN` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_OPEN_BOARDS` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_OPEN_BRICKS` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_OPEN_PANELS` | 7 | 7 | 0 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_SEWAGE` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WASTE_ALUMINIUM` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WASTE_BIO` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WASTE_GENERAL` | 9 | 9 | 0 args (9 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WASTE_GRAVEL` | 9 | 9 | 0 args (9 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WASTE_PLASTIC` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WASTE_STEEL` | 9 | 9 | 0 args (9 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WASTE_TOXIC` | 9 | 9 | 0 args (9 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WATER` | 4 | 4 | 0 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_SOURCE_WORKERS` | 12 | 12 | 0 args (12 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_VISUALIZATION` | 79 | 389 | 1 args (389 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$RESOURCE_VISUALIZATION_WASTE_AUTO_STORAGE` | 23 | 23 | 4 args (23 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ROADVEHICLE_ELETRIC` | 12 | 12 | 0 args (12 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ROADVEHICLE_NOTFLIP` | 5 | 5 | 0 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$ROADVEHICLE_TRAM` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$SEASONAL_CLOSE_IF_TEMP_BELLOW` | 10 | 10 | 1 args (10 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$SEWAGE_DISABLE` | 3 | 3 | 0 args (3 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$SHIP_STATION` | 11 | 28 | 6 args (28 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$SOVIET_BUILDING_CONNECTION_TRAM_SPECIAL` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$STATION_NOT_BLOCK` | 91 | 91 | 0 args (91 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$STATION_NOT_BLOCK_DETOUR_POINT` | 31 | 95 | 3 args (95 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$STATION_NOT_BLOCK_DETOUR_POINT_PID` | 73 | 543 | 4 args (543 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$STORAGE` | 189 | 314 | 2 args (314 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_DEMAND_ADVANCED` | 4 | 4 | 2 args (4 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_DEMAND_BASIC` | 7 | 10 | 2 args (10 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_DEMAND_HOTEL` | 4 | 8 | 2 args (8 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_DEMAND_MEDIUMADVANCED` | 1 | 1 | 2 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_DEMAND_PRISON` | 2 | 4 | 2 args (4 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_EXPORT` | 71 | 81 | 2 args (81 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_EXPORT_SCRAPYARD` | 2 | 3 | 2 args (3 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_EXPORT_SPECIAL` | 9 | 19 | 3 args (19 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_EXPORT_SPECIAL_TRASH_BIG` | 10 | 11 | 3 args (11 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_FUEL` | 53 | 53 | 2 args (53 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_IMPORT` | 49 | 75 | 2 args (75 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_IMPORT_CARPLANT` | 9 | 18 | 2 args (18 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_IMPORT_SPECIAL` | 28 | 41 | 3 args (41 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_IMPORT_SPECIAL_TRASH_BIG` | 3 | 3 | 3 args (3 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_LIVING_AUTO` | 6 | 7 | 1 args (7 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_PACKCONTAINERS_FROM_STORAGE` | 5 | 5 | 1 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_SPECIAL` | 20 | 32 | 3 args (32 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_SPECIAL_TRASH_BIG` | 2 | 12 | 3 args (12 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_SPECIAL_TRASH_SMALL` | 2 | 10 | 3 args (10 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_TRASH_BIG` | 2 | 2 | 2 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_TRASH_BIG_FOR_STORAGE` | 2 | 45 | 2 args (45 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_TRASH_SMALL` | 4 | 4 | 2 args (4 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$STORAGE_UNPACKCONTAINERS_TO_STORAGE` | 4 | 4 | 1 args (4 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$STYLE_FLAG` | 151 | 151 | 1 args (151 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$SUBTYPE_AIR` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_AIRCUSTOM` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_AIRPLANE` | 11 | 11 | 0 args (11 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_CABLEWAY` | 14 | 14 | 0 args (14 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_HOSTEL` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_MEDICAL` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_METRO` | 10 | 10 | 0 args (10 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_OWN_CUSTOM` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_PRIORITY_1` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_RADIO` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_RAIL` | 3 | 3 | 0 args (3 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_RESTAURANT` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_ROAD` | 5 | 5 | 0 args (5 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_SHIP` | 10 | 10 | 0 args (10 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_SOVIET` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_SPACE_FOR_VEHICLES` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$SUBTYPE_TECHNICAL` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_TELEVISION` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_TRAM` | 6 | 6 | 0 args (6 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_TROLLEYBUS` | 1 | 1 | 0 args (1 occurrences) | No | INFERRED - type classification indicated by name; runtime meaning not established |
| `$SUBTYPE_WATER_SWITCH` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$SUPPLY_PEDESTRIAN` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$TEXT_CAPTION` | 245 | 245 | 0 args (245 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$TOKEN_POS_ROT_SCA` | 1 | 3 | 9 args (3 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$TREATMENT_CONSUMPTION` | 1 | 1 | 2 args (1 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$TURNPIKE` | 13 | 15 | 8 args (15 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$TYPE` | 2 | 677 | 1 args (677 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$TYPE_AIRPLANE_GATE` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_AIRPLANE_PARKING` | 7 | 7 | 0 args (7 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_AIRPLANE_TOWER` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_ATTRACTION` | 14 | 14 | 0 args (14 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_BROADCAST` | 4 | 4 | 0 args (4 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CARGO_STATION` | 43 | 43 | 0 args (43 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CAR_DEALER` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CHURCH` | 5 | 5 | 0 args (5 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CITYHALL` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CONSTRUCTION_OFFICE` | 7 | 7 | 0 args (7 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CONSTRUCTION_OFFICE_RAIL` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CONTAINER_FACILITY` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_COOLING_TOWER` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_COURT_HOUSE` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_CUSTOMHOUSE` | 5 | 5 | 0 args (5 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_DEMOLITION_OFFICE` | 5 | 5 | 0 args (5 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_DISTRIBUTION_OFFICE` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_DISTRIBUTION_OFFICE_RAIL` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_ELETRIC_EXPORT` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_ELETRIC_IMPORT` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_ENGINE` | 34 | 34 | 0 args (34 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_FACTORY` | 52 | 52 | 0 args (52 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_FARM` | 4 | 4 | 0 args (4 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_FIELD` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_FIRESTATION` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_FOREIGN_PIPELINE_EXPORT` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_FORKLIFT_GARAGE` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_GARBAGE_OFFICE` | 4 | 4 | 0 args (4 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_GAS_STATION` | 5 | 5 | 0 args (5 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_HEATING_ENDSTATION` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_HEATING_PLANT` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_HEATING_SWITCH` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_HOSPITAL` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_HOTEL` | 4 | 4 | 0 args (4 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_KINDERGARTEN` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_KINO` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_LIVING` | 54 | 54 | 0 args (54 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MINE_BAUXITE` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MINE_COAL` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MINE_GRAVEL` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MINE_IRON` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MINE_OIL` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MINE_URANIUM` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MINE_WATER` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_MINE_WATER_SURFACE` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_MINE_WOOD` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_MONUMENT` | 10 | 10 | 0 args (10 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_ORPHANAGE` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_PARKING` | 8 | 8 | 0 args (8 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_PASSANGER_STATION` | 25 | 25 | 0 args (25 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_PEDESTRIAN_BRIDGE` | 18 | 18 | 0 args (18 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_POLICE_STATION` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_POLLUTION_METER` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_POWERPLANT` | 9 | 9 | 0 args (9 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_PRISON` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_PRODUCTION_LINE` | 5 | 5 | 0 args (5 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_PUB` | 4 | 4 | 0 args (4 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_RAILDEPO` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_RAIL_TRAFO` | 6 | 6 | 0 args (6 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_REPAIR_OFFICE` | 4 | 4 | 0 args (4 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_ROADDEPO` | 6 | 6 | 0 args (6 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_SCHOOL` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_SCRAPYARD` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_SECRET_POLICE` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_SEWAGE_DISCHARGE` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_SEWAGE_ENDSTATION` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_SEWAGE_PUMP` | 5 | 5 | 0 args (5 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_SEWAGE_TREATMENT` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_SHIP_DOCK` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_SHOP` | 8 | 8 | 0 args (8 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_SPORT` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_STORAGE` | 27 | 27 | 0 args (27 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_SUBSTATION` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_TRAM_GATE` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_TRANSFORMATOR` | 7 | 7 | 0 args (7 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_TRASH_CONTAINER` | 6 | 6 | 0 args (6 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_UNIVERSITY` | 6 | 6 | 0 args (6 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_WAITING_STATION` | 7 | 7 | 0 args (7 occurrences) | Yes | INFERRED - type classification indicated by name; runtime meaning not established |
| `$TYPE_WATER_ENDSTATION` | 1 | 1 | 0 args (1 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_WATER_PUMP` | 7 | 7 | 0 args (7 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$TYPE_WATER_TREATMENT` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$UNDERGROUND_MESH` | 44 | 44 | 1 args (41 occurrences), 2 args (3 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$UNDERGROUND_MESH_DEPTH_OFFSET` | 13 | 13 | 1 args (13 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$VEHICLE_CANNOTSELECT_INSIDE` | 6 | 6 | 0 args (6 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_LOADING_FACTOR` | 32 | 32 | 1 args (32 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_PARKING` | 50 | 359 | 0 args (2 occurrences), 6 args (357 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_PARKING_ADVANCED_POINT` | 3 | 5 | 3 args (5 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_PARKING_ADVANCED_POINT_PID` | 28 | 771 | 4 args (771 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_PARKING_PERSONAL` | 14 | 175 | 0 args (1 occurrences), 6 args (174 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_PASSANGER_ONLY` | 2 | 2 | 0 args (2 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_STATION` | 238 | 558 | 0 args (63 occurrences), 5 args (1 occurrences), 6 args (494 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_STATION_NOT_BLOCK_ENTRY_POINT` | 4 | 4 | 3 args (2 occurrences), 4 args (2 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_STATION_NOT_BLOCK_ENTRY_POINT_PID` | 16 | 49 | 4 args (49 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_UNLOADINGLOADING_MAXVAGONS` | 1 | 1 | 1 args (1 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$VEHICLE_UNLOADING_FACTOR` | 28 | 28 | 1 args (28 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_CONSUMPTION` | 5 | 16 | 2 args (16 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_CONSUMPTION_RATIO` | 2 | 12 | 3 args (12 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_CUSTOMERS_DISABLE` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_DISABLE` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_EXTRACTION` | 4 | 10 | 2 args (10 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_PRODUCTION_ASH` | 4 | 4 | 0 args (4 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_PRODUCTION_DISABLE` | 23 | 23 | 0 args (23 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WASTE_WORKERS_DISABLE` | 7 | 7 | 0 args (7 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WATERSEWAGE_DISABLE` | 15 | 15 | 0 args (15 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$WATER_NOT_PRODUCE_SEWAGE_FROM_PRODUCTION` | 2 | 2 | 0 args (2 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WATER_NOT_USE_FOR_INDUSTRY_SUBSTATIONS` | 6 | 6 | 0 args (6 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WATER_STORAGE_FILL_FULL_SUBSTATION` | 3 | 3 | 0 args (3 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WATER_STORAGE_POSITION` | 17 | 17 | 1 args (17 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |
| `$WORKERS_NEEDED` | 156 | 156 | 1 args (156 occurrences) | Yes | VERIFIED - loader behavior documented in ledger |
| `$WORKER_RENDERING_AREA` | 24 | 118 | 0 args (118 occurrences) | No | INFERRED - name suggests domain; exact behavior not established |
| `$WORKING_SFX` | 76 | 76 | 1 args (76 occurrences) | No | INFERRED - lexical name only; behavior not established |
| `$WORKING_VEHICLES_NEEDED` | 66 | 66 | 1 args (66 occurrences) | Yes | INFERRED - name suggests domain; exact behavior not established |

## Ten highest-frequency directives still opaque to the parser

Ranked by total directive occurrences (ties use number of definitions). Mechanic hints are INFERRED from spelling only. This is prioritization input, not a claim about runtime behavior.

| Rank | Directive | Definitions | Occurrences | Inferred mechanic area |
|---:|---|---:|---:|---|
| 1 | `$CONNECTION_ADVANCED_POINT` | 158 | 2180 | network/access connectivity (inferred from name) |
| 2 | `$CONNECTION_ROAD_DEAD` | 282 | 1451 | network/access connectivity (inferred from name) |
| 3 | `$PARKING_NOT_BLOCK_DETOUR_POINT_PID` | 27 | 1126 | Other presentation or building behavior (inferred from name) |
| 4 | `$CONNECTION_PEDESTRIAN` | 301 | 899 | network/access connectivity (inferred from name) |
| 5 | `$VEHICLE_PARKING_ADVANCED_POINT_PID` | 28 | 771 | vehicle requirements, stations, parking, transport or logistics (inferred from name) |
| 6 | `$TYPE` | 2 | 677 | Other presentation or building behavior (inferred from name) |
| 7 | `$VEHICLE_STATION` | 238 | 558 | vehicle requirements, stations, parking, transport or logistics (inferred from name) |
| 8 | `$STATION_NOT_BLOCK_DETOUR_POINT_PID` | 73 | 543 | transport or service stations (inferred from name) |
| 9 | `$CONNECTIONS_ROAD_DEAD_SQUARE` | 277 | 475 | network/access connectivity (inferred from name) |
| 10 | `$CONNECTION_CONNECTION` | 114 | 454 | network/access connectivity (inferred from name) |

## Evidence boundary

The inventory is a static text survey of the local installation. It records directive spelling, occurrence, definition coverage and token shape. It does not reproduce source text or asset content. Static parser coverage is VERIFIED by OpenRepublic source and synthetic tests; corpus prevalence is OBSERVED for the scanned local corpus. Names and argument shapes do not establish units, field order semantics, loader use, runtime effects, or economy formulas. See the [compatibility catalog](COMPAT_CATALOG.md) and the building-definition loader section of the [reverse-engineering ledger](REVERSE_ENGINEERING_LEDGER.md).
