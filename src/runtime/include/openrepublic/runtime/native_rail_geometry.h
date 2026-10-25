#pragma once

#include "openrepublic/runtime/native_economy_layout.h"

namespace openrepublic::runtime {

/** Renderer-independent transform for one adjacent native rail edge. */
struct NativeRailGeometry {
    NativePoint center;
    double length{};
    double yawRadians{};
};

/** Maps a valid horizontal unit rail edge to display space; invalid edges return false. */
bool MapNativeRailGeometry(economy::Cell a, economy::Cell b,
                           const NativeEconomyTransform &transform,
                           NativeRailGeometry &out) noexcept;

/** Maps a position along a unit rail edge using the shared vehicle interpolation policy. */
NativeVehicleMotion InterpolateNativeRailMotion(economy::Cell previous, economy::Cell current,
    double alpha, double lastYawRadians, const NativeEconomyTransform &transform,
    double wheelRadiusDisplayUnits = 0.48) noexcept;

} // namespace openrepublic::runtime
