#pragma once

#include <cstddef>
#include <cstdint>
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

struct RoadNetwork {
    std::vector<RoadNode> nodes;
    // Counts from the header; segment and group records are not decoded.
    std::uint32_t segmentCount = 0;
    std::uint32_t groupCount = 0;
    // Neighbor links whose reverse link is missing (0 in all 34 sampled files).
    std::size_t asymmetricLinks = 0;
};

constexpr std::size_t MaxRoadNodes = 1u << 20;
constexpr std::size_t MaxRoadNodeLinks = 64;

// Extracts the junction graph from a `road.bin`-style file (OBSERVED layout in
// one WRSR build, validated on 34 files): three little-endian u32 counts
// (nodes, segments, groups) followed by one node record per node. A node
// record is a 0x48-byte block - float x, y, z, then u32 counts of the four
// per-connection arrays and of a further triple of arrays (the first four
// counts always matched the neighbor count in the samples) - followed by those
// arrays of u32 (three arrays of the fifth count first, then neighbors,
// segment ids, type ids and one more per-connection word). Positions are in
// map metres. Every link must reference an existing node. Segment records
// (which hold the road polylines) come after the nodes and are not parsed.
// On failure output is unchanged.
bool ExtractRoadNetwork(std::string_view bytes, RoadNetwork& output,
                        std::string* error = nullptr);

} // namespace openrepublic::content
