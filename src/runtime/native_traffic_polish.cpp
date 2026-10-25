#include "openrepublic/runtime/native_traffic_polish.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace openrepublic::runtime {
namespace {
constexpr double kEpsilon = 1e-12;
bool Finite(NativePoint p) noexcept { return std::isfinite(p.x) && std::isfinite(p.z); }
NativePoint Add(NativePoint a, NativePoint b) noexcept { return {a.x + b.x, a.z + b.z}; }
NativePoint Sub(NativePoint a, NativePoint b) noexcept { return {a.x - b.x, a.z - b.z}; }
NativePoint Mul(NativePoint a, double s) noexcept { return {a.x * s, a.z * s}; }
double Length(NativePoint p) noexcept { return std::hypot(p.x, p.z); }
NativePoint Unit(NativePoint p) noexcept {
    const double length = Length(p);
    return length > kEpsilon && std::isfinite(length) ? Mul(p, 1.0 / length) : NativePoint{};
}
double Clamp01(double v) noexcept { return std::clamp(v, 0.0, 1.0); }
struct Piece { NativePoint a, b; double length; };
}

NativePoint OffsetToRightLane(NativePoint position, NativePoint tangent, double offset) noexcept {
    if (!Finite(position)) return {};
    if (!Finite(tangent) || !std::isfinite(offset)) return position;
    const NativePoint direction = Unit(tangent);
    if (Length(direction) <= kEpsilon) return position;
    // X/Z coordinates: for forward +Z, right is +X.
    return Add(position, {direction.z * offset, -direction.x * offset});
}

TrafficPathSample SampleRoundedTrafficPath(const std::vector<NativePoint>& input,
    double distance, double cornerRadius) {
    std::vector<NativePoint> path;
    path.reserve(input.size());
    for (const auto p : input) if (Finite(p) && (path.empty() || Length(Sub(p, path.back())) > kEpsilon)) path.push_back(p);
    if (path.empty()) return {};
    if (path.size() == 1) return {path.front(), {}};
    if (!std::isfinite(cornerRadius) || cornerRadius < 0.0) cornerRadius = 0.0;
    std::vector<NativePoint> points;
    points.reserve(path.size() * 8);
    points.push_back(path.front());
    for (std::size_t i = 1; i + 1 < path.size(); ++i) {
        const NativePoint corner = path[i];
        const NativePoint incoming = Unit(Sub(corner, path[i - 1]));
        const NativePoint outgoing = Unit(Sub(path[i + 1], corner));
        const double dot = std::clamp(incoming.x * outgoing.x + incoming.z * outgoing.z, -1.0, 1.0);
        const double radius = std::min({cornerRadius, Length(Sub(corner, path[i - 1])) * 0.5,
            Length(Sub(path[i + 1], corner)) * 0.5});
        if (radius <= kEpsilon || dot > 0.999999 || dot < -0.999999) {
            points.push_back(corner);
            continue;
        }
        const NativePoint entry = Sub(corner, Mul(incoming, radius));
        const NativePoint exit = Add(corner, Mul(outgoing, radius));
        points.push_back(entry);
        constexpr int kCurveSteps = 16;
        for (int step = 1; step <= kCurveSteps; ++step) {
            const double t = static_cast<double>(step) / kCurveSteps;
            const double u = 1.0 - t;
            points.push_back(Add(Add(Mul(entry, u * u), Mul(corner, 2.0 * u * t)), Mul(exit, t * t)));
        }
    }
    points.push_back(path.back());
    std::vector<Piece> pieces;
    pieces.reserve(points.size());
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const double length = Length(Sub(points[i], points[i - 1]));
        if (length > kEpsilon && std::isfinite(length)) {
            pieces.push_back({points[i - 1], points[i], length});
            total += length;
        }
    }
    if (pieces.empty()) return {path.front(), {}};
    if (!std::isfinite(distance)) distance = 0.0;
    distance = std::clamp(distance, 0.0, total);
    for (const Piece& piece : pieces) {
        if (distance <= piece.length) {
            const NativePoint tangent = Unit(Sub(piece.b, piece.a));
            return {Add(piece.a, Mul(Sub(piece.b, piece.a), Clamp01(distance / piece.length))), tangent};
        }
        distance -= piece.length;
    }
    const Piece& last = pieces.back();
    return {last.b, Unit(Sub(last.b, last.a))};
}

double LimitTrafficHeading(double currentHeading, double targetHeading, double maxTurnRadians) noexcept {
    if (!std::isfinite(targetHeading)) targetHeading = 0.0;
    if (!std::isfinite(currentHeading)) currentHeading = targetHeading;
    if (!std::isfinite(maxTurnRadians) || maxTurnRadians < 0.0) maxTurnRadians = 0.0;
    double delta = std::remainder(targetHeading - currentHeading, 2.0 * 3.14159265358979323846);
    if (!std::isfinite(delta)) delta = 0.0;
    delta = std::clamp(delta, -maxTurnRadians, maxTurnRadians);
    return std::remainder(currentHeading + delta, 2.0 * 3.14159265358979323846);
}

std::vector<TrafficFollowingResult> ResolveTrafficFollowing(
    const std::vector<TrafficFollowingSample>& vehicles, double safetyGap, double speedLookaheadTicks) {
    if (!std::isfinite(safetyGap) || safetyGap < 0.0) safetyGap = 0.0;
    if (!std::isfinite(speedLookaheadTicks) || speedLookaheadTicks <= 0.0) speedLookaheadTicks = 1.0;
    struct Indexed { TrafficFollowingSample sample; std::size_t index; };
    std::vector<Indexed> ordered;
    ordered.reserve(vehicles.size());
    for (std::size_t i = 0; i < vehicles.size(); ++i) ordered.push_back({vehicles[i], i});
    std::stable_sort(ordered.begin(), ordered.end(), [](const Indexed& a, const Indexed& b) {
        if (a.sample.segmentId != b.sample.segmentId) return a.sample.segmentId < b.sample.segmentId;
        if (a.sample.direction != b.sample.direction) return a.sample.direction < b.sample.direction;
        const double positionA = std::isfinite(a.sample.position) ? a.sample.position : 0.0;
        const double positionB = std::isfinite(b.sample.position) ? b.sample.position : 0.0;
        if (positionA != positionB) return positionA > positionB;
        if (a.sample.vehicleId != b.sample.vehicleId) return a.sample.vehicleId < b.sample.vehicleId;
        return a.index < b.index;
    });
    std::vector<TrafficFollowingResult> result(vehicles.size());
    std::size_t i = 0;
    while (i < ordered.size()) {
        const auto keySegment = ordered[i].sample.segmentId;
        const auto keyDirection = ordered[i].sample.direction;
        std::size_t end = i + 1;
        while (end < ordered.size() && ordered[end].sample.segmentId == keySegment && ordered[end].sample.direction == keyDirection) ++end;
        double leaderPosition = std::numeric_limits<double>::infinity();
        double leaderLength = 0.0;
        for (std::size_t j = i; j < end; ++j) {
            const auto& s = ordered[j].sample;
            double position = std::isfinite(s.position) ? s.position : 0.0;
            const double length = std::isfinite(s.length) && s.length > 0.0 ? s.length : 0.0;
            const double desired = std::isfinite(s.desiredSpeed) && s.desiredSpeed > 0.0 ? s.desiredSpeed : 0.0;
            double queued = position;
            double cap = desired;
            if (std::isfinite(leaderPosition)) {
                const double gap = std::max(0.0, leaderPosition - position - (leaderLength + length) * 0.5 - safetyGap);
                queued = std::min(position, leaderPosition - (leaderLength + length) * 0.5 - safetyGap);
                cap = std::min(desired, gap / speedLookaheadTicks);
            }
            if (!std::isfinite(queued)) queued = position;
            result[ordered[j].index] = {s.vehicleId, queued, cap};
            leaderPosition = queued;
            leaderLength = length;
        }
        i = end;
    }
    return result;
}

TrafficYieldDecision ResolveTrafficIntersectionYield(std::uint64_t vehicleId, NativePoint position,
    const std::vector<TrafficIntersectionSample>& others, double conflictRadius,
    std::uint32_t maxWaitTicks) noexcept {
    if (!Finite(position) || !std::isfinite(conflictRadius) || conflictRadius < 0.0 || maxWaitTicks == 0) return {};
    std::uint64_t winner = vehicleId;
    bool wait = false;
    const double radiusSquared = conflictRadius * conflictRadius;
    for (const auto& other : others) {
        if (other.vehicleId == vehicleId || !Finite(other.position)) continue;
        const NativePoint delta = Sub(position, other.position);
        if (delta.x * delta.x + delta.z * delta.z > radiusSquared) continue;
        if (other.vehicleId < vehicleId && (!wait || other.vehicleId < winner)) {
            wait = true;
            winner = other.vehicleId;
        }
    }
    if (!wait) return {false, 0, vehicleId};
    // Stable pair hash spreads the pause over a few presentation ticks without state.
    const std::uint64_t mixed = (vehicleId * 0x9E3779B97F4A7C15ull) ^ (winner + 0xBF58476D1CE4E5B9ull);
    return {true, static_cast<std::uint32_t>(mixed % maxWaitTicks) + 1u, winner};
}

} // namespace openrepublic::runtime
