#pragma once

#include "openrepublic/content/terrain_dds.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openrepublic::content {

// Height-only preview grid sampled across the caller-supplied terrain extent.
// Heights are row-major with row zero at the transform's minimum world Z and
// the final row at its maximum world Z. Horizontal coordinates are deliberately
// not stored so callers can choose their own centered or world-space mesh grid.
struct TerrainPreviewGrid {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<float> heights;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
};

// Samples a bounded regular preview grid over the inclusive XZ extent in the
// caller's TerrainTransform. The transform is the sole source of world extent,
// vertical scaling/base height, and raster row orientation. Each output row is
// in increasing world-Z order regardless of source raster orientation.
//
// Both dimensions must be >= 2, at most 4096, and the total point count at most
// 4,194,304. On failure, output remains unchanged and error receives a
// diagnostic when non-null.
bool BuildTerrainPreviewGrid(const Heightmap& heightmap, const TerrainTransform& transform,
                             std::uint32_t previewWidth, std::uint32_t previewHeight,
                             TerrainPreviewGrid& output, std::string* error = nullptr);

} // namespace openrepublic::content
