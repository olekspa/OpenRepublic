#pragma once

#include "openrepublic/content/road_network.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

struct LinearNetworkNode {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // Four serialized u32 arrays retained without assigning game semantics.
    std::vector<std::uint32_t> links[4];
};

struct LinearNetworkSegment {
    std::uint32_t nodeA = 0;
    std::uint32_t nodeB = 0;
    // Ordered XYZ records from the candidate 0x18-byte stream. Their use is
    // family-specific and remains INFERRED/UNKNOWN (especially power lines).
    std::vector<RoadPoint> points;
};

struct LinearNetwork {
    std::vector<LinearNetworkNode> nodes;
    std::uint32_t segmentCount = 0;
    std::uint32_t groupCount = 0;
    std::vector<LinearNetworkSegment> segments;
    bool segmentGeometryComplete = false;
    std::uint32_t unparsedSegmentCount = 0;
    std::string segmentDiagnostic;
};

constexpr std::size_t MaxLinearNetworkNodes = 1u << 20;
constexpr std::size_t MaxLinearNetworkSegments = 1u << 20;
constexpr std::size_t MaxLinearNetworkLinksPerNode = 1u << 16;
constexpr float MaxLinearNetworkWorldCoordinate = 20000.0f;

// Decodes the observed 12-byte header and variable-length node table from
// rail/electro/fence/pipe network files. Node block/array layout is OBSERVED
// for WRSR build 23935965. Candidate segment geometry follows the road decoder's
// checked record walk; it is exposed only as ordered points with unknown meaning.
// On failure, output is unchanged.
bool ExtractLinearNetwork(std::string_view bytes, LinearNetwork& output,
                          std::string* error = nullptr);

} // namespace openrepublic::content
