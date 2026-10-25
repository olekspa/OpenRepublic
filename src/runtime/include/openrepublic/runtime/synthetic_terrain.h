#pragma once

#include <cstdint>

namespace openrepublic::runtime {

/** Deterministic OpenRepublic presentation terrain. Heights and coordinates are display units. */
struct SyntheticTerrain {
    std::uint32_t seed = 20261025u;
    double waterLevel = 0.0;

    /** Smooth height lookup; coordinates are centered display-space X/Z. */
    double HeightAt(double x, double z) const noexcept;
    /** Central-difference gradient magnitude (rise/run) at a point. */
    double SlopeAt(double x, double z) const noexcept;
    /** True when the full rectangle stays under the documented 0.22 maximum gradient. */
    bool IsBuildable(double centerX, double centerZ, double width, double depth,
                     double maxSlope = 0.22) const noexcept;
    /** Lake mask used to place the synthetic water surface. */
    bool IsWater(double x, double z) const noexcept;
};

} // namespace openrepublic::runtime
