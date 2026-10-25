#include "openrepublic/runtime/synthetic_terrain.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void Check(bool ok, const char* label) {
    if (!ok) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
}

int main() {
    using openrepublic::runtime::SyntheticTerrain;
    const SyntheticTerrain a{20261025u, 0.0}, repeat{20261025u, 0.0}, other{77u, 0.0};
    Check(a.HeightAt(73.25, -51.5) == repeat.HeightAt(73.25, -51.5), "seeded height is repeatable");
    Check(a.HeightAt(73.25, -51.5) != other.HeightAt(73.25, -51.5), "seed changes the heightfield");
    Check(std::abs(a.HeightAt(0.0, 0.0)) < 1e-9, "starter center is level");
    Check(a.IsBuildable(0.0, 0.0, 12.0, 8.0), "starter footprint is buildable");
    Check(!a.IsBuildable(38.0, -27.0, 5.0, 5.0), "lake footprint is not buildable");
    bool steepAreaRejected = false;
    for (int z = -100; z <= 100 && !steepAreaRejected; z += 2) {
        for (int x = -100; x <= 100; x += 2) {
            if (!a.IsWater(x, z) && a.SlopeAt(x, z) > 0.22 && !a.IsBuildable(x, z, 2.0, 2.0)) {
                steepAreaRejected = true;
                break;
            }
        }
    }
    Check(steepAreaRejected, "steep synthetic terrain blocks placement");
    Check(!a.IsBuildable(1.0, 1.0, 0.0, 5.0), "empty footprint is rejected");
    Check(!a.IsBuildable(std::numeric_limits<double>::quiet_NaN(), 0.0, 2.0, 2.0), "non-finite footprint is rejected");
    Check(std::isfinite(a.SlopeAt(45.0, -5.0)), "slope remains finite");
    if (failures) return EXIT_FAILURE;
    std::cout << "synthetic terrain checks passed\n";
    return EXIT_SUCCESS;
}
