#pragma once

#include "openrepublic/content/road_network.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

struct LinearNetworkBuildingReference {
    std::int32_t buildingIndex = -1;
    std::int32_t opaqueA = 0;
    std::int32_t opaqueB = 0;
};

struct LinearNetworkNode {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // Four serialized u32 arrays retained without assigning game semantics.
    std::vector<std::uint32_t> links[4];
    // Static read/write tracing observed three parallel signed arrays before
    // the four links arrays. The first values index a building table; the
    // remaining values are preserved without assigning type/flag semantics.
    // Correspondence to scanner placement indices is not runtime-verified.
    std::vector<LinearNetworkBuildingReference> buildingReferences;
};

struct LinearNetworkSegment {
    std::uint32_t nodeA = 0;
    std::uint32_t nodeB = 0;
    // Ordered XYZ records from the candidate 0x18-byte stream. Their use is
    // family-specific and remains INFERRED/UNKNOWN (especially power lines).
    std::vector<RoadPoint> points;
};

// Rendering-facing copy of one decoded power-network point sequence. The
// points are candidate line geometry only: the decoder evidence does not
// identify poles, cable attachment points, or a sag model.
struct PowerLinePolylineCandidate {
    std::uint32_t nodeA = 0;
    std::uint32_t nodeB = 0;
    std::vector<RoadPoint> points;
};

// Candidate attachment to a source-indexed placement table. This preserves
// the serialized building-table index and opaque fields; it does not classify
// the node as a pole/support or connect a simulation building automatically.
struct NetworkBuildingAttachmentCandidate {
    std::uint32_t nodeIndex = 0;
    std::uint32_t buildingIndex = 0;
    std::int32_t opaqueA = 0;
    std::int32_t opaqueB = 0;
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

// Copies complete, validated segment point sequences for a power-line preview.
// Header nodes remain endpoint candidates and are not classified as pylons.
// An incomplete geometry prefix or invalid segment reference fails without
// changing output.
bool ExtractPowerLinePolylineCandidates(const LinearNetwork& network,
                                       std::vector<PowerLinePolylineCandidate>& output,
                                       std::string* error = nullptr);

// Converts valid serialized building-table references into render/import
// candidates in source order. placementCount must describe the exact source
// building table. Invalid references, coordinates or bounds fail transactionally.
bool ExtractNetworkBuildingAttachmentCandidates(
    const LinearNetwork& network, std::size_t placementCount,
    std::vector<NetworkBuildingAttachmentCandidate>& output,
    std::string* error = nullptr);

} // namespace openrepublic::content
