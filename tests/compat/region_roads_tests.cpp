#include "openrepublic/compat/region_import.h"
#include "openrepublic/compat/region_roads.h"
#include "openrepublic/economy/economy.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace openrepublic;

namespace {

int failures = 0;

void Check(bool condition, const char *message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

std::uint64_t Amount(const economy::BuildingSnapshot &building, economy::ResourceId resource) {
    for (const auto &slot : building.inventory) {
        if (slot.resource == resource) return slot.amount;
    }
    return 0;
}

} // namespace

int main() {
    const economy::Catalog catalog = economy::MakeSyntheticCatalog();
    const std::vector<compat::PlacedBuildingRef> placements = {
        {"farm", 0.0, 0.0},
        {"mill", 80.0, 0.0},
    };
    compat::RegionImportOptions options;
    options.radiusMeters = 200.0;
    std::unique_ptr<economy::World> world;
    compat::RegionImportReport imported;
    Check(compat::ImportRegionIntoWorld(placements, 0, catalog, options, world, imported),
          "the two buildings import");
    Check(imported.placed == 2, "both buildings are placed");

    // A chord that runs beside both buildings, with a stub far outside the grid.
    const std::vector<compat::RoadLinkRef> links = {
        {-12.0, -10.0, 92.0, -10.0},
        {5000.0, 5000.0, 5100.0, 5000.0},
    };
    compat::RegionRoadReport roads;
    Check(compat::ImportRoadsIntoWorld(*world, imported, options, links, {}, roads), "roads import");
    Check(roads.linksConsidered == 2 && roads.linksOutsideGrid == 1, "the far link is counted as outside");
    Check(roads.segmentsAccepted > 20 && roads.segmentsRejected == 0, "the chord becomes unit segments");
    Check(roads.buildingsConnected + roads.buildingsOnRoad == 2 && roads.buildingsUnconnected == 0,
          "both buildings reach the road");

    // A haul order over the imported layout delivers grain from farm to mill.
    economy::ResourceId grain = 0;
    economy::VehicleTypeId truck = 0;
    Check(catalog.FindResource("grain", grain) && catalog.FindVehicleType("truck", truck), "catalog names");
    const economy::CommandId order =
        world->CreateHaulOrder(imported.placedBuildings[0].second, imported.placedBuildings[1].second, grain, truck);
    Check(world->Step(400).accepted, "the world steps");
    Check(world->GetCommandResult(order).state == economy::CommandState::Accepted, "the haul order is accepted");
    const economy::WorldSnapshot snapshot = world->Snapshot();
    std::uint64_t millGrainOrFlour = 0;
    for (const auto &building : snapshot.buildings) {
        if (building.id == imported.placedBuildings[1].second) {
            economy::ResourceId flour = 0;
            catalog.FindResource("flour", flour);
            millGrainOrFlour = Amount(building, grain) + Amount(building, flour);
        }
    }
    Check(millGrainOrFlour > 0, "grain was hauled over the rasterized road and the mill produced");

    // Determinism: the same import twice yields the same world hash.
    std::unique_ptr<economy::World> again;
    compat::RegionImportReport importedAgain;
    compat::RegionRoadReport roadsAgain;
    compat::ImportRegionIntoWorld(placements, 0, catalog, options, again, importedAgain);
    compat::ImportRoadsIntoWorld(*again, importedAgain, options, links, {}, roadsAgain);
    std::unique_ptr<economy::World> third;
    compat::RegionImportReport importedThird;
    compat::RegionRoadReport roadsThird;
    compat::ImportRegionIntoWorld(placements, 0, catalog, options, third, importedThird);
    compat::ImportRoadsIntoWorld(*third, importedThird, options, links, {}, roadsThird);
    Check(again->Snapshot().stateHash == third->Snapshot().stateHash, "road import is deterministic");
    Check(compat::DescribeRegionRoads(roads) == compat::DescribeRegionRoads(roadsAgain), "report is deterministic");

    if (failures != 0) {
        std::cerr << failures << " region-road test(s) failed\n";
        return 1;
    }
    std::cout << "All region-road tests passed\n";
    return 0;
}
