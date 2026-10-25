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
    runtime::NativeEconomyTransform importedTransform{};
    check(runtime::BuildImportedRegionVehicleTransform(100.0, -40.0, 4.0, importedTransform),
          "region importer coordinates produce a vehicle display transform");
    const auto importedCell = importedTransform.CellCenter({3, 5});
    check(std::abs(importedCell.x - 114.0) < 1e-9 &&
          std::abs(importedCell.z - (-18.0)) < 1e-9,
          "region vehicle cell centers match importer origin plus half-cell mapping");
    const runtime::NativeEconomyTransform priorTransform{7.0, 8.0};
    importedTransform = priorTransform;
    check(!runtime::BuildImportedRegionVehicleTransform(100.0, -40.0, 3.0, importedTransform) &&
          importedTransform.centerCellX == 7.0 && importedTransform.centerCellZ == 8.0,
          "unsupported importer cell scale fails without changing the output transform");
    check(!runtime::BuildImportedRegionVehicleTransform(std::numeric_limits<double>::infinity(), 0.0,
          4.0, importedTransform), "non-finite imported region origin fails closed");
    const double truckScale = runtime::NativeVehicleDisplayScale(7.8);
    check(std::abs(truckScale * 7.8 - 3.0) < 1e-9,
          "procedural truck length targets three quarters of a native display cell");
    check(runtime::NativeVehicleDisplayScale(0.0) == 0.0 &&
          runtime::NativeVehicleDisplayScale(std::numeric_limits<double>::infinity()) == 0.0,
          "invalid vehicle lengths fail closed");
    check(std::abs(runtime::NativeEconomyCameraDistance(80.0, 20.0, 0.68) - 81.6) < 1e-9,
          "camera framing scale gives the demo a closer view while retaining scene-span basis");
    check(runtime::NativeEconomyCameraDistance(1.0, 1.0, 0.0) == 0.0,
          "invalid camera distance scale fails closed");
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
