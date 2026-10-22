#pragma once

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

struct LinearNetwork {
    std::vector<LinearNetworkNode> nodes;
    std::uint32_t segmentCount = 0;
    std::uint32_t groupCount = 0;
    // Segment records are intentionally not interpreted until their family
    // specific serialized lengths are established from authoritative evidence.
    std::uint32_t unparsedSegmentCount = 0;
    std::string segmentDiagnostic;
};

constexpr std::size_t MaxLinearNetworkNodes = 1u << 20;
constexpr std::size_t MaxLinearNetworkSegments = 1u << 20;
constexpr std::size_t MaxLinearNetworkLinksPerNode = 1u << 16;
constexpr float MaxLinearNetworkWorldCoordinate = 20000.0f;

// Decodes the observed 12-byte header and variable-length node table from
// rail/electro/fence/pipe network files. Node block/array layout is OBSERVED
// for WRSR build 23935965. Segment geometry remains explicitly unavailable.
// On failure, output is unchanged.
bool ExtractLinearNetwork(std::string_view bytes, LinearNetwork& output,
                          std::string* error = nullptr);

} // namespace openrepublic::content
