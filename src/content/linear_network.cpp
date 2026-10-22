#include "openrepublic/content/linear_network.h"

#include <cmath>
#include <cstring>
#include <utility>

namespace openrepublic::content {
namespace {
constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kNodeBlockBytes = 0x48;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

std::uint32_t ReadU32(std::string_view bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

float ReadF32(std::string_view bytes, std::size_t offset) {
    float value = 0.0f;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
} // namespace

bool ExtractLinearNetwork(std::string_view bytes, LinearNetwork& output, std::string* error) {
    if (bytes.size() < kHeaderBytes) return Fail(error, "linear network is shorter than its header");
    const std::uint32_t nodeCount = ReadU32(bytes, 0);
    const std::uint32_t segmentCount = ReadU32(bytes, 4);
    const std::uint32_t groupCount = ReadU32(bytes, 8);
    if (nodeCount > MaxLinearNetworkNodes)
        return Fail(error, "linear network node count exceeds the limit");
    if (segmentCount > MaxLinearNetworkSegments)
        return Fail(error, "linear network segment count exceeds the limit");

    LinearNetwork candidate;
    candidate.segmentCount = segmentCount;
    candidate.groupCount = groupCount;
    candidate.unparsedSegmentCount = segmentCount;
    candidate.nodes.reserve(nodeCount);
    std::size_t offset = kHeaderBytes;
    for (std::uint32_t i = 0; i < nodeCount; ++i) {
        if (offset > bytes.size() || kNodeBlockBytes > bytes.size() - offset)
            return Fail(error, "linear network node " + std::to_string(i) + " block is truncated");
        LinearNetworkNode node;
        node.x = ReadF32(bytes, offset);
        node.y = ReadF32(bytes, offset + 4);
        node.z = ReadF32(bytes, offset + 8);
        if (!std::isfinite(node.x) || !std::isfinite(node.y) || !std::isfinite(node.z))
            return Fail(error, "linear network node " + std::to_string(i) + " has a non-finite position");
        if (std::abs(node.x) > MaxLinearNetworkWorldCoordinate ||
            std::abs(node.y) > MaxLinearNetworkWorldCoordinate ||
            std::abs(node.z) > MaxLinearNetworkWorldCoordinate)
            return Fail(error, "linear network node " + std::to_string(i) + " is outside the world bounds");

        std::uint32_t counts[5]{};
        for (std::size_t c = 0; c < 5; ++c) {
            counts[c] = ReadU32(bytes, offset + 12 + c * 4);
            if (counts[c] > MaxLinearNetworkLinksPerNode)
                return Fail(error, "linear network node " + std::to_string(i) + " has an excessive array count");
        }
        std::size_t words = 0;
        for (std::size_t c = 0; c < 5; ++c) {
            const std::size_t multiplier = c == 4 ? 3 : 1;
            if (counts[c] > (bytes.size() - offset - kNodeBlockBytes - words * 4) / (4 * multiplier))
                return Fail(error, "linear network node " + std::to_string(i) + " arrays are truncated");
            words += static_cast<std::size_t>(counts[c]) * multiplier;
        }
        std::size_t cursor = offset + kNodeBlockBytes + static_cast<std::size_t>(counts[4]) * 12;
        for (std::size_t array = 0; array < 4; ++array) {
            node.links[array].reserve(counts[array]);
            for (std::uint32_t j = 0; j < counts[array]; ++j, cursor += 4)
                node.links[array].push_back(ReadU32(bytes, cursor));
        }
        candidate.nodes.push_back(std::move(node));
        offset += kNodeBlockBytes + words * 4;
    }
    if (segmentCount != 0) {
        candidate.segmentDiagnostic = "segment record layout is not yet established for this network family; " +
                                      std::to_string(segmentCount) + " segments left unparsed";
    }
    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
