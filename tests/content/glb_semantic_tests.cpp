#include "openrepublic/content/glb_semantic.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kGlbMagic = 0x46546C67;
constexpr std::uint32_t kJsonChunk = 0x4E4F534A;
constexpr std::uint32_t kBinChunk = 0x004E4942;

void AppendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

void PatchU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes[offset + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}

void AppendChunk(std::vector<std::uint8_t>& bytes, std::uint32_t type, const std::vector<std::uint8_t>& payload) {
    AppendU32(bytes, static_cast<std::uint32_t>(payload.size()));
    AppendU32(bytes, type);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}

std::vector<std::uint8_t> MakeGlb(std::string json, std::vector<std::uint8_t> bin) {
    while (json.size() % 4 != 0) json.push_back(' ');
    while (bin.size() % 4 != 0) bin.push_back(0);
    std::vector<std::uint8_t> bytes;
    AppendU32(bytes, kGlbMagic);
    AppendU32(bytes, 2);
    AppendU32(bytes, 0);
    AppendChunk(bytes, kJsonChunk, std::vector<std::uint8_t>(json.begin(), json.end()));
    AppendChunk(bytes, kBinChunk, bin);
    PatchU32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    return bytes;
}

std::vector<std::uint8_t> MakeTriangleBin() {
    std::vector<std::uint8_t> bytes(48, 0);
    const float positions[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    std::memcpy(bytes.data(), positions, sizeof(positions));
    const std::uint16_t indices[3] = {0, 1, 2};
    std::memcpy(bytes.data() + 36, indices, sizeof(indices));
    bytes[44] = 0x89; // The fixture verifies only the embedded-image reference path, not image decoding.
    bytes[45] = 'P';
    bytes[46] = 'N';
    bytes[47] = 'G';
    return bytes;
}

std::vector<std::uint8_t> MakeSharedIndexAccessorBin() {
    std::vector<std::uint8_t> bytes(90, 0);
    const float positions[21] = {};
    std::memcpy(bytes.data(), positions, sizeof(positions));
    const std::uint16_t indices[3] = {0, 1, 3};
    std::memcpy(bytes.data() + 84, indices, sizeof(indices));
    return bytes;
}

const std::string kValidJson = R"json({"asset":{"version":"2.0","generator":"OpenRepublic synthetic fixture"},"buffers":[{"byteLength":48}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":6,"target":34963},{"buffer":0,"byteOffset":44,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],"images":[{"bufferView":2,"mimeType":"image/png"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],"nodes":[{"mesh":0,"translation":[0,0,0]}],"scenes":[{"nodes":[0]}],"scene":0})json";
const std::string kSharedIndexAccessorJson = R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":90}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48,"target":34962},{"buffer":0,"byteOffset":48,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":84,"byteLength":6,"target":34963}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":2}]},{"primitives":[{"attributes":{"POSITION":1},"indices":2}]}]})json";

std::string Replace(std::string json, std::string_view from, std::string_view to) {
    const auto position = json.find(from);
    if (position == std::string::npos) throw std::logic_error("synthetic JSON mutation target was not found");
    json.replace(position, from.size(), to);
    return json;
}

std::string MakeAggregateAccessorWorkJson() {
    constexpr std::size_t accessorCount = openrepublic::content::kMaxGlbSemanticArrayEntries;
    std::string json;
    json.reserve(accessorCount * 72);
    json += R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":1032}],"bufferViews":[{"buffer":0,"byteLength":1032}],"accessors":[)json";
    for (std::size_t i = 0; i < accessorCount; ++i) {
        if (i != 0) json.push_back(',');
        json += R"json({"bufferView":0,"count":129,"componentType":5126,"type":"VEC2"})json";
    }
    json += R"json(],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}]})json";
    return json;
}

bool Check(const std::string& name, std::string json, std::vector<std::uint8_t> bin,
           bool expected, openrepublic::content::GlbSemanticErrorCode expectedCode =
               openrepublic::content::GlbSemanticErrorCode::None) {
    using namespace openrepublic::content;
    const auto bytes = MakeGlb(std::move(json), std::move(bin));
    GlbContainerInfo container;
    std::string framingError;
    if (!PreflightGlbContainer(bytes, container, &framingError)) {
        std::cerr << "FAIL: fixture framing for " << name << ": " << framingError << '\n';
        return false;
    }
    GlbSemanticSummary summary{71, 72, 73, 74, 75, 76, 77};
    const GlbSemanticSummary before = summary;
    GlbSemanticError error;
    const bool actual = ValidateEmbeddedGlbV2(bytes, container, summary, error);
    if (actual != expected) {
        std::cerr << "FAIL: " << name << " expected=" << expected << " got=" << actual
                  << " code=" << static_cast<int>(error.code) << " message='" << error.message << "'\n";
        return false;
    }
    if (expected) {
        if (error.code != GlbSemanticErrorCode::None || !error.message.empty() ||
            summary.meshCount != 1 || summary.primitiveCount != 1 || summary.accessorCount != 2 ||
            summary.bufferViewCount != 3 || summary.materialCount != 1 ||
            summary.textureCount != 1 || summary.imageCount != 1) {
            std::cerr << "FAIL: " << name << " produced unexpected success summary or diagnostics.\n";
            return false;
        }
    } else {
        if (summary.meshCount != before.meshCount || summary.primitiveCount != before.primitiveCount ||
            summary.accessorCount != before.accessorCount || summary.bufferViewCount != before.bufferViewCount ||
            summary.materialCount != before.materialCount || summary.textureCount != before.textureCount ||
            summary.imageCount != before.imageCount || error.code == GlbSemanticErrorCode::None || error.message.empty()) {
            std::cerr << "FAIL: " << name << " did not preserve output transactionally or report a structured error.\n";
            return false;
        }
        if (expectedCode != GlbSemanticErrorCode::None && error.code != expectedCode) {
            std::cerr << "FAIL: " << name << " expected error code " << static_cast<int>(expectedCode)
                      << " got " << static_cast<int>(error.code) << ".\n";
            return false;
        }
    }
    return true;
}

bool CheckContainerInfoMismatch() {
    using namespace openrepublic::content;
    const auto bytes = MakeGlb(kValidJson, MakeTriangleBin());
    GlbContainerInfo container;
    if (!PreflightGlbContainer(bytes, container)) return false;
    ++container.binaryChunkBytes;
    GlbSemanticSummary summary{1, 2, 3, 4, 5, 6, 7};
    GlbSemanticError error;
    const bool accepted = ValidateEmbeddedGlbV2(bytes, container, summary, error);
    return !accepted && error.code == GlbSemanticErrorCode::InvalidContainerInfo && summary.meshCount == 1;
}

} // namespace

int main() {
    using openrepublic::content::GlbSemanticErrorCode;
    bool ok = true;
    ok &= Check("valid embedded triangle, material, and image", kValidJson, MakeTriangleBin(), true);
    ok &= Check("explicit supported minVersion", Replace(kValidJson,
                R"json("version":"2.0")json", R"json("version":"2.0","minVersion":"2.0")json"), MakeTriangleBin(), true);
    ok &= Check("valid nested acyclic node graph and referenced sampler",
                Replace(Replace(kValidJson,
                    R"json("nodes":[{"mesh":0,"translation":[0,0,0]}])json",
                    R"json("nodes":[{"mesh":0,"children":[1]},{"translation":[1,0,0]}])json"),
                    R"json("textures":[{"source":0}])json",
                    R"json("samplers":[{}],"textures":[{"source":0,"sampler":0}])json"),
                MakeTriangleBin(), true);
    ok &= Check("wrong asset version", std::string(kValidJson).replace(kValidJson.find("2.0"), 3, "1.0"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("newer minVersion", Replace(kValidJson, R"json("version":"2.0")json", R"json("version":"2.0","minVersion":"2.1")json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("malformed minVersion", Replace(kValidJson, R"json("version":"2.0")json", R"json("version":"2.0","minVersion":"2.x")json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidDocument);
    ok &= Check("invalid JSON", "{broken", MakeTriangleBin(), false, GlbSemanticErrorCode::InvalidJson);
    ok &= Check("non-standard JSON comment", Replace(kValidJson, R"json({"asset")json", R"json({/*x*/"asset")json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidJson);
    ok &= Check("missing POSITION", Replace(kValidJson, "POSITION", "TANGENT"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidGeometry);
    ok &= Check("out-of-range POSITION accessor", Replace(kValidJson, R"json("POSITION":0)json", R"json("POSITION":8)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidReference);
    ok &= Check("invalid material reference", Replace(kValidJson, R"json("material":0)json", R"json("material":4)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidReference);
    ok &= Check("non-triangle primitive mode", Replace(kValidJson, R"json("indices":1)json", R"json("indices":1,"mode":1)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("external buffer URI", Replace(kValidJson, R"json("byteLength":48)json", R"json("byteLength":48,"uri":"data:bad")json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("sparse accessor", Replace(kValidJson, R"json("type":"VEC3")json", R"json("sparse":{},"type":"VEC3")json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("unsupported extension", Replace(kValidJson, R"json("scene":0)json", R"json("scene":0,"extensionsUsed":["KHR_example"])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("external image URI", Replace(kValidJson, R"json("bufferView":2)json", R"json("uri":"texture.png")json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("out-of-range bufferView", Replace(kValidJson, R"json("bufferView":0)json", R"json("bufferView":9)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidReference);
    ok &= Check("accessor exceeds bufferView", Replace(kValidJson, R"json("byteLength":36)json", R"json("byteLength":35)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidBinaryLayout);
    ok &= Check("bufferView exceeds embedded buffer", Replace(kValidJson, R"json("byteOffset":44,"byteLength":4)json", R"json("byteOffset":46,"byteLength":4)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidBinaryLayout);
    ok &= Check("out-of-range texture image", Replace(kValidJson, R"json("source":0)json", R"json("source":7)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidReference);
    ok &= Check("out-of-range triangle index", kValidJson, [] { auto bytes = MakeTriangleBin(); bytes[40] = 9; return bytes; }(), false,
                GlbSemanticErrorCode::InvalidGeometry);
    ok &= Check("shared index accessor checked against every POSITION count", kSharedIndexAccessorJson,
                MakeSharedIndexAccessorBin(), false, GlbSemanticErrorCode::InvalidGeometry);
    ok &= Check("unsupported vertex semantic", Replace(kValidJson, R"json("POSITION":0)json", R"json("POSITION":0,"JOINTS_0":1)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("unsupported TEXCOORD_0 layout", Replace(kValidJson, R"json("POSITION":0)json", R"json("POSITION":0,"TEXCOORD_0":1)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidGeometry);
    ok &= Check("node graph cycle", Replace(kValidJson, R"json("nodes":[{"mesh":0,"translation":[0,0,0]}])json", R"json("nodes":[{"mesh":0,"children":[0]}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidDocument);
    ok &= Check("node with multiple parents", Replace(kValidJson,
                R"json("nodes":[{"mesh":0,"translation":[0,0,0]}])json",
                R"json("nodes":[{"mesh":0,"children":[3]},{"children":[3]},{"children":[3]},{"translation":[0,0,0]}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidDocument);
    ok &= Check("scene root also child", Replace(
                Replace(kValidJson,
                    R"json("nodes":[{"mesh":0,"translation":[0,0,0]}])json",
                    R"json("nodes":[{"children":[1]},{"translation":[0,0,0]}])json"),
                R"json("scenes":[{"nodes":[0]}])json",
                R"json("scenes":[{"nodes":[1]}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidDocument);
    ok &= Check("morph target", Replace(kValidJson, R"json("indices":1)json", R"json("indices":1,"targets":[{}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("mesh morph weights", Replace(kValidJson, R"json("meshes":[{)json", R"json("meshes":[{"weights":[0],)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("animations unsupported", Replace(kValidJson, R"json("scene":0)json", R"json("scene":0,"animations":[{}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("skins unsupported", Replace(kValidJson, R"json("scene":0)json", R"json("scene":0,"skins":[{}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("cameras unsupported", Replace(kValidJson, R"json("scene":0)json", R"json("scene":0,"cameras":[{}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("node skin unsupported", Replace(kValidJson, R"json("translation":[0,0,0])json", R"json("translation":[0,0,0],"skin":0)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("non-finite float node transform", Replace(kValidJson, R"json("translation":[0,0,0])json", R"json("translation":[1e300,0,0])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidGeometry);
    ok &= Check("zero-length node rotation quaternion", Replace(kValidJson,
                R"json("translation":[0,0,0])json", R"json("rotation":[0,0,0,0])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidGeometry);
    ok &= Check("non-unit node rotation quaternion", Replace(kValidJson,
                R"json("translation":[0,0,0])json", R"json("rotation":[0,0,0,2])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidGeometry);
    ok &= Check("unit node rotation quaternion", Replace(kValidJson,
                R"json("translation":[0,0,0])json", R"json("rotation":[0,0,0.70710677,0.70710677])json"), MakeTriangleBin(), true);
    ok &= Check("matrix and TRS conflict", Replace(kValidJson, R"json("translation":[0,0,0])json", R"json("translation":[0,0,0],"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidDocument);
    ok &= Check("invalid sampler enum", Replace(Replace(kValidJson,
                R"json("textures":[{"source":0}])json", R"json("samplers":[{"wrapS":1}],"textures":[{"source":0,"sampler":0}])json"),
                R"json("source":0)json", R"json("source":0)json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("unsupported texture coordinate set", Replace(kValidJson,
                R"json("baseColorTexture":{"index":0})json",
                R"json("baseColorTexture":{"index":0,"texCoord":1})json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::UnsupportedFeature);
    ok &= Check("out-of-range sampler", Replace(kValidJson, R"json("textures":[{"source":0}])json", R"json("textures":[{"source":0,"sampler":0}])json"), MakeTriangleBin(), false,
                GlbSemanticErrorCode::InvalidReference);
    ok &= Check("GLB JSON size cap", std::string(openrepublic::content::kMaxGlbSemanticJsonBytes + 4, ' '), MakeTriangleBin(), false,
                GlbSemanticErrorCode::JsonTooLarge);
    ok &= Check("aggregate numeric accessor scan cap", MakeAggregateAccessorWorkJson(), std::vector<std::uint8_t>(1032, 0), false,
                GlbSemanticErrorCode::LimitsExceeded);
    ok &= Check("non-finite position", kValidJson, [] { auto bytes = MakeTriangleBin(); const float infinity = std::numeric_limits<float>::infinity(); std::memcpy(bytes.data(), &infinity, sizeof(infinity)); return bytes; }(), false,
                GlbSemanticErrorCode::InvalidGeometry);
    ok &= CheckContainerInfoMismatch();
    if (!ok) return 1;
    std::cout << "OpenRepublic embedded GLB semantic tests passed.\n";
    return 0;
}
