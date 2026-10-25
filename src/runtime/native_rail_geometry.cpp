#include "openrepublic/runtime/native_rail_geometry.h"

#include <algorithm>
#include <cmath>

namespace openrepublic::runtime {

bool MapNativeRailGeometry(economy::Cell a, economy::Cell b,
                           const NativeEconomyTransform &transform,
                           NativeRailGeometry &out) noexcept {
    const auto dx = static_cast<std::int64_t>(b.x) - a.x;
    const auto dz = static_cast<std::int64_t>(b.z) - a.z;
    if (!((dx == 0 && (dz == 1 || dz == -1)) ||
          (dz == 0 && (dx == 1 || dx == -1)))) return false;
    const NativePoint p0 = transform.CellCenter(a);
    const NativePoint p1 = transform.CellCenter(b);
    NativeRailGeometry geometry;
    geometry.center = {(p0.x + p1.x) * 0.5, (p0.z + p1.z) * 0.5};
    geometry.length = std::hypot(p1.x - p0.x, p1.z - p0.z);
    geometry.yawRadians = std::atan2(p1.x - p0.x, p1.z - p0.z);
    if (!std::isfinite(geometry.center.x) || !std::isfinite(geometry.center.z) ||
        !std::isfinite(geometry.length) || !std::isfinite(geometry.yawRadians) || geometry.length <= 0.0)
        return false;
    out = geometry;
    return true;
}

NativeVehicleMotion InterpolateNativeRailMotion(economy::Cell previous, economy::Cell current,
    double alpha, double lastYawRadians, const NativeEconomyTransform &transform,
    double wheelRadiusDisplayUnits) noexcept {
    const std::int64_t dx = static_cast<std::int64_t>(current.x) - previous.x;
    const std::int64_t dz = static_cast<std::int64_t>(current.z) - previous.z;
    const std::uint64_t cells = static_cast<std::uint64_t>(std::abs(dx) + std::abs(dz));
    if (cells <= 1 || (dx != 0 && dz != 0))
        return InterpolateNativeVehicleMotion(previous, current, alpha, lastYawRadians,
            transform, wheelRadiusDisplayUnits);

    // Rail snapshots may advance multiple cells per tick. When both endpoints lie
    // on the same straight track row/column, retain smooth motion with the shared
    // cell-center, +Z-forward, alpha-clamp and wheel-distance conventions.
    const double t = std::isfinite(alpha) ? std::clamp(alpha, 0.0, 1.0) : 0.0;
    const NativePoint from = transform.CellCenter(previous);
    const NativePoint to = transform.CellCenter(current);
    NativeVehicleMotion motion;
    motion.moving = cells > 0;
    motion.position = {from.x + (to.x - from.x) * t, from.z + (to.z - from.z) * t};
    motion.yawRadians = motion.moving ? std::atan2(to.x - from.x, to.z - from.z) :
        (std::isfinite(lastYawRadians) ? lastYawRadians : 0.0);
    motion.traveledDisplayUnits = static_cast<double>(cells) * NativeEconomyTransform::DisplayUnitsPerCell * t;
    if (std::isfinite(wheelRadiusDisplayUnits) && wheelRadiusDisplayUnits > 0.0)
        motion.wheelRotationRadians = motion.traveledDisplayUnits / wheelRadiusDisplayUnits;
    return motion;
}

} // namespace openrepublic::runtime
