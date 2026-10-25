#include "openrepublic/runtime/native_economy_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace openrepublic::runtime {
namespace {

using economy::Cell;

struct CellBounds {
    double minX{std::numeric_limits<double>::infinity()};
    double minZ{std::numeric_limits<double>::infinity()};
    double maxX{-std::numeric_limits<double>::infinity()};
    double maxZ{-std::numeric_limits<double>::infinity()};

    void Include(double x0, double z0, double x1, double z1) noexcept {
        minX = std::min(minX, x0);
        minZ = std::min(minZ, z0);
        maxX = std::max(maxX, x1);
        maxZ = std::max(maxZ, z1);
    }

    bool Empty() const noexcept { return !std::isfinite(minX); }
};

bool InGrid(Cell cell, const economy::WorldSnapshot &snapshot) noexcept {
    return cell.x >= 0 && cell.z >= 0 &&
           static_cast<std::uint32_t>(cell.x) < snapshot.gridWidth &&
           static_cast<std::uint32_t>(cell.z) < snapshot.gridDepth;
}

bool Fail(std::string_view message, std::string &error) {
    error.assign(message.data(), message.size());
    return false;
}

double BuildingHeight(std::uint32_t width, std::uint32_t depth) noexcept {
    // Stable display-only cue based on footprint. It carries no source-game scale claim.
    const double area = static_cast<double>(width) * static_cast<double>(depth);
    return std::clamp(2.0 + 0.75 * std::sqrt(area), 2.0, 12.0);
}

} // namespace

double NativeVehicleDisplayScale(double modelLength, double targetCellFraction) noexcept {
    if (!std::isfinite(modelLength) || modelLength <= 0.0 ||
        !std::isfinite(targetCellFraction) || targetCellFraction <= 0.0) return 0.0;
    return NativeEconomyTransform::DisplayUnitsPerCell * targetCellFraction / modelLength;
}

double NativeEconomyCameraDistance(double spanX, double spanZ, double distanceScale) noexcept {
    if (!std::isfinite(spanX) || !std::isfinite(spanZ) || !std::isfinite(distanceScale) ||
        spanX < 0.0 || spanZ < 0.0 || distanceScale <= 0.0) return 0.0;
    return std::max(16.0, std::max(spanX, spanZ) * 1.5) * distanceScale;
}

NativePoint NativeEconomyTransform::CellCenter(Cell cell) const noexcept {
    return {(static_cast<double>(cell.x) + 0.5 - centerCellX) * DisplayUnitsPerCell,
            (static_cast<double>(cell.z) + 0.5 - centerCellZ) * DisplayUnitsPerCell};
}

NativeVehicleMotion InterpolateNativeVehicleMotion(
    Cell previous, Cell current, double alpha, double lastYawRadians,
    const NativeEconomyTransform &transform, double wheelRadiusDisplayUnits) noexcept {
    const double t = std::isfinite(alpha) ? std::clamp(alpha, 0.0, 1.0) : 0.0;
    const double dxCells = static_cast<double>(current.x) - static_cast<double>(previous.x);
    const double dzCells = static_cast<double>(current.z) - static_cast<double>(previous.z);
    const double cellDistance = std::hypot(dxCells, dzCells);
    const bool teleport = !std::isfinite(cellDistance) || cellDistance > 1.0;
    const NativePoint from = transform.CellCenter(previous);
    const NativePoint to = transform.CellCenter(current);
    NativeVehicleMotion motion;
    if (teleport) {
        motion.position = to;
    } else {
        motion.position = {from.x + (to.x - from.x) * t,
                           from.z + (to.z - from.z) * t};
        motion.traveledDisplayUnits = cellDistance * NativeEconomyTransform::DisplayUnitsPerCell * t;
    }
    motion.moving = !teleport && cellDistance > 0.0;
    motion.yawRadians = motion.moving ? std::atan2(dxCells, dzCells) :
        (std::isfinite(lastYawRadians) ? lastYawRadians : 0.0);
    if (std::isfinite(wheelRadiusDisplayUnits) && wheelRadiusDisplayUnits > 0.0) {
        motion.wheelRotationRadians = motion.traveledDisplayUnits / wheelRadiusDisplayUnits;
    }
    return motion;
}

NativePoint NativeEconomyTransform::FootprintCenter(Cell origin, std::uint32_t width,
                                                     std::uint32_t depth) const noexcept {
    return {(static_cast<double>(origin.x) + static_cast<double>(width) * 0.5 - centerCellX) *
                DisplayUnitsPerCell,
            (static_cast<double>(origin.z) + static_cast<double>(depth) * 0.5 - centerCellZ) *
                DisplayUnitsPerCell};
}

bool BuildNativeEconomyScenePlan(const economy::WorldSnapshot &snapshot,
                                 const economy::Catalog &catalog,
                                 NativeEconomyScenePlan &outPlan,
                                 std::string &error,
                                 const NativeEconomyTransform *fixedTransform) {
    if (snapshot.gridWidth == 0 || snapshot.gridDepth == 0 ||
        snapshot.gridWidth > economy::MaxWorldGridSize ||
        snapshot.gridDepth > economy::MaxWorldGridSize) {
        return Fail("native scene grid dimensions are outside the supported range", error);
    }

    std::unordered_set<economy::BuildingId> buildingIds;
    std::unordered_set<economy::RoadId> roadIds;
    std::unordered_set<economy::VehicleId> vehicleIds;
    buildingIds.reserve(snapshot.buildings.size());
    roadIds.reserve(snapshot.roads.size());
    vehicleIds.reserve(snapshot.vehicles.size());

    CellBounds occupied;
    for (const auto &building : snapshot.buildings) {
        if (building.id == 0 || !buildingIds.insert(building.id).second) {
            return Fail("native scene contains a zero or duplicate building ID", error);
        }
        const auto *type = catalog.BuildingType(building.type);
        if (type == nullptr) {
            return Fail("native scene building references an unknown catalog type", error);
        }
        if (type->width == 0 || type->depth == 0) {
            return Fail("native scene building type has an empty footprint", error);
        }
        if (!InGrid(building.origin, snapshot) || !InGrid(building.access, snapshot)) {
            return Fail("native scene building origin or access cell is outside the grid", error);
        }
        const auto endX = static_cast<std::int64_t>(building.origin.x) + type->width;
        const auto endZ = static_cast<std::int64_t>(building.origin.z) + type->depth;
        if (endX > snapshot.gridWidth || endZ > snapshot.gridDepth) {
            return Fail("native scene building footprint exceeds the grid", error);
        }
        occupied.Include(building.origin.x, building.origin.z,
                         static_cast<double>(endX), static_cast<double>(endZ));
        occupied.Include(building.access.x, building.access.z,
                         static_cast<double>(building.access.x) + 1.0,
                         static_cast<double>(building.access.z) + 1.0);
    }

    for (const auto &road : snapshot.roads) {
        if (road.id == 0 || !roadIds.insert(road.id).second) {
            return Fail("native scene contains a zero or duplicate road ID", error);
        }
        if (!InGrid(road.a, snapshot) || !InGrid(road.b, snapshot)) {
            return Fail("native scene road endpoint is outside the grid", error);
        }
        const auto dx = static_cast<std::int64_t>(road.b.x) - road.a.x;
        const auto dz = static_cast<std::int64_t>(road.b.z) - road.a.z;
        if (!((dx == 0 && (dz == 1 || dz == -1)) ||
              (dz == 0 && (dx == 1 || dx == -1)))) {
            return Fail("native scene road is not a unit axis-aligned segment", error);
        }
        occupied.Include(std::min(road.a.x, road.b.x), std::min(road.a.z, road.b.z),
                         static_cast<double>(std::max(road.a.x, road.b.x)) + 1.0,
                         static_cast<double>(std::max(road.a.z, road.b.z)) + 1.0);
    }

    for (const auto &vehicle : snapshot.vehicles) {
        if (vehicle.id == 0 || !vehicleIds.insert(vehicle.id).second) {
            return Fail("native scene contains a zero or duplicate vehicle ID", error);
        }
        if (catalog.VehicleType(vehicle.type) == nullptr) {
            return Fail("native scene vehicle references an unknown catalog type", error);
        }
        if (!InGrid(vehicle.cell, snapshot)) {
            return Fail("native scene vehicle cell is outside the grid", error);
        }
        occupied.Include(vehicle.cell.x, vehicle.cell.z,
                         static_cast<double>(vehicle.cell.x) + 1.0,
                         static_cast<double>(vehicle.cell.z) + 1.0);
    }

    NativeEconomyTransform transform;
    if (fixedTransform != nullptr) {
        if (!std::isfinite(fixedTransform->centerCellX) ||
            !std::isfinite(fixedTransform->centerCellZ)) {
            return Fail("native scene fixed transform is not finite", error);
        }
        transform = *fixedTransform;
    } else if (!occupied.Empty()) {
        transform.centerCellX = (occupied.minX + occupied.maxX) * 0.5;
        transform.centerCellZ = (occupied.minZ + occupied.maxZ) * 0.5;
    } else {
        // Empty worlds start with a view over the first at-most-16 cells on each axis.
        transform.centerCellX = static_cast<double>(std::min(snapshot.gridWidth, 16u)) * 0.5;
        transform.centerCellZ = static_cast<double>(std::min(snapshot.gridDepth, 16u)) * 0.5;
    }

    NativeEconomyScenePlan plan;
    plan.transform = transform;
    plan.buildings.reserve(snapshot.buildings.size());
    plan.roads.reserve(snapshot.roads.size());

    std::vector<const economy::BuildingSnapshot *> buildings;
    buildings.reserve(snapshot.buildings.size());
    for (const auto &building : snapshot.buildings) {
        buildings.push_back(&building);
    }
    std::sort(buildings.begin(), buildings.end(), [](const auto *left, const auto *right) {
        return left->id < right->id;
    });
    CellBounds displayBounds;
    for (const auto *building : buildings) {
        const auto *type = catalog.BuildingType(building->type);
        const NativePoint center = transform.FootprintCenter(building->origin, type->width, type->depth);
        double width = static_cast<double>(type->width) * NativeEconomyTransform::DisplayUnitsPerCell;
        double depth = static_cast<double>(type->depth) * NativeEconomyTransform::DisplayUnitsPerCell;
        if (!std::isfinite(center.x) || !std::isfinite(center.z)) {
            return Fail("native scene building transform exceeds finite display coordinates", error);
        }
        double height = BuildingHeight(type->width, type->depth);
        if (building->constructionSite && building->constructionState < 2) {
            // OpenRepublic presentation: a site rises from a quarter height (foundation) to full height
            // as its synthetic construction progress goes from 0 to 1.
            const double progress = std::min(1.0, std::max(0.0, building->constructionProgress));
            height *= 0.25 + 0.75 * progress;
        }
        plan.buildings.push_back({building->id, building->type, center.x, center.z, width, depth, height});
        displayBounds.Include(center.x - width * 0.5, center.z - depth * 0.5,
                              center.x + width * 0.5, center.z + depth * 0.5);
    }

    std::vector<const economy::RoadSnapshot *> roads;
    roads.reserve(snapshot.roads.size());
    for (const auto &road : snapshot.roads) {
        roads.push_back(&road);
    }
    std::sort(roads.begin(), roads.end(), [](const auto *left, const auto *right) {
        return left->id < right->id;
    });
    constexpr double roadThickness = 0.35 * NativeEconomyTransform::DisplayUnitsPerCell;
    for (const auto *road : roads) {
        const NativePoint a = transform.CellCenter(road->a);
        const NativePoint b = transform.CellCenter(road->b);
        const double width = (road->a.x != road->b.x
                                  ? std::abs(static_cast<double>(road->b.x) - road->a.x) *
                                        NativeEconomyTransform::DisplayUnitsPerCell
                                  : roadThickness);
        const double depth = (road->a.z != road->b.z
                                  ? std::abs(static_cast<double>(road->b.z) - road->a.z) *
                                        NativeEconomyTransform::DisplayUnitsPerCell
                                  : roadThickness);
        const double centerX = (a.x + b.x) * 0.5;
        const double centerZ = (a.z + b.z) * 0.5;
        if (!std::isfinite(centerX) || !std::isfinite(centerZ)) {
            return Fail("native scene road transform exceeds finite display coordinates", error);
        }
        plan.roads.push_back({road->id, centerX, centerZ, width, depth, road->enabled});
        displayBounds.Include(centerX - width * 0.5, centerZ - depth * 0.5,
                              centerX + width * 0.5, centerZ + depth * 0.5);
    }

    for (const auto &vehicle : snapshot.vehicles) {
        const NativePoint center = transform.CellCenter(vehicle.cell);
        if (!std::isfinite(center.x) || !std::isfinite(center.z)) {
            return Fail("native scene vehicle transform exceeds finite display coordinates", error);
        }
        constexpr double vehicleCellSize = NativeEconomyTransform::DisplayUnitsPerCell;
        displayBounds.Include(center.x - vehicleCellSize * 0.5, center.z - vehicleCellSize * 0.5,
                              center.x + vehicleCellSize * 0.5, center.z + vehicleCellSize * 0.5);
    }

    if (displayBounds.Empty()) {
        const double fallbackWidth = static_cast<double>(std::min(snapshot.gridWidth, 16u));
        const double fallbackDepth = static_cast<double>(std::min(snapshot.gridDepth, 16u));
        plan.minX = (0.0 - transform.centerCellX) * NativeEconomyTransform::DisplayUnitsPerCell;
        plan.maxX = (fallbackWidth - transform.centerCellX) *
                    NativeEconomyTransform::DisplayUnitsPerCell;
        plan.minZ = (0.0 - transform.centerCellZ) * NativeEconomyTransform::DisplayUnitsPerCell;
        plan.maxZ = (fallbackDepth - transform.centerCellZ) *
                    NativeEconomyTransform::DisplayUnitsPerCell;
    } else {
        plan.minX = displayBounds.minX;
        plan.minZ = displayBounds.minZ;
        plan.maxX = displayBounds.maxX;
        plan.maxZ = displayBounds.maxZ;
    }

    if (!std::isfinite(plan.minX) || !std::isfinite(plan.minZ) ||
        !std::isfinite(plan.maxX) || !std::isfinite(plan.maxZ)) {
        return Fail("native scene bounds exceed finite display coordinates", error);
    }
    const double spanX = plan.maxX - plan.minX;
    const double spanZ = plan.maxZ - plan.minZ;
    if (!std::isfinite(spanX) || !std::isfinite(spanZ)) {
        return Fail("native scene span exceeds finite display coordinates", error);
    }
    plan.cameraDistance = NativeEconomyCameraDistance(spanX, spanZ);
    if (!std::isfinite(plan.cameraDistance)) {
        return Fail("native scene camera distance exceeds finite coordinates", error);
    }

    outPlan = std::move(plan);
    error.clear();
    return true;
}

bool NativeEconomyPlanFitsFloatPrecision(const NativeEconomyScenePlan &plan,
                                         std::string &error) {
    constexpr double maxConversionError = 0.0625;
    constexpr double maxFloatUlp = 0.125;
    const float maxFloat = std::numeric_limits<float>::max();
    auto checkCoordinate = [&](double value, std::string_view label) {
        if (!std::isfinite(value) || value > static_cast<double>(maxFloat) ||
            value < -static_cast<double>(maxFloat)) {
            error.assign(label.data(), label.size());
            error.append(" is not a finite float-range display coordinate");
            return false;
        }
        const float converted = static_cast<float>(value);
        if (!std::isfinite(converted) || std::abs(static_cast<double>(converted) - value) >
                                             maxConversionError) {
            error.assign(label.data(), label.size());
            error.append(" exceeds float conversion precision");
            return false;
        }
        const float towardPositive = std::nextafter(converted, std::numeric_limits<float>::infinity());
        const float towardNegative = std::nextafter(converted, -std::numeric_limits<float>::infinity());
        double ulp = 0.0;
        if (std::isfinite(towardPositive)) {
            ulp = std::max(ulp, std::abs(static_cast<double>(towardPositive) - converted));
        }
        if (std::isfinite(towardNegative)) {
            ulp = std::max(ulp, std::abs(static_cast<double>(converted) - towardNegative));
        }
        if (ulp > maxFloatUlp) {
            error.assign(label.data(), label.size());
            error.append(" has float spacing larger than 0.125 display units");
            return false;
        }
        return true;
    };

    if (!(plan.minX <= plan.maxX) || !(plan.minZ <= plan.maxZ)) {
        error = "native scene bounds are inverted";
        return false;
    }
    const double scalarValues[] = {plan.minX, plan.minZ, plan.maxX, plan.maxZ,
                                   plan.cameraDistance};
    for (double value : scalarValues) {
        if (!std::isfinite(value)) {
            error = "native scene bounds or camera distance are not finite";
            return false;
        }
    }
    if (plan.cameraDistance <= 0.0) {
        error = "native scene camera distance is not positive";
        return false;
    }

    if (!checkCoordinate(plan.minX, "native scene minX") ||
        !checkCoordinate(plan.minZ, "native scene minZ") ||
        !checkCoordinate(plan.maxX, "native scene maxX") ||
        !checkCoordinate(plan.maxZ, "native scene maxZ") ||
        !checkCoordinate(plan.cameraDistance, "native scene camera distance")) {
        return false;
    }

    for (const auto &building : plan.buildings) {
        if (!(building.width > 0.0) || !(building.depth > 0.0) || !(building.height > 0.0)) {
            error = "native building dimensions must be positive";
            return false;
        }
        const double coordinates[] = {building.centerX, building.centerZ, building.width,
                                      building.depth, building.height,
                                      building.centerX - building.width * 0.5,
                                      building.centerX + building.width * 0.5,
                                      building.centerZ - building.depth * 0.5,
                                      building.centerZ + building.depth * 0.5};
        const std::string_view labels[] = {"native building centerX", "native building centerZ",
                                          "native building width", "native building depth",
                                          "native building height", "native building minX",
                                          "native building maxX", "native building minZ",
                                          "native building maxZ"};
        for (std::size_t i = 0; i < sizeof(coordinates) / sizeof(coordinates[0]); ++i) {
            if (!checkCoordinate(coordinates[i], labels[i])) {
                return false;
            }
        }
    }

    for (const auto &road : plan.roads) {
        if (!(road.width > 0.0) || !(road.depth > 0.0)) {
            error = "native road dimensions must be positive";
            return false;
        }
        const double coordinates[] = {road.centerX, road.centerZ, road.width, road.depth,
                                      road.centerX - road.width * 0.5,
                                      road.centerX + road.width * 0.5,
                                      road.centerZ - road.depth * 0.5,
                                      road.centerZ + road.depth * 0.5};
        const std::string_view labels[] = {"native road centerX", "native road centerZ",
                                          "native road width", "native road depth",
                                          "native road minX", "native road maxX",
                                          "native road minZ", "native road maxZ"};
        for (std::size_t i = 0; i < sizeof(coordinates) / sizeof(coordinates[0]); ++i) {
            if (!checkCoordinate(coordinates[i], labels[i])) {
                return false;
            }
        }
    }

    error.clear();
    return true;
}

} // namespace openrepublic::runtime
