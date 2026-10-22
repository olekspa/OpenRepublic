#include "openrepublic/content/road_network.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void U32(std::string& out, std::uint32_t value) {
    out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

void F32(std::string& out, float value) {
    out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

void AppendSegment(std::string& out, std::uint32_t nodeA, std::uint32_t nodeB,
                   const std::vector<RoadPoint>& points) {
    const std::size_t start = out.size();
    out.resize(start + 0x88, '\0');
    const auto count = static_cast<std::uint32_t>(points.size());
    std::memcpy(out.data() + start, &count, sizeof(count));
    std::memcpy(out.data() + start + 4, &nodeA, sizeof(nodeA));
    std::memcpy(out.data() + start + 8, &nodeB, sizeof(nodeB));
    for (const auto& point : points) {
        F32(out, point.x);
        F32(out, point.y);
        F32(out, point.z);
        out.resize(out.size() + 12, '\0');
    }
    for (int i = 0; i < 6; ++i) U32(out, 0);
}

// One node: block (x, y, z, five counts, padding to 0x48) + arrays.
void AppendNode(std::string& out, float x, float y, float z, const std::vector<std::uint32_t>& neighbors,
                std::uint32_t triple) {
    const std::size_t start = out.size();
    F32(out, x);
    F32(out, y);
    F32(out, z);
    const auto n = static_cast<std::uint32_t>(neighbors.size());
    U32(out, n);
    U32(out, n);
    U32(out, n);
    U32(out, n);
    U32(out, triple);
    out.resize(start + 0x48, '\0');
    for (std::uint32_t i = 0; i < 3 * triple; ++i) U32(out, 7);
    for (const auto id : neighbors) U32(out, id);
    for (std::uint32_t i = 0; i < n; ++i) U32(out, 100 + i); // segment ids
    for (std::uint32_t i = 0; i < n; ++i) U32(out, 24);      // type ids
    for (std::uint32_t i = 0; i < n; ++i) U32(out, 0);       // extra words
}

std::string MakeNetwork() {
    std::string out;
    U32(out, 3);
    U32(out, 2);
    U32(out, 1);
    AppendNode(out, 10.0f, 1.0f, -20.0f, {1}, 1);
    AppendNode(out, 30.0f, 2.0f, -20.0f, {0, 2}, 0);
    AppendNode(out, 50.0f, 3.0f, -20.0f, {1}, 0);
    AppendSegment(out, 0, 1, {{10, 1, -20}, {20, 1.5f, -20}, {30, 2, -20}});
    AppendSegment(out, 1, 2, {{30, 2, -20}, {40, 2.5f, -20}, {50, 3, -20}});
    return out;
}

void TestExtraction() {
    RoadNetwork network;
    std::string error;
    Check(ExtractRoadNetwork(MakeNetwork(), network, &error), "synthetic network parses: " + error);
    Check(network.nodes.size() == 3 && network.segmentCount == 2 && network.groupCount == 1,
          "header counts are recorded");
    Check(network.nodes.size() == 3 && network.nodes[1].x == 30.0f && network.nodes[1].z == -20.0f &&
              network.nodes[1].neighbors == std::vector<std::uint32_t>{0, 2},
          "positions and neighbors are read");
    Check(network.nodes.size() == 3 && network.nodes[1].segmentIds[1] == 101 &&
              network.nodes[1].typeIds[0] == 24,
          "segment and type ids follow the neighbors");
    Check(network.asymmetricLinks == 0, "a symmetric network reports no asymmetric links");
    Check(network.segments.size() == 2 && network.segmentGeometryComplete &&
              network.segments[0].nodeA == 0 && network.segments[0].nodeB == 1 &&
              network.segments[0].polyline.size() == 3 && network.segments[0].polyline[1].x == 20.0f,
          "segment endpoints and point-list geometry are decoded");
    const auto sampled = SampleRoadPolyline(network.segments[0].polyline, 3);
    Check(sampled.size() == 3 && sampled.front().x == 10.0f && sampled.back().x == 30.0f &&
              std::abs(sampled[1].x - 20.0f) < 1e-5f,
          "polyline sampling uses equal arc-length spacing and includes endpoints");
}

void TestRejections() {
    RoadNetwork keep;
    keep.segmentCount = 99;
    const auto rejects = [&](const std::string& bytes, const std::string& label) {
        std::string error;
        Check(!ExtractRoadNetwork(bytes, keep, &error) && !error.empty() && keep.segmentCount == 99, label);
    };
    rejects("short", "a short file is rejected");
    std::string truncated = MakeNetwork();
    truncated.resize(truncated.size() - 6);
    rejects(truncated, "a truncated node array is rejected");
    std::string out;
    U32(out, 1);
    U32(out, 0);
    U32(out, 0);
    AppendNode(out, 0, 0, 0, {5}, 0);
    rejects(out, "a link to a missing node is rejected");
    std::string huge;
    U32(huge, static_cast<std::uint32_t>(MaxRoadNodes + 1));
    U32(huge, 0);
    U32(huge, 0);
    rejects(huge, "an oversized node count is rejected");

    // One-directional link: accepted but counted.
    std::string oneWay;
    U32(oneWay, 2);
    U32(oneWay, 1);
    U32(oneWay, 0);
    AppendNode(oneWay, 0, 0, 0, {1}, 0);
    AppendNode(oneWay, 5, 0, 0, {}, 0);
    AppendSegment(oneWay, 0, 1, {{0, 0, 0}, {5, 0, 0}});
    RoadNetwork network;
    std::string error;
    Check(ExtractRoadNetwork(oneWay, network, &error) && network.asymmetricLinks == 1,
          "a missing reverse link is counted, not fatal");
}

void TestPartialUnsupportedNestedSegment() {
    std::string bytes;
    U32(bytes, 2);
    U32(bytes, 1);
    U32(bytes, 0);
    AppendNode(bytes, 0, 0, 0, {1}, 0);
    AppendNode(bytes, 5, 0, 0, {0}, 0);
    const std::size_t header = bytes.size();
    bytes.resize(header + 0x88, '\0');
    const std::uint32_t nodeA = 0, nodeB = 1, nestedCount = 1;
    std::memcpy(bytes.data() + header + 4, &nodeA, 4);
    std::memcpy(bytes.data() + header + 8, &nodeB, 4);
    std::memcpy(bytes.data() + header + 0x34, &nestedCount, 4);
    RoadNetwork network;
    std::string error;
    Check(ExtractRoadNetwork(bytes, network, &error) && network.nodes.size() == 2 &&
              network.segments.empty() && !network.segmentGeometryComplete &&
              network.unparsedSegmentCount == 1 && !network.segmentGeometryDiagnostic.empty(),
          "an unknown nested-vector payload stops segment decoding with an explicit partial result");
}

} // namespace

int main() {
    TestExtraction();
    TestRejections();
    TestPartialUnsupportedNestedSegment();
    if (failures != 0) {
        std::cerr << failures << " road-network test(s) failed\n";
        return 1;
    }
    std::cout << "All road-network tests passed\n";
    return 0;
}
