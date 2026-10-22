#include "openrepublic/content/linear_network.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

using namespace openrepublic::content;
namespace {
int failures = 0;
void Check(bool condition, const char* label) {
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
void U32(std::string& out, std::uint32_t value) {
    out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}
void F32(std::string& out, float value) {
    out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}
void I32(std::string& out, std::int32_t value) {
    out.append(reinterpret_cast<const char*>(&value), sizeof(value));
}
void AppendNode(std::string& bytes, float x, std::uint32_t neighbor) {
    const std::size_t start = bytes.size();
    F32(bytes, x); F32(bytes, 2.0f); F32(bytes, -3.0f);
    U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 0);
    bytes.resize(start + 0x48, '\0');
    U32(bytes, neighbor); U32(bytes, 0); U32(bytes, 5); U32(bytes, 0);
}
std::string EmptyFile() { return std::string(12, '\0'); }
std::string NodeFile() {
    std::string bytes;
    U32(bytes, 1); U32(bytes, 0); U32(bytes, 3);
    F32(bytes, 100.0f); F32(bytes, 4.5f); F32(bytes, -25.0f);
    U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 0);
    bytes.resize(12 + 0x48, '\0');
    U32(bytes, 0); U32(bytes, 0); U32(bytes, 5); U32(bytes, 11);
    return bytes;
}
std::string BuildingReferenceFile() {
    std::string bytes;
    U32(bytes, 1); U32(bytes, 0); U32(bytes, 0);
    F32(bytes, 10.0f); F32(bytes, 20.0f); F32(bytes, 30.0f);
    U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 4);
    bytes.resize(12 + 0x48, '\0');
    // Three parallel arrays, deliberately distinct from an interleaved layout.
    for (const auto value : {2, -1, 0, 3}) I32(bytes, value);
    for (const auto value : {101, 102, 103, 104}) I32(bytes, value);
    for (const auto value : {-201, -202, -203, -204}) I32(bytes, value);
    U32(bytes, 0); U32(bytes, 0); U32(bytes, 0); U32(bytes, 0);
    return bytes;
}
std::string PolylineFile() {
    std::string bytes;
    U32(bytes, 2); U32(bytes, 1); U32(bytes, 0);
    AppendNode(bytes, 0.0f, 1);
    AppendNode(bytes, 10.0f, 0);
    const auto header = bytes.size();
    bytes.resize(header + 0x88, '\0');
    const std::uint32_t pointCount = 2, nodeA = 0, nodeB = 1;
    std::memcpy(bytes.data() + header, &pointCount, 4);
    std::memcpy(bytes.data() + header + 4, &nodeA, 4);
    std::memcpy(bytes.data() + header + 8, &nodeB, 4);
    const std::uint8_t supportedConditionalTail = 1;
    std::memcpy(bytes.data() + header + 0x84, &supportedConditionalTail, 1);
    for (float x : {0.5f, 9.5f}) {
        F32(bytes, x); F32(bytes, 2.0f); F32(bytes, -3.0f);
        bytes.resize(bytes.size() + 12, '\0');
    }
    for (int i = 0; i < 6; ++i) U32(bytes, 0);
    return bytes;
}
bool ValidatePackageFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { std::cerr << "FAIL: cannot read " << path.filename().string() << '\n'; return false; }
    std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    LinearNetwork network;
    std::string error;
    if (!ExtractLinearNetwork(bytes, network, &error)) {
        std::cerr << "FAIL: " << path.filename().string() << ": " << error << '\n';
        return false;
    }
    if (network.nodes.size() == 0 && network.segmentCount != 0) {
        std::cerr << "FAIL: missing nodes for declared segments in " << path.filename().string() << '\n';
        return false;
    }
    if (network.segmentGeometryComplete && network.segments.size() != network.segmentCount) {
        std::cerr << "FAIL: segment count mismatch in " << path.filename().string() << '\n';
        return false;
    }
    for (const auto& segment : network.segments) {
        if (segment.nodeA >= network.nodes.size() || segment.nodeB >= network.nodes.size()) {
            std::cerr << "FAIL: endpoint node out of range in " << path.filename().string() << '\n';
            return false;
        }
        for (const auto& point : segment.points) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) ||
                std::abs(point.x) > MaxLinearNetworkWorldCoordinate ||
                std::abs(point.y) > MaxLinearNetworkWorldCoordinate ||
                std::abs(point.z) > MaxLinearNetworkWorldCoordinate) {
                std::cerr << "FAIL: invalid point in " << path.filename().string() << '\n';
                return false;
            }
        }
    }
    std::cout << path.parent_path().filename().string() << '/' << path.filename().string()
              << " nodes=" << network.nodes.size() << " segments=" << network.segmentCount
              << " decoded=" << network.segments.size() << " points=";
    std::size_t pointCount = 0;
    for (const auto& segment : network.segments) pointCount += segment.points.size();
    std::cout << pointCount << '\n';
    return true;
}
}
int main(int argc, char** argv) {
    LinearNetwork n;
    std::string error;
    Check(ExtractLinearNetwork(EmptyFile(), n, &error), "12-byte empty network parses");
    Check(n.nodes.empty() && n.segmentCount == 0 && n.groupCount == 0,
          "empty header counts are preserved");

    Check(ExtractLinearNetwork(NodeFile(), n, &error), "node and link arrays parse");
    Check(n.nodes.size() == 1 && n.nodes[0].x == 100.0f && n.nodes[0].y == 4.5f &&
              n.nodes[0].z == -25.0f, "node XYZ is retained");
    Check(n.nodes[0].links[0] == std::vector<std::uint32_t>{0} &&
              n.nodes[0].links[3] == std::vector<std::uint32_t>{11},
          "four arrays are retained without semantic assumptions");

    const bool parsedBuildingReferences = ExtractLinearNetwork(BuildingReferenceFile(), n, &error);
    Check(parsedBuildingReferences, "three parallel signed building-reference arrays parse");
    if (!parsedBuildingReferences || n.nodes.empty() || n.nodes[0].buildingReferences.size() != 4)
        return 1;
    Check(n.nodes.size() == 1 && n.nodes[0].buildingReferences.size() == 4 &&
              n.nodes[0].buildingReferences[0].buildingIndex == 2 &&
              n.nodes[0].buildingReferences[0].opaqueA == 101 &&
              n.nodes[0].buildingReferences[0].opaqueB == -201 &&
              n.nodes[0].buildingReferences[1].buildingIndex == -1 &&
              n.nodes[0].buildingReferences[1].opaqueA == 102 &&
              n.nodes[0].buildingReferences[1].opaqueB == -202 &&
              n.nodes[0].buildingReferences[3].buildingIndex == 3 &&
              n.nodes[0].buildingReferences[3].opaqueA == 104 &&
              n.nodes[0].buildingReferences[3].opaqueB == -204,
          "parallel arrays retain order and signed opaque values, including invalid indices");
    Check(n.nodes[0].links[0] == std::vector<std::uint32_t>{0} &&
              n.nodes[0].links[1] == std::vector<std::uint32_t>{0} &&
              n.nodes[0].links[2] == std::vector<std::uint32_t>{0} &&
              n.nodes[0].links[3] == std::vector<std::uint32_t>{0},
          "existing link arrays remain aligned after building-reference arrays");

    LinearNetwork attachmentNetwork;
    const bool parsedAttachmentFixture =
        ExtractLinearNetwork(BuildingReferenceFile(), attachmentNetwork, &error);
    Check(parsedAttachmentFixture, "attachment helper fixture parses");
    if (!parsedAttachmentFixture || attachmentNetwork.nodes.empty() ||
        attachmentNetwork.nodes[0].buildingReferences.size() != 4)
        return 1;
    // Invalid source indices are intentionally retained by the decoder, but
    // the adapter rejects them because it is bound to an exact placement table.
    std::vector<NetworkBuildingAttachmentCandidate> attachments{{77, 88, 99, 111}};
    Check(!ExtractNetworkBuildingAttachmentCandidates(attachmentNetwork, 4, attachments, &error) &&
              !error.empty() && attachments.size() == 1 && attachments[0].nodeIndex == 77,
          "invalid signed reference fails adapter transactionally");
    attachmentNetwork.nodes[0].buildingReferences.erase(
        attachmentNetwork.nodes[0].buildingReferences.begin() + 1);
    Check(ExtractNetworkBuildingAttachmentCandidates(attachmentNetwork, 4, attachments, &error) &&
              attachments.size() == 3 && attachments[0].nodeIndex == 0 &&
              attachments[0].buildingIndex == 2 && attachments[0].opaqueA == 101 &&
              attachments[0].opaqueB == -201 && attachments[1].buildingIndex == 0 &&
              attachments[1].opaqueA == 103 && attachments[1].opaqueB == -203 &&
              attachments[2].buildingIndex == 3,
          "valid candidates preserve source order and opaque values");
    const auto validAttachments = attachments;
    Check(!ExtractNetworkBuildingAttachmentCandidates(attachmentNetwork, 3, attachments, &error) &&
              !error.empty() && attachments.size() == validAttachments.size() &&
              attachments[0].buildingIndex == validAttachments[0].buildingIndex,
          "out-of-placement-range index fails transactionally");
    Check(!ExtractNetworkBuildingAttachmentCandidates(attachmentNetwork,
                                                       MaxLinearNetworkNodes + 1,
                                                       attachments, &error) &&
              !error.empty() && attachments.size() == validAttachments.size(),
          "excessive placement source count is rejected transactionally");
    LinearNetwork excessiveReferences;
    excessiveReferences.nodes.push_back({});
    excessiveReferences.nodes[0].buildingReferences.resize(MaxLinearNetworkNodes + 1);
    Check(!ExtractNetworkBuildingAttachmentCandidates(excessiveReferences, 0, attachments, &error) &&
              !error.empty() && attachments.size() == validAttachments.size(),
          "excessive total candidate count is rejected transactionally");
    attachmentNetwork.nodes[0].x = std::numeric_limits<float>::infinity();
    Check(!ExtractNetworkBuildingAttachmentCandidates(attachmentNetwork, 4, attachments, &error) &&
              !error.empty() && attachments.size() == validAttachments.size(),
          "non-finite manually constructed node position is rejected transactionally");
    attachmentNetwork.nodes[0].x = MaxLinearNetworkWorldCoordinate + 1.0f;
    Check(!ExtractNetworkBuildingAttachmentCandidates(attachmentNetwork, 4, attachments, &error) &&
              !error.empty() && attachments.size() == validAttachments.size(),
          "out-of-bounds manually constructed node position is rejected transactionally");
    attachmentNetwork.nodes[0].x = 10.0f;
    attachmentNetwork.nodes[0].buildingReferences.clear();
    Check(ExtractNetworkBuildingAttachmentCandidates(attachmentNetwork, 0, attachments, &error) &&
              attachments.empty(),
          "empty network attachment set clears output successfully");

    auto truncatedReferences = BuildingReferenceFile();
    truncatedReferences.resize(truncatedReferences.size() - 4);
    LinearNetwork preservedAttachments;
    preservedAttachments.nodes.push_back({});
    Check(!ExtractLinearNetwork(truncatedReferences, preservedAttachments, &error) &&
              !error.empty() && preservedAttachments.nodes.size() == 1,
          "truncated third parallel array fails transactionally");

    Check(ExtractLinearNetwork(PolylineFile(), n, &error), "segment point records parse");
    Check(n.segmentGeometryComplete && n.segments.size() == 1 &&
              n.segments[0].nodeA == 0 && n.segments[0].nodeB == 1 &&
              n.segments[0].points.size() == 2 && n.segments[0].points[1].x == 9.5f,
          "segment references and ordered XYZ points are retained");

    LinearNetwork emptyNetwork;
    std::vector<PowerLinePolylineCandidate> powerLines;
    Check(ExtractLinearNetwork(EmptyFile(), emptyNetwork, &error) &&
              ExtractPowerLinePolylineCandidates(emptyNetwork, powerLines, &error) &&
              powerLines.empty(),
          "an empty complete network produces no power-line candidates");

    Check(ExtractPowerLinePolylineCandidates(n, powerLines, &error) &&
              powerLines.size() == 1 && powerLines[0].nodeA == 0 && powerLines[0].nodeB == 1 &&
              powerLines[0].points.size() == 2 && powerLines[0].points[0].x == 0.5f &&
              powerLines[0].points[1].x == 9.5f && powerLines[0].points[1].y == 2.0f &&
              powerLines[0].points[1].z == -3.0f,
          "complete power-line candidate paths preserve endpoint ids and ordered XYZ");

    auto unknownTail = PolylineFile();
    const std::size_t segmentHeader = 12 + 2 * (0x48 + 4 * 4);
    unknownTail[segmentHeader + 0x85] = 1;
    Check(ExtractLinearNetwork(unknownTail, n, &error) && !n.segmentGeometryComplete &&
              n.segments.size() == 1 && !n.segmentDiagnostic.empty(),
          "unknown conditional tail returns a safe geometry prefix with a diagnostic");
    const auto previousPowerLines = powerLines;
    Check(!ExtractPowerLinePolylineCandidates(n, powerLines, &error) &&
              !error.empty() && powerLines.size() == previousPowerLines.size() &&
              powerLines[0].points[1].x == previousPowerLines[0].points[1].x,
          "incomplete segment prefixes are not exposed as power-line candidates");

    LinearNetwork invalidPowerNetwork;
    Check(ExtractLinearNetwork(PolylineFile(), invalidPowerNetwork, &error),
          "fresh network parses for power-line candidate validation");
    invalidPowerNetwork.segments[0].nodeB = 2;
    Check(!ExtractPowerLinePolylineCandidates(invalidPowerNetwork, powerLines, &error) &&
              !error.empty() && powerLines.size() == previousPowerLines.size(),
          "invalid endpoint ids fail transactionally");
    invalidPowerNetwork.segments[0].nodeB = 1;
    invalidPowerNetwork.segments[0].points[0].y = std::numeric_limits<float>::quiet_NaN();
    Check(!ExtractPowerLinePolylineCandidates(invalidPowerNetwork, powerLines, &error) &&
              !error.empty() && powerLines.size() == previousPowerLines.size(),
          "non-finite candidate points fail transactionally");
    invalidPowerNetwork.segments[0].points[0].y = 2.0f;
    invalidPowerNetwork.segments[0].points[0].x = MaxLinearNetworkWorldCoordinate + 1.0f;
    Check(!ExtractPowerLinePolylineCandidates(invalidPowerNetwork, powerLines, &error) &&
              !error.empty() && powerLines.size() == previousPowerLines.size(),
          "out-of-world candidate points fail transactionally");

    const auto previous = n.nodes.size();
    auto truncated = PolylineFile(); truncated.pop_back();
    Check(!ExtractLinearNetwork(truncated, n, &error) && n.nodes.size() == previous,
          "truncated data fails transactionally");
    auto outOfBounds = NodeFile();
    const float far = 20001.0f;
    std::memcpy(outOfBounds.data() + 12, &far, sizeof(far));
    Check(!ExtractLinearNetwork(outOfBounds, n, &error) && n.nodes.size() == previous,
          "out-of-world node fails transactionally");
    auto outOfBoundsPoint = PolylineFile();
    const std::size_t pointOffset = segmentHeader + 0x88;
    std::memcpy(outOfBoundsPoint.data() + pointOffset, &far, sizeof(far));
    Check(!ExtractLinearNetwork(outOfBoundsPoint, n, &error) && n.nodes.size() == previous,
          "out-of-world segment point fails transactionally");
    if (failures != 0) return 1;
    if (argc > 1) {
        const std::filesystem::path root(argv[1]);
        std::size_t checked = 0;
        if (std::filesystem::is_regular_file(root)) {
            if (!ValidatePackageFile(root)) return 1;
            checked = 1;
        } else if (std::filesystem::is_directory(root)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                if (!entry.is_regular_file()) continue;
                const auto name = entry.path().filename().string();
                if (name == "rail.bin" || name == "rail_secondary.bin" ||
                    name == "electro_high.bin" || name == "electro_low.bin" ||
                    name == "fence.bin" || name == "pipe.bin") {
                    if (!ValidatePackageFile(entry.path())) return 1;
                    ++checked;
                }
            }
        }
        std::cout << "checked_files=" << checked << '\n';
    }
    return 0;
}
