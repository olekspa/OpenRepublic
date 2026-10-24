#include "openrepublic/runtime/native_economy_layout.h"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace openrepublic;
    int failures = 0;
    const auto check = [&](bool ok, const char *message) {
        if (!ok) { ++failures; std::cerr << message << '\n'; }
    };
    runtime::NativeEconomyTransform transform{};
    const economy::Cell west{2, 3}, east{3, 3};
    const auto half = runtime::InterpolateNativeVehicleMotion(west, east, 0.5, 1.25, transform);
    const auto from = transform.CellCenter(west);
    const auto to = transform.CellCenter(east);
    check(std::abs(half.position.x - (from.x + to.x) * 0.5) < 1e-9 &&
          std::abs(half.position.z - from.z) < 1e-9, "one-cell movement interpolates between cell centers");
    check(half.moving && std::abs(half.yawRadians - 1.5707963267948966) < 1e-9,
          "eastward movement faces +X");
    check(std::abs(half.traveledDisplayUnits - 2.0) < 1e-9 &&
          std::abs(half.wheelRotationRadians - 2.0 / 0.48) < 1e-9,
          "wheel angle follows interpolated display distance");

    const auto stationary = runtime::InterpolateNativeVehicleMotion(west, west, 1.0, 0.75, transform);
    check(!stationary.moving && stationary.yawRadians == 0.75 &&
          stationary.traveledDisplayUnits == 0.0 && stationary.wheelRotationRadians == 0.0,
          "stationary vehicle holds yaw without wheel rotation");
    const auto teleport = runtime::InterpolateNativeVehicleMotion(west, {7, 3}, 0.2, 0.5, transform);
    check(teleport.position.x == to.x + 16.0 && teleport.position.z == to.z &&
          !teleport.moving && teleport.traveledDisplayUnits == 0.0,
          "multi-cell respawn snaps to the current cell without spinning wheels");
    const auto clamped = runtime::InterpolateNativeVehicleMotion(west, east, 4.0, 0.0, transform);
    check(clamped.position.x == to.x, "alpha clamps to one");
    const auto invalidAlpha = runtime::InterpolateNativeVehicleMotion(west, east,
        std::numeric_limits<double>::quiet_NaN(), 0.0, transform);
    check(invalidAlpha.position.x == from.x, "non-finite alpha clamps to zero");
    const auto invalidYaw = runtime::InterpolateNativeVehicleMotion(west, west, 0.0,
        std::numeric_limits<double>::quiet_NaN(), transform);
    check(invalidYaw.yawRadians == 0.0, "non-finite held yaw falls back to zero");
    const auto invalidRadius = runtime::InterpolateNativeVehicleMotion(west, east, 1.0, 0.0,
        transform, 0.0);
    check(invalidRadius.wheelRotationRadians == 0.0, "invalid wheel radius avoids division");
    return failures == 0 ? 0 : 1;
}
