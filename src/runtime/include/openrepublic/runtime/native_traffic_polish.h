#pragma once

#include "openrepublic/runtime/native_economy_layout.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openrepublic::runtime {

// Presentation-only road motion helpers. All distances, speeds, turn limits,
// and yielding rules are OpenRepublic policy; WRSR semantics remain UNKNOWN.
struct TrafficPathSample {
    NativePoint position{};
    NativePoint tangent{}; // unit direction in X/Z, or zero when stationary
};

// Positive offset moves to the vehicle's right while travelling along tangent.
NativePoint OffsetToRightLane(NativePoint position, NativePoint tangent,
    double offset) noexcept;

// Sample a polyline at distance from its beginning. Corners are rounded with
// quadratic Bezier arcs whose control point is the original corner.
TrafficPathSample SampleRoundedTrafficPath(const std::vector<NativePoint>& path,
    double distance, double cornerRadius);

// Rotate a heading toward target by at most maxTurnRadians, with shortest-wrap
// handling. Headings use atan2(tangent.x, tangent.z), matching native motion.
double LimitTrafficHeading(double currentHeading, double targetHeading,
    double maxTurnRadians) noexcept;

struct TrafficFollowingSample {
    std::uint64_t vehicleId{};
    std::uint64_t segmentId{};
    int direction{}; // equal values indicate same-way travel on this segment
    double position{}; // distance along the segment
    double desiredSpeed{};
    double length{};
};

struct TrafficFollowingResult {
    std::uint64_t vehicleId{};
    double queuedPosition{};
    double speedCap{};
};

// Pure snapshot calculation: each same-segment/same-direction follower is
// moved behind the prior vehicle with a bounded speed cap. Does not mutate or
// feed back into economy state. Invalid samples are returned stopped.
std::vector<TrafficFollowingResult> ResolveTrafficFollowing(
    const std::vector<TrafficFollowingSample>& vehicles,
    double safetyGap, double speedLookaheadTicks = 1.0);

struct TrafficIntersectionSample {
    std::uint64_t vehicleId{};
    NativePoint position{};
};

struct TrafficYieldDecision {
    bool shouldWait{};
    std::uint32_t waitTicks{};
    std::uint64_t priorityVehicleId{};
};

// Vehicles within conflictRadius are resolved by lower stable ID. A yielding
// decision lasts 1..maxWaitTicks presentation ticks; no persistent state is
// owned here, so callers recompute from each snapshot.
TrafficYieldDecision ResolveTrafficIntersectionYield(std::uint64_t vehicleId,
    NativePoint position, const std::vector<TrafficIntersectionSample>& others,
    double conflictRadius, std::uint32_t maxWaitTicks = 3) noexcept;

} // namespace openrepublic::runtime
