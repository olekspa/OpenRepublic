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

std::int32_t ReadI32(std::string_view bytes, std::size_t offset) {
    std::int32_t value = 0;
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
        std::size_t cursor = offset + kNodeBlockBytes;
        // OBSERVED static read/write layout: three parallel int32 arrays,
        // each with counts[4] entries, precede the four existing link arrays.
        // Only the first array is known to index the source building table;
        // opaqueA/opaqueB and placement-scanner correspondence stay unknown.
        node.buildingReferences.reserve(counts[4]);
        const std::size_t referenceBytes = static_cast<std::size_t>(counts[4]) * 12;
        const std::size_t referencesEnd = cursor + referenceBytes;
        for (std::uint32_t j = 0; j < counts[4]; ++j) {
            LinearNetworkBuildingReference reference;
            reference.buildingIndex = ReadI32(bytes, cursor + static_cast<std::size_t>(j) * 4);
            reference.opaqueA = ReadI32(bytes, cursor + static_cast<std::size_t>(counts[4] + j) * 4);
            reference.opaqueB = ReadI32(bytes, cursor + static_cast<std::size_t>(counts[4] * 2u + j) * 4);
            node.buildingReferences.push_back(reference);
        }
        cursor = referencesEnd;
        for (std::size_t array = 0; array < 4; ++array) {
            node.links[array].reserve(counts[array]);
            for (std::uint32_t j = 0; j < counts[array]; ++j, cursor += 4)
                node.links[array].push_back(ReadU32(bytes, cursor));
        }
        candidate.nodes.push_back(std::move(node));
        offset += kNodeBlockBytes + words * 4;
    }

    // Reuse the independently bounded segment walker already used for road.bin.
    // Keep the linear node decoder's full four-array view; the road view is only
    // used to validate endpoint indices and obtain safely decoded XYZ points.
    RoadNetwork geometry;
    std::string geometryError;
    if (!ExtractRoadNetwork(bytes, geometry, &geometryError))
        return Fail(error, "linear segment decode failed: " + geometryError);
    candidate.segments.reserve(geometry.segments.size());
    for (std::size_t i = 0; i < geometry.segments.size(); ++i) {
        const auto& source = geometry.segments[i];
        LinearNetworkSegment segment;
        segment.nodeA = source.nodeA;
        segment.nodeB = source.nodeB;
        segment.points = source.polyline;
        for (const auto& point : segment.points) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                return Fail(error, "linear segment " + std::to_string(i) + " has a non-finite point");
            if (std::abs(point.x) > MaxLinearNetworkWorldCoordinate ||
                std::abs(point.y) > MaxLinearNetworkWorldCoordinate ||
                std::abs(point.z) > MaxLinearNetworkWorldCoordinate)
                return Fail(error, "linear segment " + std::to_string(i) + " is outside the world bounds");
        }
        candidate.segments.push_back(std::move(segment));
    }
    candidate.segmentGeometryComplete = geometry.segmentGeometryComplete;
    candidate.unparsedSegmentCount = geometry.unparsedSegmentCount;
    candidate.segmentDiagnostic = geometry.segmentGeometryDiagnostic;
    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

bool ExtractPowerLinePolylineCandidates(
    const LinearNetwork& network,
    std::vector<PowerLinePolylineCandidate>& output,
    std::string* error) {
    if (!network.segmentGeometryComplete || network.unparsedSegmentCount != 0 ||
        network.segments.size() != network.segmentCount) {
        return Fail(error, "power-line segment geometry is incomplete");
    }

    std::vector<PowerLinePolylineCandidate> candidate;
    candidate.reserve(network.segments.size());
    for (std::size_t i = 0; i < network.segments.size(); ++i) {
        const auto& segment = network.segments[i];
        if (segment.nodeA >= network.nodes.size() || segment.nodeB >= network.nodes.size()) {
            return Fail(error, "power-line segment " + std::to_string(i) +
                                   " references a missing node");
        }

        PowerLinePolylineCandidate line;
        line.nodeA = segment.nodeA;
        line.nodeB = segment.nodeB;
        line.points = segment.points;
        for (const auto& point : line.points) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
                return Fail(error, "power-line segment " + std::to_string(i) +
                                       " has a non-finite point");
            }
            if (std::abs(point.x) > MaxLinearNetworkWorldCoordinate ||
                std::abs(point.y) > MaxLinearNetworkWorldCoordinate ||
                std::abs(point.z) > MaxLinearNetworkWorldCoordinate) {
                return Fail(error, "power-line segment " + std::to_string(i) +
                                       " is outside the world bounds");
            }
        }
        candidate.push_back(std::move(line));
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

bool ExtractNetworkBuildingAttachmentCandidates(
    const LinearNetwork& network, std::size_t placementCount,
    std::vector<NetworkBuildingAttachmentCandidate>& output,
    std::string* error) {
    if (placementCount > MaxLinearNetworkNodes)
        return Fail(error, "network placement count exceeds the limit");
    if (network.nodes.size() > MaxLinearNetworkNodes)
        return Fail(error, "linear network node count exceeds the limit");

    std::size_t referenceCount = 0;
    for (std::size_t i = 0; i < network.nodes.size(); ++i) {
        const auto& node = network.nodes[i];
        if (!std::isfinite(node.x) || !std::isfinite(node.y) || !std::isfinite(node.z))
            return Fail(error, "linear network node " + std::to_string(i) +
                                   " has a non-finite position");
        if (std::abs(node.x) > MaxLinearNetworkWorldCoordinate ||
            std::abs(node.y) > MaxLinearNetworkWorldCoordinate ||
            std::abs(node.z) > MaxLinearNetworkWorldCoordinate)
            return Fail(error, "linear network node " + std::to_string(i) +
                                   " is outside the world bounds");
        if (node.buildingReferences.size() > MaxLinearNetworkNodes - referenceCount)
            return Fail(error, "linear network building-reference count exceeds the limit");
        referenceCount += node.buildingReferences.size();
    }

    std::vector<NetworkBuildingAttachmentCandidate> candidate;
    candidate.reserve(referenceCount);
    for (std::size_t nodeIndex = 0; nodeIndex < network.nodes.size(); ++nodeIndex) {
        for (const auto& reference : network.nodes[nodeIndex].buildingReferences) {
            if (reference.buildingIndex < 0 ||
                static_cast<std::size_t>(reference.buildingIndex) >= placementCount) {
                return Fail(error, "linear network node " + std::to_string(nodeIndex) +
                                       " has an invalid source building index");
            }
            candidate.push_back({static_cast<std::uint32_t>(nodeIndex),
                                 static_cast<std::uint32_t>(reference.buildingIndex),
                                 reference.opaqueA, reference.opaqueB});
        }
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
