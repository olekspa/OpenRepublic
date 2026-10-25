#include "openrepublic/runtime/native_economy_layout.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

using namespace openrepublic;

namespace {

int failures = 0;

void Check(bool condition, const std::string &label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

economy::Catalog MakeCatalog() {
    economy::CatalogBuilder builder;
    economy::CatalogErrorInfo error;
    economy::ResourceId grain = economy::InvalidResourceId;
    economy::BuildingTypeId small = economy::InvalidBuildingTypeId;
    economy::BuildingTypeId wide = economy::InvalidBuildingTypeId;
    economy::VehicleTypeId truck = economy::InvalidVehicleTypeId;
    Check(builder.AddResource("grain", &grain, error), "add test resource");
    Check(builder.AddBuildingType("small", 1, 1, &small, error), "add small type");
    Check(builder.AddStorage(small, grain, 20, error), "add small storage");
    Check(builder.AddBuildingType("wide", 3, 2, &wide, error), "add wide type");
    Check(builder.AddStorage(wide, grain, 40, error), "add wide storage");
    Check(builder.AddVehicleType("truck", 4, 1, &truck, error), "add truck type");
    economy::Catalog catalog;
    Check(builder.Build(catalog, error), "build test catalog: " + error.message);
    return catalog;
}

economy::BuildingSnapshot Building(economy::BuildingId id, economy::BuildingTypeId type,
                                   economy::Cell origin, economy::Cell access) {
    economy::BuildingSnapshot result;
    result.id = id;
    result.type = type;
    result.origin = origin;
    result.access = access;
    return result;
}

bool Near(double a, double b, double tolerance = 1e-9) {
    return std::abs(a - b) <= tolerance;
}

} // namespace

int main() {
    const economy::Catalog catalog = MakeCatalog();
    std::string error;
    runtime::NativeEconomyScenePlan plan;

    economy::WorldSnapshot snapshot;
    snapshot.gridWidth = 128;
    snapshot.gridDepth = 96;
    // Reordered, gapped IDs and repeated types are all valid. Position order does not
    // become an identity binding: plan primitives are sorted by their native IDs.
    snapshot.buildings.push_back(Building(900000, 1, {40, 20}, {40, 19}));
    snapshot.buildings.push_back(Building(7, 0, {10, 12}, {10, 11}));
    snapshot.buildings.push_back(Building(500, 1, {20, 12}, {20, 11}));
    snapshot.roads.push_back({4000, {40, 19}, {40, 20}, false});
    snapshot.roads.push_back({3, {10, 11}, {11, 11}, true});
    snapshot.vehicles.push_back({900, 45, 0, {45, 19}, 2, economy::HaulState::Waiting});

    Check(runtime::BuildNativeEconomyScenePlan(snapshot, catalog, plan, error),
          "build full native plan: " + error);
    Check(plan.buildings.size() == 3 && plan.roads.size() == 2,
          "all buildings and roads are represented");
    Check(plan.buildings[0].buildingId == 7 && plan.buildings[1].buildingId == 500 &&
              plan.buildings[2].buildingId == 900000,
          "plan output order is stable by ID");
    Check(plan.buildings[0].typeId == plan.buildings[1].typeId ||
              plan.buildings[1].typeId == plan.buildings[2].typeId,
          "same catalog type can appear on multiple native buildings");
    Check(plan.roads[0].roadId == 3 && plan.roads[1].roadId == 4000,
          "road output order is stable by ID");
    Check(!plan.roads[1].enabled, "disabled roads remain visible with their state");
    Check(Near(plan.buildings[0].width, 4.0) && Near(plan.buildings[0].depth, 4.0),
          "one-cell footprint uses full four-unit sizes");
    Check(Near(plan.buildings[2].width, 12.0) && Near(plan.buildings[2].depth, 8.0),
          "wide footprint dimensions use full display sizes");
    Check(plan.buildings[0].height >= 2.0 && plan.buildings[0].height <= 12.0 &&
              plan.buildings[2].height >= plan.buildings[0].height && plan.buildings[2].height <= 12.0,
          "display height policy is bounded and footprint-stable");
    Check(Near(plan.roads[0].width, 4.0) && Near(plan.roads[0].depth, 1.4),
          "X road spans one cell with 0.35-cell thickness");
    Check(Near(plan.roads[1].width, 1.4) && Near(plan.roads[1].depth, 4.0),
          "Z road spans one cell with 0.35-cell thickness");
    Check(Near(plan.transform.centerCellX, 28.0) && Near(plan.transform.centerCellZ, 16.5),
          "initial transform includes footprint, access, road, and vehicle occupancy (" +
              std::to_string(plan.transform.centerCellX) + "," +
              std::to_string(plan.transform.centerCellZ) + ")");
    Check(plan.minX < plan.maxX && plan.minZ < plan.maxZ && plan.cameraDistance >= 16.0,
          "plan includes valid camera bounds and distance");
    for (const auto &building : plan.buildings) {
        Check(building.centerX - building.width * 0.5 >= plan.minX - 1e-9 &&
                  building.centerX + building.width * 0.5 <= plan.maxX + 1e-9 &&
                  building.centerZ - building.depth * 0.5 >= plan.minZ - 1e-9 &&
                  building.centerZ + building.depth * 0.5 <= plan.maxZ + 1e-9,
              "camera bounds contain each complete building footprint");
    }
    for (const auto &road : plan.roads) {
        Check(road.centerX - road.width * 0.5 >= plan.minX - 1e-9 &&
                  road.centerX + road.width * 0.5 <= plan.maxX + 1e-9 &&
                  road.centerZ - road.depth * 0.5 >= plan.minZ - 1e-9 &&
                  road.centerZ + road.depth * 0.5 <= plan.maxZ + 1e-9,
              "camera bounds contain each complete road segment");
    }
    Check(runtime::NativeEconomyPlanFitsFloatPrecision(plan, error),
          "ordinary native scene fits float precision: " + error);

    runtime::NativeEconomyScenePlan originalPlan;
    snapshot.gridWidth = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    snapshot.gridDepth = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    snapshot.buildings.clear();
    snapshot.roads.clear();
    snapshot.vehicles.clear();
    snapshot.buildings.push_back(Building(4000000000u, 0, {2100000000, 2100000000},
                                           {2100000000, 2099999999}));
    snapshot.buildings.push_back(Building(12, 0, {2100000001, 2100000000},
                                           {2100000001, 2099999999}));
    snapshot.roads.push_back({800000000u, {2100000000, 2099999999},
                              {2100000001, 2099999999}, true});
    snapshot.vehicles.push_back({5, 1, 0, {2100000000, 2099999999}, 0,
                                 economy::HaulState::Waiting});
    Check(runtime::BuildNativeEconomyScenePlan(snapshot, catalog, originalPlan, error),
          "high coordinate small-span plan: " + error);
    Check(std::isfinite(originalPlan.buildings[0].centerX) &&
              Near(originalPlan.buildings[1].centerX - originalPlan.buildings[0].centerX, -4.0),
          "large origins preserve small adjacent-cell precision");
    Check(Near(originalPlan.roads[0].centerZ,
                originalPlan.transform.CellCenter({2100000000, 2099999999}).z),
          "road and truck cell positions share one transform convention");
    Check(Near(originalPlan.transform.centerCellX, 2100000001.0),
          "center transform is computed in double across large native coordinates");
    Check(runtime::NativeEconomyPlanFitsFloatPrecision(originalPlan, error),
          "freshly centered large-origin cluster preserves adjacent cells: " + error);

    const runtime::NativeEconomyTransform fixed = originalPlan.transform;
    const double previousX = originalPlan.buildings.front().centerX;
    snapshot.buildings.push_back(Building(999999, 0, {2100000010, 2100000000},
                                           {2100000010, 2099999999}));
    runtime::NativeEconomyScenePlan fixedPlan;
    Check(runtime::BuildNativeEconomyScenePlan(snapshot, catalog, fixedPlan, error, &fixed),
          "fixed-transform structural update: " + error);
    Check(fixedPlan.transform.centerCellX == fixed.centerCellX &&
              fixedPlan.transform.centerCellZ == fixed.centerCellZ &&
              Near(fixedPlan.buildings.front().centerX, previousX),
          "fixed transform does not recenter when the world gains buildings");

    economy::World changingWorld(catalog, 32, 32);
    Check(changingWorld.PlaceBuilding(0, {2, 2}) != 0 && changingWorld.Step().accepted,
          "initial native world structure is accepted");
    const auto beforeCommands = changingWorld.Snapshot();
    runtime::NativeEconomyScenePlan beforeCommandPlan;
    Check(runtime::BuildNativeEconomyScenePlan(beforeCommands, catalog, beforeCommandPlan, error),
          "plan initial command-driven world: " + error);
    const auto stableTransform = beforeCommandPlan.transform;
    const double stableBuildingX = beforeCommandPlan.buildings.front().centerX;
    Check(changingWorld.PlaceBuilding(1, {20, 20}) != 0 &&
              changingWorld.BuildRoad({20, 19}, {21, 19}) != 0 && changingWorld.Step().accepted,
          "queued building and road commands apply together");
    const auto afterCommands = changingWorld.Snapshot();
    runtime::NativeEconomyScenePlan afterCommandPlan;
    Check(runtime::BuildNativeEconomyScenePlan(afterCommands, catalog, afterCommandPlan, error,
                                                &stableTransform),
          "plan world after structural commands: " + error);
    Check(afterCommandPlan.buildings.size() == 2 && afterCommandPlan.roads.size() == 1 &&
              afterCommandPlan.transform.centerCellX == stableTransform.centerCellX &&
              afterCommandPlan.transform.centerCellZ == stableTransform.centerCellZ &&
              Near(afterCommandPlan.buildings.front().centerX, stableBuildingX),
          "accepted structural commands change the plan without moving the pinned transform");

    economy::WorldSnapshot empty;
    empty.gridWidth = 4;
    empty.gridDepth = 30;
    runtime::NativeEconomyScenePlan emptyPlan;
    Check(runtime::BuildNativeEconomyScenePlan(empty, catalog, emptyPlan, error),
          "empty-world plan: " + error);
    Check(Near(emptyPlan.transform.centerCellX, 2.0) && Near(emptyPlan.transform.centerCellZ, 8.0),
          "empty world centers the first at-most-16 cells");
    Check(Near(emptyPlan.minX, -8.0) && Near(emptyPlan.maxX, 8.0) &&
              Near(emptyPlan.minZ, -32.0) && Near(emptyPlan.maxZ, 32.0),
          "empty-world camera bounds describe the initial visible cells");
    Check(runtime::NativeEconomyPlanFitsFloatPrecision(emptyPlan, error),
          "empty-world initial view fits float precision: " + error);

    economy::World precisionWorld(catalog, economy::MaxWorldGridSize,
                                  economy::MaxWorldGridSize);
    const auto initialEmpty = precisionWorld.Snapshot();
    runtime::NativeEconomyScenePlan initialPrecisionPlan;
    Check(runtime::BuildNativeEconomyScenePlan(initialEmpty, catalog, initialPrecisionPlan, error),
          "maximum-grid empty plan: " + error);
    const auto initialTransform = initialPrecisionPlan.transform;
    constexpr std::int32_t farX = std::numeric_limits<std::int32_t>::max() - 3;
    Check(precisionWorld.PlaceBuilding(0, {farX, 10}) != 0 &&
              precisionWorld.PlaceBuilding(0, {farX + 1, 10}) != 0 &&
              precisionWorld.Step().accepted,
          "adjacent buildings are accepted near the maximum grid coordinate");
    const auto farSnapshot = precisionWorld.Snapshot();
    runtime::NativeEconomyScenePlan pinnedFarPlan;
    Check(runtime::BuildNativeEconomyScenePlan(farSnapshot, catalog, pinnedFarPlan, error,
                                                &initialTransform),
          "large-span pinned plan remains valid in double: " + error);
    const auto pinnedCenter = pinnedFarPlan.buildings.front().centerX;
    Check(!runtime::NativeEconomyPlanFitsFloatPrecision(pinnedFarPlan, error) &&
              !error.empty() && pinnedFarPlan.buildings.front().centerX == pinnedCenter,
          "pinned far-away plan rejects lossy float conversion without mutation");
    runtime::NativeEconomyScenePlan recenteredFarPlan;
    Check(runtime::BuildNativeEconomyScenePlan(farSnapshot, catalog, recenteredFarPlan, error),
          "recentered far-away cluster plan remains valid in double: " + error);
    Check(runtime::NativeEconomyPlanFitsFloatPrecision(recenteredFarPlan, error),
          "recentered far-away adjacent buildings fit float precision: " + error);

    economy::WorldSnapshot fullSpan = farSnapshot;
    fullSpan.buildings.push_back(Building(9000000, 0, {1, 10}, {1, 9}));
    runtime::NativeEconomyScenePlan fullSpanPlan;
    Check(runtime::BuildNativeEconomyScenePlan(fullSpan, catalog, fullSpanPlan, error),
          "full-span double plan remains constructible: " + error);
    Check(!runtime::NativeEconomyPlanFitsFloatPrecision(fullSpanPlan, error) &&
              error.find("float") != std::string::npos,
          "genuinely huge scene span is rejected at float-render boundary");

    auto expectInvalid = [&](economy::WorldSnapshot invalid, const std::string &label) {
        runtime::NativeEconomyScenePlan sentinel;
        sentinel.cameraDistance = 123.0;
        const bool accepted = runtime::BuildNativeEconomyScenePlan(invalid, catalog, sentinel, error);
        Check(!accepted && !error.empty() && sentinel.cameraDistance == 123.0,
              label + " rejects transactionally");
    };
    economy::WorldSnapshot invalid;
    invalid.gridWidth = 8;
    invalid.gridDepth = 8;
    invalid.buildings = {Building(1, 0, {1, 1}, {1, 0}), Building(1, 0, {3, 1}, {3, 0})};
    expectInvalid(invalid, "duplicate building IDs");
    invalid.buildings = {Building(1, 200, {1, 1}, {1, 0})};
    expectInvalid(invalid, "unknown building type");
    invalid.buildings = {Building(1, 1, {6, 1}, {6, 0})};
    expectInvalid(invalid, "footprint crossing grid edge");
    invalid.buildings = {Building(1, 0, {-1, 1}, {1, 0})};
    expectInvalid(invalid, "negative building origin");
    invalid.buildings.clear();
    invalid.roads = {{1, {1, 1}, {3, 1}, true}};
    expectInvalid(invalid, "non-unit road segment");
    invalid.roads = {{1, {1, 1}, {2, 1}, true}, {1, {2, 1}, {3, 1}, true}};
    expectInvalid(invalid, "duplicate road IDs");
    invalid.roads.clear();
    invalid.vehicles = {{1, 1, 200, {1, 1}, 0, economy::HaulState::Waiting}};
    expectInvalid(invalid, "unknown vehicle type");
    invalid.vehicles = {{1, 1, 0, {8, 1}, 0, economy::HaulState::Waiting}};
    expectInvalid(invalid, "vehicle outside grid");
    invalid.vehicles.clear();
    invalid.gridWidth = 0;
    expectInvalid(invalid, "zero grid width");
    invalid.gridWidth = 8;
    const runtime::NativeEconomyTransform badTransform{
        std::numeric_limits<double>::quiet_NaN(), 0.0};
    runtime::NativeEconomyScenePlan unchanged;
    unchanged.cameraDistance = 456.0;
    Check(!runtime::BuildNativeEconomyScenePlan(invalid, catalog, unchanged, error, &badTransform) &&
              unchanged.cameraDistance == 456.0,
          "non-finite fixed transform rejects without mutating output");
    const runtime::NativeEconomyTransform overflowingTransform{1.0e308, -1.0e308};
    Check(!runtime::BuildNativeEconomyScenePlan(empty, catalog, unchanged, error,
                                                 &overflowingTransform) &&
              unchanged.cameraDistance == 456.0,
          "finite but overflowing fixed transform rejects without mutating output");

    {
        economy::WorldSnapshot sites;
        sites.gridWidth = 32;
        sites.gridDepth = 32;
        sites.buildings.push_back(Building(1, 1, {4, 4}, {4, 3}));
        sites.buildings.push_back(Building(2, 1, {12, 4}, {12, 3}));
        sites.buildings.push_back(Building(3, 1, {20, 4}, {20, 3}));
        sites.buildings[1].constructionSite = true;
        sites.buildings[1].constructionState = 0;
        sites.buildings[1].constructionProgress = 0.0;
        sites.buildings[2].constructionSite = true;
        sites.buildings[2].constructionState = 1;
        sites.buildings[2].constructionProgress = 0.5;
        runtime::NativeEconomyScenePlan sitePlan;
        Check(runtime::BuildNativeEconomyScenePlan(sites, catalog, sitePlan, error), "site plan builds: " + error);
        const double full = sitePlan.buildings[0].height;
        Check(Near(sitePlan.buildings[1].height, full * 0.25) && Near(sitePlan.buildings[2].height, full * 0.625),
              "construction sites render at a progress-scaled height");
        sites.buildings[1].constructionState = 2;
        Check(runtime::BuildNativeEconomyScenePlan(sites, catalog, sitePlan, error) &&
                  Near(sitePlan.buildings[1].height, full),
              "a finished site renders at full height");
    }

    if (failures != 0) {
        std::cerr << failures << " native-economy-layout test(s) failed\n";
        return 1;
    }
    std::cout << "All native-economy-layout tests passed\n";
    return 0;
}
