#pragma once

#include "openrepublic/content/nmf_geometry.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace openrepublic::content {

inline constexpr double NmfPreviewDisplaySpan = 10.0;
inline constexpr double NmfPreviewCenterX = 8.0;
inline constexpr double NmfPreviewCameraDistanceFactor = 1.65;
inline constexpr std::uint32_t NoNmfPreviewSubmaterialIndex =
    std::numeric_limits<std::uint32_t>::max();

struct NmfPreviewDrawRange {
    std::uint32_t indexOffset{};
    std::uint32_t indexCount{};
    std::uint32_t submaterialIndex{NoNmfPreviewSubmaterialIndex};
};

struct NmfPreviewGeometry {
    // Positions use the current diagnostic preview transform: positive uniform
    // scale, X/Z recentered, minimum Y moved to zero. No axis or winding change.
    std::vector<NmfPosition> positions;
    // Source normals are retained byte-for-value at the parsed float level;
    // mask entries distinguish them from zero-filled vertices without normals.
    std::vector<NmfNormal> sourceNormals;
    std::vector<std::uint8_t> sourceNormalMask;
    // The flattened index stream preserves object order and every source index.
    std::vector<std::uint32_t> indices;
    std::vector<NmfPreviewDrawRange> drawRanges;

    NmfPosition sourceMinimum{};
    NmfPosition sourceMaximum{};
    NmfPosition cameraTarget{};
    double displayScale{};
    float cameraDistance{};
    std::size_t sourceNormalCount{};
    std::size_t ignoredSubsetIndexCount{};
    std::size_t renderedTriangleCount{};
    std::size_t zeroAreaTriangleCount{};
    double sourceNormalLengthMinimum{};
    double sourceNormalLengthMean{};
    double sourceNormalLengthMaximum{};
};

// Builds the app's pure CPU-side geometry preview handoff from parsed static
// objects. It does not interpret stored bounds or node matrices, modify source
// axes/winding/normals, or claim that the preview transform matches the game.
// Only complete triangles in each observed subset draw range are counted. On
// failure, output is unchanged.
bool BuildNmfPreviewGeometry(const NmfStaticGeometry& geometry,
                             NmfPreviewGeometry& output,
                             std::string* error = nullptr);

} // namespace openrepublic::content
