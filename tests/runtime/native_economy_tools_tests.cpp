#include "openrepublic/runtime/native_economy_tools.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace openrepublic;

namespace {
int failures = 0;

void Check(bool condition, const std::string &label) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }
}

economy::BuildingSnapshot Building(economy::BuildingId id, economy::BuildingTypeId type) {
    economy::BuildingSnapshot building;
    building.id = id;
    building.type = type;
    return building;
}

const economy::BuildingSnapshot *FindBuilding(const economy::WorldSnapshot &snapshot,
                                               economy::BuildingId id) {
    for (const auto &building : snapshot.buildings) {
        if (building.id == id) return &building;
    }
    return nullptr;
}

const economy::InventorySnapshot *FindInventory(const economy::BuildingSnapshot &building,
                                                 economy::ResourceId resource) {
    for (const auto &slot : building.inventory) {
        if (slot.resource == resource) return &slot;
    }
    return nullptr;
}

economy::Catalog MakeStorageCatalog() {
    economy::CatalogBuilder builder;
    economy::CatalogErrorInfo error;
    economy::ResourceId grain{}, flour{}, bread{}, steel{};
    economy::BuildingTypeId mixed{}, storageOnly{}, other{}, empty{};
    Check(builder.AddResource("grain", &grain, error), "add grain");
    Check(builder.AddResource("flour", &flour, error), "add flour");
    Check(builder.AddResource("bread", &bread, error), "add bread");
    Check(builder.AddResource("steel", &steel, error), "add steel");
    Check(builder.AddBuildingType("mixed", 1, 1, &mixed, error), "add mixed type");
    Check(builder.AddStorage(mixed, steel, 8, error) && builder.AddStorage(mixed, bread, 7, error) &&
              builder.AddStorage(mixed, grain, 6, error) && builder.AddStorage(mixed, flour, 5, error),
          "add mixed storages in non-ID order");
    Check(builder.AddRecipe(mixed, {"bake", 2, {{grain, 1}}, {{bread, 1}}}, error) &&
              builder.AddRecipe(mixed, {"mill", 3, {{grain, 1}}, {{flour, 1}}}, error),
          "add multiple recipes");
    Check(builder.AddBuildingType("storage-only", 1, 1, &storageOnly, error), "add storage-only type");
    Check(builder.AddStorage(storageOnly, bread, 20, error) &&
              builder.AddStorage(storageOnly, grain, 10, error),
          "add storage-only resources");
    Check(builder.AddBuildingType("other", 1, 1, &other, error), "add other type");
    Check(builder.AddStorage(other, flour, 4, error), "add other storage");
    Check(builder.AddBuildingType("empty", 1, 1, &empty, error), "add empty type");
    economy::Catalog catalog;
    Check(builder.Build(catalog, error), "build helper catalog: " + error.message);
    return catalog;
}

void AddRoadLine(economy::World &world, int x0, int x1, int z) {
    for (int x = x0; x < x1; ++x) world.BuildRoad({x, z}, {x + 1, z});
}

} // namespace

int main() {
    {
        runtime::NativeEconomyTransform transform{10.25, -4.5};
        economy::Cell out{91, 92};
        const auto cellCenter = transform.CellCenter({5, 7});
        Check(runtime::NativeDisplayPointToCell(transform, cellCenter.x, cellCenter.z, 20, 20, out) &&
                  out == economy::Cell{5, 7},
              "cell center round-trips through a nonzero fractional transform");

        const double leftX = (0.0 - transform.centerCellX) * 4.0;
        const double leftZ = (0.0 - transform.centerCellZ) * 4.0;
        Check(runtime::NativeDisplayPointToCell(transform, leftX, leftZ, 8, 9, out) &&
                  out == economy::Cell{0, 0},
              "left and top half-open boundaries belong to cell zero");
        Check(runtime::NativeDisplayPointToCell(transform, leftX + 4.0, leftZ + 4.0,
                                                8, 9, out) && out == economy::Cell{1, 1},
              "right and bottom boundaries belong to the adjacent cells");
        Check(runtime::NativeDisplayPointToCell({4.0, 3.0}, -6.0, -6.0, 8, 8, out) &&
                  out == economy::Cell{2, 1},
              "negative display coordinates invert correctly with nonzero centers");

        runtime::NativeEconomyTransform huge{1.0e12, 8.0e11};
        const auto hugePoint = huge.CellCenter({123, 456});
        Check(runtime::NativeDisplayPointToCell(huge, hugePoint.x, hugePoint.z, 1000, 1000, out) &&
                  out == economy::Cell{123, 456},
              "large finite transform origins retain double-precision inverse mapping");

        out = {91, 92};
        Check(!runtime::NativeDisplayPointToCell(transform, std::numeric_limits<double>::quiet_NaN(),
                                                  0.0, 8, 9, out) && out == economy::Cell{91, 92},
              "NaN fails transactionally");
        Check(!runtime::NativeDisplayPointToCell(transform, 0.0,
                                                  std::numeric_limits<double>::infinity(),
                                                  8, 9, out) && out == economy::Cell{91, 92},
              "infinity fails transactionally");
        Check(!runtime::NativeDisplayPointToCell({std::numeric_limits<double>::infinity(), 0.0},
                                                  0.0, 0.0, 8, 9, out) &&
                  out == economy::Cell{91, 92},
              "non-finite transform origin fails transactionally");
        Check(!runtime::NativeDisplayPointToCell(transform, leftX - 0.01, leftZ, 8, 9, out) &&
                  out == economy::Cell{91, 92},
              "point beyond lower grid edge fails transactionally");
        Check(!runtime::NativeDisplayPointToCell(transform, leftX + 8.0 * 4.0, leftZ,
                                                  8, 9, out) && out == economy::Cell{91, 92},
              "upper half-open grid edge is outside");
        Check(!runtime::NativeDisplayPointToCell(transform, 0.0, 0.0, 0, 9, out) &&
                  out == economy::Cell{91, 92},
              "zero grid dimension is invalid");
        Check(!runtime::NativeDisplayPointToCell(transform, 0.0, 0.0,
                                                  static_cast<std::uint32_t>(economy::MaxWorldGridSize) + 1,
                                                  9, out) && out == economy::Cell{91, 92},
              "dimensions beyond signed native bounds are invalid");
        Check(!runtime::NativeDisplayPointToCell(transform,
                                                  (static_cast<double>(economy::MaxWorldGridSize) + 1.0 -
                                                   transform.centerCellX) * 4.0,
                                                  leftZ, economy::MaxWorldGridSize, 9, out) &&
                  out == economy::Cell{91, 92},
              "computed coordinate outside native signed range is rejected");
    }

    const auto catalog = MakeStorageCatalog();
    {
        economy::WorldSnapshot snapshot;
        // IDs are intentionally gapped and vector order differs from ID order.
        snapshot.buildings = {Building(90, 1), Building(7, 0), Building(31, 2), Building(44, 3)};
        Check(snapshot.buildings[0].inventory.empty() && snapshot.buildings[1].inventory.empty(),
              "resource choices are queried before any inventory exists");
        const auto resources = runtime::ListNativeHaulResources(snapshot, catalog, 7, 90);
        Check(resources == std::vector<economy::ResourceId>({0, 2}),
              "endpoint storage intersection is stable by resource ID, independent of recipes");
        Check(runtime::ListNativeHaulResources(snapshot, catalog, 90, 7) ==
                  std::vector<economy::ResourceId>({0, 2}),
              "storage-only endpoint can haul resources accepted by a recipe building");
        Check(runtime::ListNativeHaulResources(snapshot, catalog, 90, 31).empty(),
              "resources absent from either endpoint do not appear");
        Check(runtime::ListNativeHaulResources(snapshot, catalog, 7, 44).empty(),
              "endpoint with no positive storage has no haul resources");
        Check(runtime::ListNativeHaulResources(snapshot, catalog, 0, 90).empty(),
              "zero endpoint ID is invalid");
        Check(runtime::ListNativeHaulResources(snapshot, catalog, 7, 7).empty(),
              "same source and destination are invalid");
        Check(runtime::ListNativeHaulResources(snapshot, catalog, 7, 999).empty(),
              "missing endpoint is rejected");
        snapshot.buildings.push_back(Building(7, 0));
        Check(runtime::ListNativeHaulResources(snapshot, catalog, 7, 90).empty(),
              "ambiguous duplicate exact endpoint ID is rejected");
    }

    {
        const economy::Catalog synthetic = economy::MakeSyntheticCatalog();
        economy::ResourceId grain{}, flour{}, bread{};
        economy::BuildingTypeId farm{}, mill{}, bakery{}, market{};
        economy::VehicleTypeId truck{};
        Check(synthetic.FindResource("grain", grain) && synthetic.FindResource("flour", flour) &&
                  synthetic.FindResource("bread", bread),
              "resolve synthetic chain resources");
        Check(synthetic.FindBuildingType("farm", farm) && synthetic.FindBuildingType("mill", mill) &&
                  synthetic.FindBuildingType("bakery", bakery) && synthetic.FindBuildingType("market", market) &&
                  synthetic.FindVehicleType("truck", truck),
              "resolve synthetic chain types");
        economy::World world(synthetic, 20, 8);
        AddRoadLine(world, 1, 14, 1);
        // A vertical branch exercises the other road axis while retaining the chain route.
        world.BuildRoad({2, 0}, {2, 1});
        world.PlaceBuilding(farm, {1, 2});
        world.PlaceBuilding(mill, {4, 2});
        world.PlaceBuilding(bakery, {7, 2});
        world.PlaceBuilding(market, {10, 2});
        world.Step();
        const auto placed = world.Snapshot();
        Check(placed.roads.size() == 14 && placed.buildings.size() == 4,
              "synthetic chain fixture places buildings and both road axes");
        const std::vector<std::pair<economy::BuildingId, economy::BuildingId>> endpoints{
            {1, 2}, {2, 3}, {3, 4}};
        const std::vector<economy::ResourceId> expected{grain, flour, bread};
        for (std::size_t i = 0; i < endpoints.size(); ++i) {
            const auto available = runtime::ListNativeHaulResources(placed, synthetic,
                                                                      endpoints[i].first,
                                                                      endpoints[i].second);
            Check(available == std::vector<economy::ResourceId>{expected[i]},
                  "storage helper selects the intended synthetic chain resource");
            if (!available.empty()) world.CreateHaulOrder(endpoints[i].first, endpoints[i].second,
                                                           available.front(), truck);
        }
        world.Step();
        Check(world.Snapshot().orders.size() == 3,
              "selected storage intersections create three core haul orders");
        Check(world.Step(300).accepted, "synthetic chain advances through core simulation");
        const auto beforeSave = world.Snapshot();
        const auto *marketState = FindBuilding(beforeSave, 4);
        const auto *bakeryState = FindBuilding(beforeSave, 3);
        Check(marketState && !marketState->recipes.empty() &&
                  marketState->recipes[0].completedCycles > 0,
              "Farm to Mill to Bakery to Market chain reaches the consuming market");
        Check(bakeryState && !bakeryState->recipes.empty() &&
                  bakeryState->recipes[0].completedCycles > 0,
              "bakery completes production based on helper-selected haul resources");
        const auto *farmState = FindBuilding(beforeSave, 1);
        const auto *millState = FindBuilding(beforeSave, 2);
        Check(farmState && FindInventory(*farmState, grain) && millState &&
                  FindInventory(*millState, flour) && bakeryState && FindInventory(*bakeryState, bread),
              "chain snapshots expose expected catalog resources at each production stage");

        const std::string saved = economy::SaveWorld(world);
        Check(!saved.empty(), "save synthetic resource chain mid-run");
        economy::World restored(synthetic, 1, 1);
        economy::ParseError parseError;
        Check(economy::LoadWorld(saved, synthetic, restored, parseError),
              "load synthetic chain save: " + parseError.message);
        for (int tick = 0; tick < 24; ++tick) {
            Check(world.Step().accepted && restored.Step().accepted,
                  "original and restored chain accept continuation tick");
            Check(world.Snapshot().stateHash == restored.Snapshot().stateHash,
                  "save/reload continuation matches every chain hash");
        }
    }

    if (failures != 0) {
        std::cerr << failures << " native economy tools test(s) failed\n";
        return 1;
    }
    std::cout << "native economy tools tests passed\n";
    return 0;
}
