#pragma once

#include "openrepublic/content/terrain_dds.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openrepublic::content {

// Preview grid sampled across the caller-supplied terrain extent. Heights are
// row-major; worldXCoordinates are ascending and shared by every row, while
// worldZCoordinates are ascending and shared by every column. These coordinates
// describe the caller's transform, not an inferred game coordinate contract.
struct TerrainPreviewGrid {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<double> worldXCoordinates;
    std::vector<double> worldZCoordinates;
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

// Like BuildTerrainPreviewGrid, but samples only the axis-aligned world region
// [regionMinX, regionMinX + regionSizeX] x [regionMinZ, regionMinZ + regionSizeZ]
// out of the full terrain described by `transform`. The region must lie inside
// the transform's extent and have positive size; the same dimension and sample
// bounds apply. Rows ascend in world Z. On failure output is unchanged.
bool BuildTerrainRegionGrid(const Heightmap& heightmap, const TerrainTransform& transform,
                            double regionMinX, double regionMinZ, double regionSizeX,
                            double regionSizeZ, std::uint32_t previewWidth,
                            std::uint32_t previewHeight, TerrainPreviewGrid& output,
                            std::string* error = nullptr);

} // namespace openrepublic::content
