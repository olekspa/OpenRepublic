#include "openrepublic/content/terrain_preview.h"

#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::uint32_t kMaxPreviewDimension = 4096;
constexpr std::uint64_t kMaxPreviewPoints = 4'194'304;

bool Fail(std::string* error, const char* message) {
    if (error != nullptr)
        *error = message;
    return false;
}

} // namespace

bool BuildTerrainPreviewGrid(const Heightmap& heightmap, const TerrainTransform& transform,
                             std::uint32_t previewWidth, std::uint32_t previewHeight,
                             TerrainPreviewGrid& output, std::string* error) {
    if (previewWidth < 2 || previewHeight < 2)
        return Fail(error, "terrain preview dimensions must both be at least 2");
    if (previewWidth > kMaxPreviewDimension || previewHeight > kMaxPreviewDimension)
        return Fail(error, "terrain preview dimension exceeds 4096 samples");

    const std::uint64_t pointCount = static_cast<std::uint64_t>(previewWidth) * previewHeight;
    if (pointCount > kMaxPreviewPoints || pointCount > std::numeric_limits<std::size_t>::max())
        return Fail(error, "terrain preview sample count exceeds the configured bound");

    const double maxX = transform.originX + transform.sizeX;
    const double maxZ = transform.originZ + transform.sizeZ;
    if (!std::isfinite(transform.originX) || !std::isfinite(transform.originZ) ||
        !std::isfinite(transform.sizeX) || !std::isfinite(transform.sizeZ) ||
        !std::isfinite(maxX) || !std::isfinite(maxZ) ||
        transform.sizeX <= 0.0 || transform.sizeZ <= 0.0 ||
        maxX <= transform.originX || maxZ <= transform.originZ) {
        return Fail(error, "terrain preview transform must have finite, positive, representable XZ extents");
    }

    TerrainPreviewGrid candidate;
    candidate.width = previewWidth;
    candidate.height = previewHeight;
    try {
        candidate.worldXCoordinates.resize(previewWidth);
        candidate.worldZCoordinates.resize(previewHeight);
        candidate.heights.resize(static_cast<std::size_t>(pointCount));
    } catch (const std::bad_alloc&) {
        return Fail(error, "unable to allocate bounded terrain preview grid");
    } catch (const std::length_error&) {
        return Fail(error, "terrain preview grid size is not representable");
    }

    double previousX = transform.originX;
    for (std::uint32_t x = 0; x < previewWidth; ++x) {
        const double fractionX = static_cast<double>(x) / static_cast<double>(previewWidth - 1);
        const double worldX = x == 0 ? transform.originX :
                              (x + 1 == previewWidth ? maxX : transform.originX + transform.sizeX * fractionX);
        if (!std::isfinite(worldX))
            return Fail(error, "terrain preview X coordinate is not finite");
        if (x != 0 && worldX <= previousX)
            return Fail(error, "terrain preview X coordinates collapse at the requested resolution");
        previousX = worldX;
        candidate.worldXCoordinates[x] = worldX;
    }

    double previousZ = transform.originZ;
    for (std::uint32_t y = 0; y < previewHeight; ++y) {
        const double fractionZ = static_cast<double>(y) / static_cast<double>(previewHeight - 1);
        const double worldZ = y == 0 ? transform.originZ :
                              (y + 1 == previewHeight ? maxZ : transform.originZ + transform.sizeZ * fractionZ);
        if (!std::isfinite(worldZ))
            return Fail(error, "terrain preview Z coordinate is not finite");
        if (y != 0 && worldZ <= previousZ)
            return Fail(error, "terrain preview Z coordinates collapse at the requested resolution");
        previousZ = worldZ;
        candidate.worldZCoordinates[y] = worldZ;
    }

    bool first = true;
    std::string samplingError;
    for (std::uint32_t y = 0; y < previewHeight; ++y) {
        const double worldZ = candidate.worldZCoordinates[y];
        for (std::uint32_t x = 0; x < previewWidth; ++x) {
            const double worldX = candidate.worldXCoordinates[x];
            double sampledHeight = 0.0;
            samplingError.clear();
            if (!SampleTerrainHeight(heightmap, transform, worldX, worldZ, sampledHeight, &samplingError)) {
                if (error != nullptr)
                    *error = "terrain preview sample failed: " + samplingError;
                return false;
            }
            if (!std::isfinite(sampledHeight) ||
                sampledHeight < -std::numeric_limits<float>::max() ||
                sampledHeight > std::numeric_limits<float>::max()) {
                return Fail(error, "terrain preview height is not representable as a finite float");
            }

            const float value = static_cast<float>(sampledHeight);
            if (!std::isfinite(value))
                return Fail(error, "terrain preview height is not finite");
            candidate.heights[static_cast<std::size_t>(y) * previewWidth + x] = value;
            if (first) {
                candidate.minHeight = value;
                candidate.maxHeight = value;
                first = false;
            } else {
                if (value < candidate.minHeight)
                    candidate.minHeight = value;
                if (value > candidate.maxHeight)
                    candidate.maxHeight = value;
            }
        }
    }

    output = std::move(candidate);
    if (error != nullptr)
        error->clear();
    return true;
}

bool BuildTerrainRegionGrid(const Heightmap& heightmap, const TerrainTransform& transform,
                            double regionMinX, double regionMinZ, double regionSizeX,
                            double regionSizeZ, std::uint32_t previewWidth,
                            std::uint32_t previewHeight, TerrainPreviewGrid& output,
                            std::string* error) {
    if (previewWidth < 2 || previewHeight < 2)
        return Fail(error, "terrain region dimensions must both be at least 2");
    if (previewWidth > kMaxPreviewDimension || previewHeight > kMaxPreviewDimension ||
        static_cast<std::uint64_t>(previewWidth) * previewHeight > kMaxPreviewPoints)
        return Fail(error, "terrain region sample count exceeds the configured bound");
    if (!std::isfinite(regionMinX) || !std::isfinite(regionMinZ) || !std::isfinite(regionSizeX) ||
        !std::isfinite(regionSizeZ) || regionSizeX <= 0.0 || regionSizeZ <= 0.0)
        return Fail(error, "terrain region must be finite with positive size");
    if (regionMinX < transform.originX || regionMinZ < transform.originZ ||
        regionMinX + regionSizeX > transform.originX + transform.sizeX ||
        regionMinZ + regionSizeZ > transform.originZ + transform.sizeZ)
        return Fail(error, "terrain region lies outside the terrain extent");

    TerrainPreviewGrid candidate;
    candidate.width = previewWidth;
    candidate.height = previewHeight;
    try {
        candidate.worldXCoordinates.resize(previewWidth);
        candidate.worldZCoordinates.resize(previewHeight);
        candidate.heights.resize(static_cast<std::size_t>(previewWidth) * previewHeight);
    } catch (const std::bad_alloc&) {
        return Fail(error, "unable to allocate bounded terrain region grid");
    }
    for (std::uint32_t x = 0; x < previewWidth; ++x)
        candidate.worldXCoordinates[x] = regionMinX + regionSizeX * x / (previewWidth - 1);
    for (std::uint32_t z = 0; z < previewHeight; ++z)
        candidate.worldZCoordinates[z] = regionMinZ + regionSizeZ * z / (previewHeight - 1);

    bool first = true;
    std::string samplingError;
    for (std::uint32_t z = 0; z < previewHeight; ++z) {
        for (std::uint32_t x = 0; x < previewWidth; ++x) {
            double height = 0.0;
            if (!SampleTerrainHeight(heightmap, transform, candidate.worldXCoordinates[x],
                                     candidate.worldZCoordinates[z], height, &samplingError)) {
                if (error != nullptr) *error = "terrain region sample failed: " + samplingError;
                return false;
            }
            const float value = static_cast<float>(height);
            if (!std::isfinite(value)) return Fail(error, "terrain region height is not finite");
            candidate.heights[static_cast<std::size_t>(z) * previewWidth + x] = value;
            if (first || value < candidate.minHeight) candidate.minHeight = value;
            if (first || value > candidate.maxHeight) candidate.maxHeight = value;
            first = false;
        }
    }
    output = std::move(candidate);
    if (error != nullptr) error->clear();
    return true;
}

} // namespace openrepublic::content
