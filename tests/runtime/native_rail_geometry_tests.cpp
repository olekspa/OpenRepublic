#include "openrepublic/economy/economy.h"
#include "openrepublic/runtime/native_rail_geometry.h"

#include <cmath>
#include <iostream>

int main() {
    using namespace openrepublic;
    int failures = 0;
    const auto check = [&failures](bool ok, const char *message) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };

    runtime::NativeEconomyTransform transform;
    transform.centerCellX = 2.0;
    transform.centerCellZ = 3.0;
    runtime::NativeRailGeometry mapped{{99.0, 99.0}, 99.0, 99.0};
    check(runtime::MapNativeRailGeometry({2, 3}, {3, 3}, transform, mapped),
        "east-west unit edge maps");
    check(std::abs(mapped.center.x - 4.0) < 1e-9 && std::abs(mapped.center.z - 2.0) < 1e-9,
        "edge center uses cell-center display coordinates");
    check(std::abs(mapped.length - 4.0) < 1e-9 && std::abs(mapped.yawRadians - 1.5707963267948966) < 1e-9,
        "horizontal edge maps one cell length and +X heading");
    check(runtime::MapNativeRailGeometry({3, 3}, {3, 4}, transform, mapped) &&
        std::abs(mapped.yawRadians) < 1e-9, "northbound edge faces +Z");
    const runtime::NativeRailGeometry before = mapped;
    check(!runtime::MapNativeRailGeometry({0, 0}, {2, 1}, transform, mapped),
        "diagonal multi-cell edge is rejected");
    check(mapped.center.x == before.center.x && mapped.center.z == before.center.z &&
        mapped.length == before.length && mapped.yawRadians == before.yawRadians,
        "failed mapping leaves output unchanged");

    const auto motion = runtime::InterpolateNativeRailMotion({2, 3}, {3, 3}, 0.5, 0.0, transform);
    check(motion.moving && std::abs(motion.position.x - 4.0) < 1e-9 &&
        std::abs(motion.position.z - 2.0) < 1e-9 && std::abs(motion.traveledDisplayUnits - 2.0) < 1e-9,
        "rail motion reuses shared cell interpolation and distance policy");
    const auto longMotion = runtime::InterpolateNativeRailMotion({1, 3}, {4, 3}, 0.5, 0.0, transform);
    check(longMotion.moving && std::abs(longMotion.position.x - 4.0) < 1e-9 &&
        std::abs(longMotion.position.z - 2.0) < 1e-9 && std::abs(longMotion.traveledDisplayUnits - 6.0) < 1e-9,
        "rail interpolation supports multi-cell straight track movement");

    const economy::Catalog catalog = economy::MakeSyntheticRailCatalog();
    economy::BuildingTypeId station{};
    economy::VehicleTypeId train{};
    check(catalog.FindBuildingType("rail_station", station) && catalog.BuildingType(station)->station,
        "rail preset provides a station building");
    check(catalog.FindVehicleType("train", train) && catalog.VehicleType(train)->railOnly,
        "rail preset provides a rail-only train");
    return failures == 0 ? 0 : 1;
}
