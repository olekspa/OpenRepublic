#pragma once

#include "openrepublic/economy/economy.h"

#include <cstdint>
#include <optional>

namespace openrepublic::runtime {

enum class CoalDemoActionKind { ImportCoal, DispatchToStorage, DispatchToPlant };
struct CoalDemoAction {
    CoalDemoActionKind kind{};
    economy::VehicleId vehicle{};
    economy::BuildingId source{}, destination{};
    std::uint64_t amount{};
};

/** Deterministic planner for the authored coal demo; it never mutates a world. */
std::optional<CoalDemoAction> NextCoalDemoAction(const economy::WorldSnapshot& world,
    economy::BuildingId border, economy::BuildingId depot, economy::BuildingId storage,
    economy::BuildingId plant, economy::ResourceId coal, std::uint64_t batch = 4);

} // namespace openrepublic::runtime
