#include "openrepublic/content/road_network.h"

#include <cmath>
#include <cstring>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kNodeBlockBytes = 0x48;
constexpr std::size_t kHeaderBytes = 12;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

std::uint32_t ReadU32(std::string_view bytes, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

float ReadF32(std::string_view bytes, std::size_t offset) {
    float value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

} // namespace

bool ExtractRoadNetwork(std::string_view bytes, RoadNetwork& output, std::string* error) {
    if (bytes.size() < kHeaderBytes) return Fail(error, "road file is shorter than its header");
    const std::uint32_t nodeCount = ReadU32(bytes, 0);
    if (nodeCount > MaxRoadNodes) return Fail(error, "road node count exceeds the limit");

    RoadNetwork candidate;
    candidate.segmentCount = ReadU32(bytes, 4);
    candidate.groupCount = ReadU32(bytes, 8);
    candidate.nodes.reserve(nodeCount);

    std::size_t offset = kHeaderBytes;
    for (std::uint32_t i = 0; i < nodeCount; ++i) {
        if (offset + kNodeBlockBytes > bytes.size())
            return Fail(error, "road node " + std::to_string(i) + " is truncated");
        RoadNode node;
        node.x = ReadF32(bytes, offset);
        node.y = ReadF32(bytes, offset + 4);
        node.z = ReadF32(bytes, offset + 8);
        if (!std::isfinite(node.x) || !std::isfinite(node.y) || !std::isfinite(node.z))
            return Fail(error, "road node " + std::to_string(i) + " has a non-finite position");
        const std::uint32_t neighborCount = ReadU32(bytes, offset + 12);
        const std::uint32_t segmentCount = ReadU32(bytes, offset + 16);
        const std::uint32_t typeCount = ReadU32(bytes, offset + 20);
        const std::uint32_t extraCount = ReadU32(bytes, offset + 24);
        const std::uint32_t tripleCount = ReadU32(bytes, offset + 28);
        if (neighborCount > MaxRoadNodeLinks || segmentCount > MaxRoadNodeLinks ||
            typeCount > MaxRoadNodeLinks || extraCount > MaxRoadNodeLinks ||
            tripleCount > MaxRoadNodeLinks)
            return Fail(error, "road node " + std::to_string(i) + " has an implausible link count");

        const std::size_t words = 3u * tripleCount + neighborCount + segmentCount + typeCount + extraCount;
        const std::size_t end = offset + kNodeBlockBytes + words * 4;
        if (end > bytes.size()) return Fail(error, "road node " + std::to_string(i) + " arrays are truncated");

        std::size_t cursor = offset + kNodeBlockBytes + 3u * tripleCount * 4;
        for (std::uint32_t k = 0; k < neighborCount; ++k, cursor += 4) {
            node.neighbors.push_back(ReadU32(bytes, cursor));
        }
        for (std::uint32_t k = 0; k < segmentCount; ++k, cursor += 4) {
            node.segmentIds.push_back(ReadU32(bytes, cursor));
        }
        for (std::uint32_t k = 0; k < typeCount; ++k, cursor += 4) {
            node.typeIds.push_back(ReadU32(bytes, cursor));
        }
        candidate.nodes.push_back(std::move(node));
        offset = end;
    }

    for (std::size_t i = 0; i < candidate.nodes.size(); ++i) {
        for (const std::uint32_t neighbor : candidate.nodes[i].neighbors) {
            if (neighbor >= candidate.nodes.size())
                return Fail(error, "road node " + std::to_string(i) + " links to a missing node");
            bool reverse = false;
            for (const std::uint32_t back : candidate.nodes[neighbor].neighbors) {
                reverse = reverse || back == i;
            }
            if (!reverse) ++candidate.asymmetricLinks;
        }
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
