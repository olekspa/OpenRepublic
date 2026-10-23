#include "openrepublic/content/road_network.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kNodeBlockBytes = 0x48;
constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kSegmentHeaderBytes = 0x88;
constexpr std::uint32_t kMaxSegmentCollectionCount = 1u << 20;

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

bool SkipRecords(std::string_view bytes, std::size_t& offset, std::uint32_t count,
                 std::size_t stride) {
    if (offset > bytes.size() || count > kMaxSegmentCollectionCount) return false;
    if (stride != 0 && static_cast<std::size_t>(count) > (bytes.size() - offset) / stride)
        return false;
    const std::size_t amount = static_cast<std::size_t>(count) * stride;
    offset += amount;
    return true;
}

std::optional<std::uint32_t> SharedTypeForSegment(const RoadNetwork& network,
                                                  std::uint32_t nodeA,
                                                  std::uint32_t nodeB,
                                                  std::uint32_t segmentIndex) {
    const auto find = [&](std::uint32_t from, std::uint32_t to) -> std::optional<std::uint32_t> {
        const auto& node = network.nodes[from];
        for (std::size_t i = 0; i < node.neighbors.size(); ++i) {
            if (node.neighbors[i] == to && i < node.segmentIds.size() &&
                node.segmentIds[i] == segmentIndex && i < node.typeIds.size())
                return node.typeIds[i];
        }
        return std::nullopt;
    };
    const auto a = find(nodeA, nodeB);
    const auto b = find(nodeB, nodeA);
    return a && b && *a == *b ? a : std::nullopt;
}

bool DecodeSegments(std::string_view bytes, std::size_t& offset, RoadNetwork& network,
                    std::string* error) {
    network.segments.reserve(network.segmentCount);
    for (std::uint32_t i = 0; i < network.segmentCount; ++i) {
        if (offset > bytes.size() || kSegmentHeaderBytes > bytes.size() - offset)
            return Fail(error, "road segment " + std::to_string(i) + " header is truncated");

        const std::size_t header = offset;
        const std::uint32_t pointCount = ReadU32(bytes, header);
        const std::uint32_t nodeA = ReadU32(bytes, header + 4);
        const std::uint32_t nodeB = ReadU32(bytes, header + 8);
        if (nodeA >= network.nodes.size() || nodeB >= network.nodes.size())
            return Fail(error, "road segment " + std::to_string(i) + " references a missing node");
        if (pointCount > MaxRoadPolylinePoints)
            return Fail(error, "road segment " + std::to_string(i) + " point count exceeds the limit");

        // The writer emits the point records after the preceding segment
        // collections. The +0x34 subrecords carry their three-array length in
        // each serialized 0xcc prefix at +0xc4.
        std::size_t cursor = header + kSegmentHeaderBytes;
        const auto skip = [&](std::size_t& pos, std::size_t countOffset, std::size_t stride) {
            return SkipRecords(bytes, pos, ReadU32(bytes, header + countOffset), stride);
        };
        if (!skip(cursor, 0x0c, 4) || !skip(cursor, 0x24, 0x2c0) ||
            !skip(cursor, 0x28, 0x2c0) || !skip(cursor, 0x2c, 0x2c0) ||
            !skip(cursor, 0x30, 0x14))
            return Fail(error, "road segment " + std::to_string(i) + " has a truncated or excessive prefix");

        const std::uint32_t nestedCount = ReadU32(bytes, header + 0x34);
        if (nestedCount > kMaxSegmentCollectionCount)
            return Fail(error, "road segment " + std::to_string(i) + " has too many nested records");
        for (std::uint32_t nested = 0; nested < nestedCount; ++nested) {
            if (cursor > bytes.size() || 0xcc > bytes.size() - cursor)
                return Fail(error, "road segment " + std::to_string(i) + " nested record is truncated");
            const std::uint32_t arrayCount = ReadU32(bytes, cursor + 0xc4);
            if (arrayCount > kMaxSegmentCollectionCount)
                return Fail(error, "road segment " + std::to_string(i) + " nested array count exceeds the limit");
            cursor += 0xcc;
            if (!SkipRecords(bytes, cursor, arrayCount, 4) ||
                !SkipRecords(bytes, cursor, arrayCount, 4) ||
                !SkipRecords(bytes, cursor, arrayCount, 4))
                return Fail(error, "road segment " + std::to_string(i) + " nested arrays are truncated");
        }

        if (!SkipRecords(bytes, cursor, pointCount, 0x18))
            return Fail(error, "road segment " + std::to_string(i) + " polyline is truncated");
        const std::size_t pointsOffset = cursor - static_cast<std::size_t>(pointCount) * 0x18;

        RoadSegment segment;
        segment.pointCount = pointCount;
        segment.nodeA = nodeA;
        segment.nodeB = nodeB;
        segment.nestedRecordCount = nestedCount;
        segment.hasTrailingVectors = static_cast<unsigned char>(bytes[header + 0x84]) == 1;
        segment.typeId = SharedTypeForSegment(network, nodeA, nodeB, i);
        std::memcpy(segment.rawHeader.data(), bytes.data() + header, kSegmentHeaderBytes);
        segment.polyline.reserve(pointCount);
        for (std::uint32_t p = 0; p < pointCount; ++p) {
            const std::size_t pos = pointsOffset + static_cast<std::size_t>(p) * 0x18;
            RoadPoint point{ReadF32(bytes, pos), ReadF32(bytes, pos + 4), ReadF32(bytes, pos + 8)};
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                return Fail(error, "road segment " + std::to_string(i) + " has a non-finite point");
            segment.polyline.push_back(point);
        }
        network.segments.push_back(std::move(segment));

        // Continue past the remaining fixed-stride collections to the next
        // segment. The writer narrows the +0x85 reference-vector length to a
        // byte but serializes using the full runtime count, so nonzero values
        // cannot be skipped safely. The final two vectors are always written;
        // the reader consumes them only when +0x84 is one.
        const std::size_t familyCounts[] = {0x10, 0x1c, 0x20, 0x14, 0x18, 0x80};
        bool tailOk = true;
        for (const std::size_t countOffset : familyCounts)
            tailOk = tailOk && skip(cursor, countOffset, 0x30);
        tailOk = tailOk && skip(cursor, 0x70, 4) && skip(cursor, 0x74, 4) &&
                 SkipRecords(bytes, cursor, ReadU32(bytes, header + 0x7c), 1);
        if (!tailOk)
            return Fail(error, "road segment " + std::to_string(i) + " has a truncated or excessive tail");
        if (static_cast<unsigned char>(bytes[header + 0x85]) != 0 ||
            static_cast<unsigned char>(bytes[header + 0x84]) != 1) {
            network.segmentGeometryDiagnostic =
                "segment " + std::to_string(i) + " has an unsupported conditional tail (+0x84/+0x85)";
            network.unparsedSegmentCount = network.segmentCount - i - 1;
            return true;
        }
        if (!SkipRecords(bytes, cursor, 6, 4))
            return Fail(error, "road segment " + std::to_string(i) + " trailing vectors are truncated");
        offset = cursor;
    }
    network.segmentGeometryComplete = true;
    network.unparsedSegmentCount = 0;
    return true;
}

} // namespace

std::vector<RoadPoint> SampleRoadPolyline(const std::vector<RoadPoint>& polyline,
                                         std::size_t sampleCount) {
    if (polyline.empty() || sampleCount == 0) return {};
    if (sampleCount == 1 || polyline.size() == 1) return {polyline.front()};

    std::vector<double> cumulative(polyline.size(), 0.0);
    for (std::size_t i = 1; i < polyline.size(); ++i) {
        const double dx = static_cast<double>(polyline[i].x) - polyline[i - 1].x;
        const double dy = static_cast<double>(polyline[i].y) - polyline[i - 1].y;
        const double dz = static_cast<double>(polyline[i].z) - polyline[i - 1].z;
        cumulative[i] = cumulative[i - 1] + std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    const double total = cumulative.back();
    if (!(total > 0.0)) return std::vector<RoadPoint>(sampleCount, polyline.front());

    std::vector<RoadPoint> samples;
    samples.reserve(sampleCount);
    std::size_t segment = 1;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const double distance = total * static_cast<double>(i) / static_cast<double>(sampleCount - 1);
        while (segment + 1 < cumulative.size() && cumulative[segment] < distance) ++segment;
        const double span = cumulative[segment] - cumulative[segment - 1];
        const double t = span > 0.0 ? (distance - cumulative[segment - 1]) / span : 0.0;
        const auto lerp = [t](float a, float b) {
            return static_cast<float>(static_cast<double>(a) +
                                      (static_cast<double>(b) - a) * t);
        };
        samples.push_back({lerp(polyline[segment - 1].x, polyline[segment].x),
                           lerp(polyline[segment - 1].y, polyline[segment].y),
                           lerp(polyline[segment - 1].z, polyline[segment].z)});
    }
    return samples;
}

bool ExtractRoadNetwork(std::string_view bytes, RoadNetwork& output, std::string* error) {
    if (bytes.size() < kHeaderBytes) return Fail(error, "road file is shorter than its header");
    const std::uint32_t nodeCount = ReadU32(bytes, 0);
    if (nodeCount > MaxRoadNodes) return Fail(error, "road node count exceeds the limit");
    if (ReadU32(bytes, 4) > MaxRoadSegments)
        return Fail(error, "road segment count exceeds the limit");

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

    if (!DecodeSegments(bytes, offset, candidate, error)) return false;

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
