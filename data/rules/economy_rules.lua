-- Example economy tuning for OpenRepublicEconomyHost --rules (percent values, 10..1000).
-- Names must exist in the catalog. speed_percent shortens recipe intervals; storage_percent scales capacity.
return {
  schema_version = 1,
  buildings = {
    farm = { speed_percent = 150 },
    mill = { storage_percent = 200 },
  },
  vehicles = {
    truck = { capacity_percent = 200 },
  },
}
