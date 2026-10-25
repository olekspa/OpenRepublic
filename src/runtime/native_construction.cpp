#include "openrepublic/runtime/native_construction.h"

namespace openrepublic::runtime {
namespace {
bool InFootprint(const economy::BuildingTypeDef& type, economy::Cell origin, economy::Cell cell) noexcept {
    return cell.x >= origin.x && cell.z >= origin.z &&
        static_cast<std::int64_t>(cell.x) < static_cast<std::int64_t>(origin.x) + type.width &&
        static_cast<std::int64_t>(cell.z) < static_cast<std::int64_t>(origin.z) + type.depth;
}
bool InGrid(economy::Cell cell, const economy::WorldSnapshot& snapshot) noexcept {
    return cell.x >= 0 && cell.z >= 0 && static_cast<std::uint32_t>(cell.x) < snapshot.gridWidth &&
        static_cast<std::uint32_t>(cell.z) < snapshot.gridDepth;
}
}

PlacementValidation ValidateNativeBuildingPlacement(const economy::WorldSnapshot& snapshot,
    const economy::Catalog& catalog, economy::BuildingTypeId id, economy::Cell origin) noexcept {
    const auto* type = catalog.BuildingType(id);
    if (!type || type->width == 0 || type->depth == 0)
        return {PlacementFailure::UnknownBuildingType, {}};
    if (origin.x < 0 || origin.z < 0 ||
        static_cast<std::uint64_t>(origin.x) + type->width > snapshot.gridWidth ||
        static_cast<std::uint64_t>(origin.z) + type->depth > snapshot.gridDepth)
        return {PlacementFailure::OutOfBounds, {}};
    for (const auto& building : snapshot.buildings) {
        const auto* occupied = catalog.BuildingType(building.type);
        if (!occupied) continue;
        if (origin.x < static_cast<std::int64_t>(building.origin.x) + occupied->width &&
            building.origin.x < static_cast<std::int64_t>(origin.x) + type->width &&
            origin.z < static_cast<std::int64_t>(building.origin.z) + occupied->depth &&
            building.origin.z < static_cast<std::int64_t>(origin.z) + type->depth)
            return {PlacementFailure::BuildingCollision, {}};
    }
    const economy::Cell access{origin.x + static_cast<std::int32_t>((type->width - 1) / 2), origin.z - 1};
    if (!InGrid(access, snapshot)) return {PlacementFailure::AccessCellUnavailable, access};
    for (const auto& building : snapshot.buildings) {
        const auto* occupied = catalog.BuildingType(building.type);
        if (!occupied) continue;
        if (InFootprint(*occupied, building.origin, access)) return {PlacementFailure::AccessCellUnavailable, access};
        if (InFootprint(*type, origin, building.access)) return {PlacementFailure::AccessCellUnavailable, access};
    }
    for (const auto& road : snapshot.roads)
        if (InFootprint(*type, origin, road.a) || InFootprint(*type, origin, road.b))
            return {PlacementFailure::RoadOnBuilding, access};
    for (const auto& rail : snapshot.rails)
        if (InFootprint(*type, origin, rail.a) || InFootprint(*type, origin, rail.b))
            return {PlacementFailure::RailOnBuilding, access};
    return {PlacementFailure::None, access};
}
} // namespace openrepublic::runtime
