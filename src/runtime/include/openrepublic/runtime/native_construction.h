#pragma once

#include "openrepublic/economy/economy.h"

namespace openrepublic::runtime {

enum class PlacementFailure : std::uint8_t {
    None, UnknownBuildingType, OutOfBounds, BuildingCollision,
    AccessCellUnavailable, RoadOnBuilding, RailOnBuilding
};

struct PlacementValidation {
    PlacementFailure failure{PlacementFailure::None};
    economy::Cell access{};
    bool Valid() const noexcept { return failure == PlacementFailure::None; }
};

/** Mirrors native economy placement constraints for pointer feedback only.
 *  The queued economy command remains authoritative. */
PlacementValidation ValidateNativeBuildingPlacement(const economy::WorldSnapshot& snapshot,
    const economy::Catalog& catalog, economy::BuildingTypeId type, economy::Cell origin) noexcept;

} // namespace openrepublic::runtime
