#include "openrepublic/runtime/ambient_traffic.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace openrepublic::runtime {
namespace {
struct Segment { double x0, z0, x1, z1, length; };
}

std::vector<AmbientTrafficVehicle> ComputeAmbientTraffic(
    const std::vector<std::vector<economy::Cell>>& polylines,
    std::uint64_t tick, std::size_t requestedCount, double cellsPerTick) {
    std::vector<AmbientTrafficVehicle> result;
    if (!std::isfinite(cellsPerTick) || cellsPerTick <= 0.0) return result;
    requestedCount = std::min<std::size_t>(requestedCount, 32);
    std::vector<Segment> segments;
    double totalLength = 0.0;
    for (const auto& line : polylines) {
        for (std::size_t i = 1; i < line.size(); ++i) {
            const double dx = static_cast<double>(line[i].x) - line[i - 1].x;
            const double dz = static_cast<double>(line[i].z) - line[i - 1].z;
            const double length = std::hypot(dx, dz);
            if (!(length > 0.0) || !std::isfinite(length)) continue;
            segments.push_back({line[i - 1].x + 0.5, line[i - 1].z + 0.5,
                line[i].x + 0.5, line[i].z + 0.5, length});
            totalLength += length;
        }
    }
    if (segments.empty() || !std::isfinite(totalLength) || totalLength <= 0.0) return result;
    result.reserve(requestedCount);
    constexpr double goldenPhase = 0.6180339887498948482;
    for (std::size_t id = 0; id < requestedCount; ++id) {
        const double phase = std::fmod(static_cast<double>(id) * goldenPhase, 1.0) * totalLength;
        double distance = std::fmod(phase + static_cast<double>(tick) * cellsPerTick, totalLength);
        if (!std::isfinite(distance)) continue;
        const Segment* chosen = &segments.back();
        for (const Segment& segment : segments) {
            if (distance < segment.length) { chosen = &segment; break; }
            distance -= segment.length;
        }
        const double t = std::clamp(distance / chosen->length, 0.0, 1.0);
        const double dx = chosen->x1 - chosen->x0;
        const double dz = chosen->z1 - chosen->z0;
        result.push_back({static_cast<std::uint32_t>(id),
            {chosen->x0 + dx * t, chosen->z0 + dz * t}, std::atan2(dx, dz)});
    }
    return result;
}

} // namespace openrepublic::runtime
