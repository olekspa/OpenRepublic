#include "openrepublic/runtime/ambient_traffic.h"

#include <cmath>
#include <iostream>

using namespace openrepublic;

int main() {
    int failures = 0;
    const std::vector<std::vector<economy::Cell>> roads{{{0, 0}, {8, 0}, {8, 6}}, {{2, 2}, {2, 7}}};
    const auto check = [&](bool pass, const char* label) {
        if (!pass) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
    };
    const auto first = runtime::ComputeAmbientTraffic(roads, 10, 4);
    const auto repeat = runtime::ComputeAmbientTraffic(roads, 10, 4);
    check(first.size() == 4 && repeat.size() == first.size(), "requested bounded traffic count is returned");
    bool deterministic = first.size() == repeat.size();
    bool finite = true;
    for (std::size_t i = 0; i < first.size() && i < repeat.size(); ++i) {
        deterministic = deterministic && first[i].id == repeat[i].id &&
            first[i].position.x == repeat[i].position.x && first[i].position.z == repeat[i].position.z &&
            first[i].yawRadians == repeat[i].yawRadians;
        finite = finite && std::isfinite(first[i].position.x) && std::isfinite(first[i].position.z) &&
            std::isfinite(first[i].yawRadians);
    }
    check(deterministic, "tick and road input produce deterministic positions");
    check(finite, "traffic positions and headings are finite");
    const auto later = runtime::ComputeAmbientTraffic(roads, 11, 4);
    check(later.size() == first.size() && later[0].position.x != first[0].position.x,
        "tick advances ambient traffic along roads");
    check(runtime::ComputeAmbientTraffic({}, 10).empty(), "empty roads produce no ambient traffic");
    check(runtime::ComputeAmbientTraffic({{{1, 1}, {1, 1}}}, 10).empty(),
        "zero-length road geometry produces no traffic");
    check(runtime::ComputeAmbientTraffic(roads, 10, 100).size() == 32,
        "requested traffic is capped at the documented low-cost limit");
    check(runtime::ComputeAmbientTraffic(roads, 10, 8, 0.0).empty(),
        "invalid traffic speed fails closed");
    return failures == 0 ? 0 : 1;
}
