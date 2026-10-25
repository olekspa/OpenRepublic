#pragma once

#include "openrepublic/economy/economy.h"
#include "openrepublic/runtime/native_economy_layout.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openrepublic::runtime {

struct AmbientTrafficVehicle {
    std::uint32_t id{};
    NativePoint position; // continuous cell-center coordinates in grid space
    double yawRadians{};
};

/** Pure deterministic decorative traffic placement on caller-supplied road polylines.
 *  Speed, spacing, count and phase are OpenRepublic presentation constants, not game rules.
 *  Invalid/zero-length lines are ignored; output count is capped at 32. */
std::vector<AmbientTrafficVehicle> ComputeAmbientTraffic(
    const std::vector<std::vector<economy::Cell>>& roadPolylines,
    std::uint64_t tick, std::size_t requestedCount = 8,
    double cellsPerTick = 0.08);

} // namespace openrepublic::runtime
