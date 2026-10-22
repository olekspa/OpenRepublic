#pragma once

#include "openrepublic/runtime/native_economy_layout.h"

#include <vector>

namespace openrepublic::runtime {

/** Converts a centered display-space point back to its native grid cell.
 *  Each cell occupies a half-open four-display-unit interval. On failure,
 *  including non-finite input, invalid dimensions, or an out-of-grid point,
 *  outCell is left unchanged. */
bool NativeDisplayPointToCell(const NativeEconomyTransform &transform, double worldX,
                              double worldZ, std::uint32_t gridWidth,
                              std::uint32_t gridDepth, economy::Cell &outCell) noexcept;

/** Lists resources both exact native endpoints can store, in catalog ID order.
 *  Positive-capacity storage slots define eligibility independently of current
 *  inventory, enabled state, or recipe structure. Invalid endpoints return empty. */
std::vector<economy::ResourceId>
ListNativeHaulResources(const economy::WorldSnapshot &snapshot,
                        const economy::Catalog &catalog,
                        economy::BuildingId source, economy::BuildingId destination);

} // namespace openrepublic::runtime
