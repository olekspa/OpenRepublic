#include "openrepublic/runtime/native_economy_tools.h"

#include <cmath>
#include <limits>
#include <unordered_set>

namespace openrepublic::runtime {

bool NativeDisplayPointToCell(const NativeEconomyTransform &transform, double worldX,
                              double worldZ, std::uint32_t gridWidth,
                              std::uint32_t gridDepth, economy::Cell &outCell) noexcept {
    if (!std::isfinite(worldX) || !std::isfinite(worldZ) ||
        !std::isfinite(transform.centerCellX) || !std::isfinite(transform.centerCellZ) ||
        gridWidth == 0 || gridDepth == 0 ||
        gridWidth > economy::MaxWorldGridSize || gridDepth > economy::MaxWorldGridSize) {
        return false;
    }

    const double nativeX = worldX / NativeEconomyTransform::DisplayUnitsPerCell +
                           transform.centerCellX;
    const double nativeZ = worldZ / NativeEconomyTransform::DisplayUnitsPerCell +
                           transform.centerCellZ;
    if (!std::isfinite(nativeX) || !std::isfinite(nativeZ)) {
        return false;
    }
    const double cellX = std::floor(nativeX);
    const double cellZ = std::floor(nativeZ);
    // Check before conversion: converting an out-of-range double to int32_t is undefined.
    if (cellX < 0.0 || cellZ < 0.0 || cellX >= static_cast<double>(gridWidth) ||
        cellZ >= static_cast<double>(gridDepth) ||
        cellX > static_cast<double>(std::numeric_limits<std::int32_t>::max()) ||
        cellZ > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }

    const economy::Cell converted{static_cast<std::int32_t>(cellX),
                                  static_cast<std::int32_t>(cellZ)};
    outCell = converted;
    return true;
}

std::vector<economy::ResourceId>
ListNativeHaulResources(const economy::WorldSnapshot &snapshot,
                        const economy::Catalog &catalog,
                        economy::BuildingId source, economy::BuildingId destination) {
    if (source == 0 || destination == 0 || source == destination) {
        return {};
    }

    const economy::BuildingSnapshot *sourceBuilding = nullptr;
    const economy::BuildingSnapshot *destinationBuilding = nullptr;
    for (const auto &building : snapshot.buildings) {
        if (building.id == source) {
            if (sourceBuilding != nullptr) return {};
            sourceBuilding = &building;
        }
        if (building.id == destination) {
            if (destinationBuilding != nullptr) return {};
            destinationBuilding = &building;
        }
    }
    if (sourceBuilding == nullptr || destinationBuilding == nullptr) {
        return {};
    }

    const auto *sourceType = catalog.BuildingType(sourceBuilding->type);
    const auto *destinationType = catalog.BuildingType(destinationBuilding->type);
    if (sourceType == nullptr || destinationType == nullptr) {
        return {};
    }

    std::unordered_set<economy::ResourceId> destinationResources;
    destinationResources.reserve(destinationType->storage.size());
    for (const auto &slot : destinationType->storage) {
        if (slot.capacity > 0 && catalog.Resource(slot.resource) != nullptr) {
            destinationResources.insert(slot.resource);
        }
    }

    // Catalog resources are in dense ID order, so iterating them makes the API
    // stable even if either storage declaration has another internal ordering.
    std::vector<economy::ResourceId> result;
    for (const auto &resource : catalog.Resources()) {
        if (destinationResources.find(resource.id) == destinationResources.end()) continue;
        for (const auto &slot : sourceType->storage) {
            if (slot.resource == resource.id && slot.capacity > 0) {
                result.push_back(resource.id);
                break;
            }
        }
    }
    return result;
}

} // namespace openrepublic::runtime
