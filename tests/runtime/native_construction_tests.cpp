#include "openrepublic/runtime/native_construction.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic;
namespace {
void Check(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }
}
int main() {
    economy::CatalogBuilder builder; economy::CatalogErrorInfo error;
    economy::ResourceId resource{}; economy::BuildingTypeId one{}, wide{};
    Check(builder.AddResource("ore", &resource, error), "resource");
    Check(builder.AddBuildingType("one", 1, 1, &one, error), "one-cell type");
    Check(builder.AddBuildingType("wide", 2, 2, &wide, error), "wide type");
    economy::Catalog catalog; Check(builder.Build(catalog, error), "build catalog");
    economy::WorldSnapshot snapshot; snapshot.gridWidth = 8; snapshot.gridDepth = 8;
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, one, {2, 2}).Valid(), "free placement");
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, wide, {2, 2}).Valid(), "wide free placement");
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, one, {0, 0}).failure == runtime::PlacementFailure::AccessCellUnavailable, "access at north edge");
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, wide, {7, 2}).failure == runtime::PlacementFailure::OutOfBounds, "footprint bounds");
    snapshot.buildings.push_back({10, one, {3, 3}, true, {3, 2}});
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, one, {3, 3}).failure == runtime::PlacementFailure::BuildingCollision, "building overlap");
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, one, {3, 2}).failure == runtime::PlacementFailure::AccessCellUnavailable, "reserved existing access");
    snapshot.buildings.clear(); snapshot.roads.push_back({1, {3, 3}, {4, 3}, true});
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, one, {3, 3}).failure == runtime::PlacementFailure::RoadOnBuilding, "road overlap");
    snapshot.roads.clear(); snapshot.rails.push_back({1, {3, 3}, {4, 3}});
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, one, {3, 3}).failure == runtime::PlacementFailure::RailOnBuilding, "rail overlap");
    Check(runtime::ValidateNativeBuildingPlacement(snapshot, catalog, 99, {2, 2}).failure == runtime::PlacementFailure::UnknownBuildingType, "unknown type");
    std::cout << "native construction placement checks passed\n";
}
