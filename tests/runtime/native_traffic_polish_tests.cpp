#include "openrepublic/runtime/native_traffic_polish.h"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace openrepublic::runtime;
    int failures = 0;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) { ++failures; std::cerr << message << '\n'; }
    };
    const auto near = [](double a, double b, double epsilon = 1e-8) { return std::abs(a - b) <= epsilon; };

    const NativePoint right = OffsetToRightLane({2, 3}, {0, 5}, 0.25);
    const NativePoint left = OffsetToRightLane({2, 3}, {1, 0}, 0.25);
    check(near(right.x, 2.25) && near(right.z, 3), "right-hand offset of northbound path moves +X");
    check(near(left.x, 2) && near(left.z, 2.75), "right-hand offset of eastbound path moves -Z");
    const NativePoint zeroTangent = OffsetToRightLane({1, 2}, {}, 10);
    check(zeroTangent.x == 1 && zeroTangent.z == 2, "zero tangent leaves lane point in place");

    const std::vector<NativePoint> cornerPath{{0, 0}, {2, 0}, {2, 2}};
    const auto sharp = SampleRoundedTrafficPath(cornerPath, 2.0, 0.0);
    const auto rounded = SampleRoundedTrafficPath(cornerPath, 2.0, 0.5);
    check(near(sharp.position.x, 2) && near(sharp.position.z, 0), "zero radius retains original polyline corner");
    check(rounded.position.x < 2 && rounded.position.z < 0.5 &&
          std::hypot(rounded.tangent.x, rounded.tangent.z) > 0.99,
          "positive radius samples smooth curve before its corner with unit tangent");
    const auto end = SampleRoundedTrafficPath(cornerPath, 100, 0.5);
    check(near(end.position.x, 2) && near(end.position.z, 2), "path distance clamps to endpoint");
    const auto badPath = SampleRoundedTrafficPath({{std::numeric_limits<double>::quiet_NaN(), 0}, {1, 1}}, 1, 1);
    check(near(badPath.position.x, 1) && near(badPath.position.z, 1), "nonfinite points are ignored safely");

    constexpr double pi = 3.14159265358979323846;
    check(near(LimitTrafficHeading(0, pi, 0.2), 0.2), "heading turn is capped at maximum rate");
	check(near(std::remainder(LimitTrafficHeading(3.1, -3.1, 0.2) - 3.1, 2 * pi), 2 * pi - 6.2),
		"heading uses shortest wrap direction across the pi boundary");
    check(near(LimitTrafficHeading(1, -1, -2), 1), "negative turn limit permits no rotation");

    const std::vector<TrafficFollowingSample> queue{
        {3, 9, 1, 8, 4, 2}, {1, 9, 1, 10, 5, 2}, {2, 9, 1, 9, 6, 2},
        {4, 9, -1, 9, 6, 2}, {5, 10, 1, 9, 6, 2}};
    const auto following = ResolveTrafficFollowing(queue, 1.0, 2.0);
    const auto resultFor = [&](std::uint64_t id) {
        for (const auto& result : following) if (result.vehicleId == id) return result;
        return TrafficFollowingResult{};
    };
    check(near(resultFor(1).queuedPosition - resultFor(2).queuedPosition, 3.0), "following keeps leader/follower vehicle lengths and gap apart");
    check(near(resultFor(2).queuedPosition - resultFor(3).queuedPosition, 3.0), "queue is recursively spaced without overlap");
    check(near(resultFor(2).speedCap, 0), "overlapping snapshot follower receives zero speed cap");
    check(near(resultFor(4).queuedPosition, 9) && near(resultFor(5).queuedPosition, 9), "opposite direction and other segment are independent");
    const auto invalidFollowing = ResolveTrafficFollowing({{7, 1, 1, 4, std::numeric_limits<double>::quiet_NaN(), -2}}, -1, 0);
    check(invalidFollowing.size() == 1 && invalidFollowing[0].speedCap == 0 && invalidFollowing[0].queuedPosition == 4,
          "invalid following parameters and speed fail closed");

    const auto yield = ResolveTrafficIntersectionYield(8, {0, 0}, {{9, {0.5, 0}}, {2, {0.25, 0}}}, 1.0, 3);
    const auto noYield = ResolveTrafficIntersectionYield(2, {0, 0}, {{8, {0.2, 0}}}, 1.0, 3);
    const auto distant = ResolveTrafficIntersectionYield(8, {0, 0}, {{2, {5, 0}}}, 1.0, 3);
    check(yield.shouldWait && yield.priorityVehicleId == 2 && yield.waitTicks >= 1 && yield.waitTicks <= 3,
          "higher ID yields to nearby lower-priority ID with bounded wait");
    check(!noYield.shouldWait && noYield.waitTicks == 0, "lower ID retains intersection priority");
    check(!distant.shouldWait, "vehicles outside conflict radius do not yield");
    check(!ResolveTrafficIntersectionYield(3, {0, 0}, {{1, {0, 0}}}, -1, 3).shouldWait,
          "invalid intersection radius fails closed");
    return failures == 0 ? 0 : 1;
}
