#pragma once

#include "openrepublic/economy/economy.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openrepublic::runtime {

/** A point in the native economy's horizontal cell plane, before display scaling. */
struct NativePoint {
    double x{};
    double z{};
};
struct NativeEconomyTransform;

/** Presentation-only interpolation of one cell-quantized vehicle sample pair. */
struct NativeVehicleMotion {
    NativePoint position;
    double yawRadians{};
    double wheelRotationRadians{};
    double traveledDisplayUnits{};
    bool moving{};
};

/** Presentation scale for the procedural vehicle mesh, sized to a fraction of a cell. */
double NativeVehicleDisplayScale(double modelLength,
                                 double targetCellFraction = 0.75) noexcept;

/** Camera distance policy for a compact desktop economy view. */
double NativeEconomyCameraDistance(double spanX, double spanZ,
                                   double distanceScale = 1.0) noexcept;

/** Interpolates within one cell step; jumps larger than one cell snap to current.
 *  yaw is radians about +Y with +Z as forward. Stationary samples retain lastYaw.
 *  Wheel rotation is traveled display distance divided by the supplied wheel radius. */
NativeVehicleMotion InterpolateNativeVehicleMotion(
    economy::Cell previous, economy::Cell current, double alpha, double lastYawRadians,
    const NativeEconomyTransform &transform, double wheelRadiusDisplayUnits = 0.48) noexcept;

/** Presentation-only mapping from native grid coordinates to centered display units.
 *  Four display units per cell is a renderer policy; native economy coordinates do
 *  not declare physical units. The center values are cell-boundary coordinates. */
struct NativeEconomyTransform {
    static constexpr double DisplayUnitsPerCell = 4.0;

    double centerCellX{};
    double centerCellZ{};

    /** Maps a cell center to centered display-space X/Z. */
    NativePoint CellCenter(economy::Cell cell) const noexcept;
    /** Maps a building footprint center to centered display-space X/Z. */
    NativePoint FootprintCenter(economy::Cell origin, std::uint32_t width,
                                std::uint32_t depth) const noexcept;
};

/** Full-size renderer input for one native building footprint. */
struct NativeBuildingPrimitive {
    economy::BuildingId buildingId{};
    economy::BuildingTypeId typeId{};
    double centerX{};
    double centerZ{};
    double width{};
    double depth{};
    double height{};
};

/** Full-size renderer input for one native unit road segment. */
struct NativeRoadPrimitive {
    economy::RoadId roadId{};
    double centerX{};
    double centerZ{};
    double width{};
    double depth{};
    bool enabled{true};
};

/** Renderer-independent geometry for one immutable economy snapshot. */
struct NativeEconomyScenePlan {
    NativeEconomyTransform transform;
    std::vector<NativeBuildingPrimitive> buildings;
    std::vector<NativeRoadPrimitive> roads;
    double minX{};
    double minZ{};
    double maxX{};
    double maxZ{};
    double cameraDistance{};
};

/** Builds a complete presentation plan transactionally.
 *  If fixedTransform is null, the transform is centered on all occupied geometry
 *  (building footprints/access cells, roads, and vehicle cells), or on the first
 *  at-most-16-by-16 cells for an empty world. A provided transform is kept verbatim
 *  as the world changes. Malformed geometry fails without changing outPlan. */
bool BuildNativeEconomyScenePlan(const economy::WorldSnapshot &snapshot,
                                 const economy::Catalog &catalog,
                                 NativeEconomyScenePlan &outPlan,
                                 std::string &error,
                                 const NativeEconomyTransform *fixedTransform = nullptr);

/** Checks whether a double-precision plan can be represented safely by a float renderer.
 *  Every emitted center, scene bound, and primitive corner must be finite, within float
 *  range, within 0.0625 display units of its float conversion, and have local float ULP
 *  no greater than 0.125 display units. This is a separate view-boundary check: large
 *  native worlds remain valid double-precision plans even when a renderer cannot show
 *  their full span at the chosen fixed transform. */
bool NativeEconomyPlanFitsFloatPrecision(const NativeEconomyScenePlan &plan,
                                         std::string &error);

} // namespace openrepublic::runtime
