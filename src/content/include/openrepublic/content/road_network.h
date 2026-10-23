#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

struct RoadNode {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // Node indices connected to this node (the file stores them symmetrically).
    std::vector<std::uint32_t> neighbors;
    // Segment ids parallel to `neighbors` (INFERRED from the shared ordering).
    std::vector<std::uint32_t> segmentIds;
    // Per-connection value parallel to `neighbors`; it equals the same id for
    // every road of one kind in the sampled data, so it is probably a road
    // type id. Meaning UNKNOWN.
    std::vector<std::uint32_t> typeIds;
};

struct RoadPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct RoadSegment {
    // Structural fields copied from the segment record header by the matching
    // reader. Their serialized offsets are 0x00, 0x04, 0x08, 0x34, and 0x84.
    std::uint32_t pointCount = 0;
    std::uint32_t nodeA = 0;
    std::uint32_t nodeB = 0;
    std::uint32_t nestedRecordCount = 0;
    bool hasTrailingVectors = false;
    // Present only when endpoint connection arrays agree on this value.
    // The game's semantic meaning for the value is not yet known.
    std::optional<std::uint32_t> typeId;
    // Exact serialized header bytes. The matching writer/reader maps more
    // ranges to members on the native object; most meanings remain unknown.
    // In particular, no width, lane, material, speed, or vertical-offset
    // interpretation is established. Retain the bytes for later analysis.
    std::array<std::uint8_t, 0x88> rawHeader{};
    // The 0x18-record XYZ sequence. Centerline interpretation is INFERRED;
    // points are retained in file order.
    std::vector<RoadPoint> polyline;
};

struct RoadNetwork {
    std::vector<RoadNode> nodes;
    // Counts from the header.
    std::uint32_t segmentCount = 0;
    std::uint32_t groupCount = 0;
    // Geometry may be a safely decoded prefix if an unsupported conditional
    // tail is encountered in a segment record.
    std::vector<RoadSegment> segments;
    bool segmentGeometryComplete = false;
    std::uint32_t unparsedSegmentCount = 0;
    std::string segmentGeometryDiagnostic;
    // Neighbor links whose reverse link is missing (0 in all 66 prior samples).
    std::size_t asymmetricLinks = 0;
};

constexpr std::size_t MaxRoadNodes = 1u << 20;
constexpr std::size_t MaxRoadNodeLinks = 64;
constexpr std::size_t MaxRoadSegments = 1u << 20;
constexpr std::size_t MaxRoadPolylinePoints = 1u << 20;

// Returns samples at equal distances along the polyline, including endpoints
// when sampleCount > 1. Empty input or zero samples returns an empty vector.
std::vector<RoadPoint> SampleRoadPolyline(const std::vector<RoadPoint>& polyline,
                                         std::size_t sampleCount);

// Extracts the junction graph from a `road.bin`-style file (OBSERVED layout in
// one WRSR build, previously validated on 66 files): three little-endian u32 counts
// (nodes, segments, groups) followed by one node record per node. A node
// record is a 0x48-byte block - float x, y, z, then u32 counts of the four
// per-connection arrays and of a further triple of arrays (the first four
// counts always matched the neighbor count in the samples) - followed by those
// arrays of u32 (three arrays of the fifth count first, then neighbors,
// segment ids, type ids and one more per-connection word). Positions are in
// map metres. Every link must reference an existing node. Segment geometry
// is decoded while its serialized length can be determined; unsupported
// conditional tails stop parsing and leave a safe prefix.
// On failure output is unchanged.
bool ExtractRoadNetwork(std::string_view bytes, RoadNetwork& output,
                        std::string* error = nullptr);

} // namespace openrepublic::content
