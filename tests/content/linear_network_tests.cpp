#include "openrepublic/content/linear_network.h"

#include <cstring>
#include <iostream>
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
std::string EmptyFile() { return std::string(12, '\0'); }
std::string OneNodeFile() {
    std::string bytes;
    U32(bytes, 1); U32(bytes, 2); U32(bytes, 3);
    F32(bytes, 100.0f); F32(bytes, 4.5f); F32(bytes, -25.0f);
    U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 1); U32(bytes, 0);
    bytes.resize(12 + 0x48, '\0');
    U32(bytes, 8); U32(bytes, 9); U32(bytes, 10); U32(bytes, 11);
    return bytes;
}
}
int main() {
    LinearNetwork n;
    std::string error;
    Check(ExtractLinearNetwork(EmptyFile(), n, &error), "12-byte empty network parses");
    Check(n.nodes.empty() && n.segmentCount == 0 && n.groupCount == 0,
          "empty header counts are preserved");

    Check(ExtractLinearNetwork(OneNodeFile(), n, &error), "node and link arrays parse");
    Check(n.nodes.size() == 1 && n.nodes[0].x == 100.0f && n.nodes[0].y == 4.5f &&
              n.nodes[0].z == -25.0f, "node XYZ is retained");
    Check(n.nodes[0].links[0] == std::vector<std::uint32_t>{8} &&
              n.nodes[0].links[3] == std::vector<std::uint32_t>{11},
          "four arrays are retained without semantic assumptions");
    Check(n.segmentCount == 2 && n.unparsedSegmentCount == 2 && !n.segmentDiagnostic.empty(),
          "segment records fail closed with an explicit diagnostic");

    const auto previous = n.nodes.size();
    auto truncated = OneNodeFile(); truncated.pop_back();
    Check(!ExtractLinearNetwork(truncated, n, &error) && n.nodes.size() == previous,
          "truncated data fails transactionally");
    auto outOfBounds = OneNodeFile();
    const float far = 20001.0f;
    std::memcpy(outOfBounds.data() + 12, &far, sizeof(far));
    Check(!ExtractLinearNetwork(outOfBounds, n, &error) && n.nodes.size() == previous,
          "out-of-world node fails transactionally");
    return failures == 0 ? 0 : 1;
}
