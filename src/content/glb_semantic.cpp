#include "openrepublic/content/glb_semantic.h"

#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace openrepublic::content {
namespace {

using Json = nlohmann::json;

constexpr std::uint32_t kGlbBinChunk = 0x004E4942;
constexpr std::size_t kMaxJsonDepth = 64;
constexpr std::size_t kMaxJsonNodes = 1'000'000;

struct BufferViewInfo {
    std::size_t offset = 0;
    std::size_t length = 0;
    std::size_t stride = 0;
    std::size_t target = 0;
};

struct AccessorInfo {
    std::size_t offset = 0;
    std::size_t count = 0;
    std::size_t stride = 0;
    std::uint32_t componentType = 0;
    std::size_t componentBytes = 0;
    std::size_t components = 0;
    std::string type;
    bool normalized = false;
};

bool Fail(GlbSemanticError& error, GlbSemanticErrorCode code, std::string message) {
    error.code = code;
    error.message = std::move(message);
    return false;
}

bool IsUnsignedInteger(const Json& value) {
    return value.is_number_unsigned() ||
           (value.is_number_integer() && value.get<std::int64_t>() >= 0);
}

bool ReadSize(const Json& object, const char* key, std::size_t& value, bool required,
              GlbSemanticError& error, GlbSemanticErrorCode code) {
    const auto it = object.find(key);
    if (it == object.end()) {
        if (required) return Fail(error, code, std::string("Missing required integer field '") + key + "'.");
        value = 0;
        return true;
    }
    if (!IsUnsignedInteger(*it)) {
        return Fail(error, code, std::string("Field '") + key + "' must be a non-negative integer.");
    }
    const auto parsed = it->get<std::uint64_t>();
    if (parsed > std::numeric_limits<std::size_t>::max()) {
        return Fail(error, code, std::string("Field '") + key + "' exceeds this platform's size range.");
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

bool GetArray(const Json& object, const char* key, const Json*& array,
              bool required, GlbSemanticError& error) {
    const auto it = object.find(key);
    if (it == object.end()) {
        if (required) return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                                  std::string("Missing required array '") + key + "'.");
        array = nullptr;
        return true;
    }
    if (!it->is_array()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                    std::string("Field '") + key + "' must be an array.");
    }
    if (it->size() > kMaxGlbSemanticArrayEntries) {
        return Fail(error, GlbSemanticErrorCode::LimitsExceeded,
                    std::string("Array '") + key + "' exceeds the configured entry limit.");
    }
    array = &*it;
    return true;
}

bool ReadU16LE(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < 2) return false;
    value = static_cast<std::uint16_t>(bytes[offset]) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
    return true;
}

bool ReadU32LE(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < 4) return false;
    value = static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    return true;
}

bool ReadFloatLE(const std::vector<std::uint8_t>& bytes, std::size_t offset, float& value) {
    std::uint32_t bits = 0;
    if (!ReadU32LE(bytes, offset, bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

std::size_t ComponentBytes(std::uint32_t type) {
    switch (type) {
    case 5120: // BYTE
    case 5121: // UNSIGNED_BYTE
        return 1;
    case 5122: // SHORT
    case 5123: // UNSIGNED_SHORT
        return 2;
    case 5125: // UNSIGNED_INT
    case 5126: // FLOAT
        return 4;
    default:
        return 0;
    }
}

std::size_t ComponentCount(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    return 0;
}

bool CheckNoExtensionsOrDeepJson(const Json& value, std::size_t depth,
                                 std::size_t& visited, GlbSemanticError& error) {
    if (depth > kMaxJsonDepth) {
        return Fail(error, GlbSemanticErrorCode::LimitsExceeded,
                    "GLB JSON nesting exceeds the configured depth limit.");
    }
    if (++visited > kMaxJsonNodes) {
        return Fail(error, GlbSemanticErrorCode::LimitsExceeded,
                    "GLB JSON exceeds the configured structural node limit.");
    }
    if (value.is_object()) {
        if (value.contains("extensions")) {
            return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                        "GLB uses an extension object unsupported by the bounded profile.");
        }
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (!CheckNoExtensionsOrDeepJson(it.value(), depth + 1, visited, error)) return false;
        }
    } else if (value.is_array()) {
        for (const auto& child : value) {
            if (!CheckNoExtensionsOrDeepJson(child, depth + 1, visited, error)) return false;
        }
    }
    return true;
}

bool ValidateTextureInfo(const Json& owner, const char* key, std::size_t textureCount,
                         GlbSemanticError& error) {
    const auto it = owner.find(key);
    if (it == owner.end()) return true;
    if (!it->is_object()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                    std::string("Texture info '") + key + "' must be an object.");
    }
    std::size_t index = 0;
    if (!ReadSize(*it, "index", index, true, error, GlbSemanticErrorCode::InvalidReference)) return false;
    if (index >= textureCount) {
        return Fail(error, GlbSemanticErrorCode::InvalidReference,
                    std::string("Texture info '") + key + "' references an out-of-range texture.");
    }
    if (it->contains("texCoord")) {
        std::size_t texCoord = 0;
        if (!ReadSize(*it, "texCoord", texCoord, true, error, GlbSemanticErrorCode::InvalidDocument)) return false;
        if (texCoord != 0) {
            return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                        "Only TEXCOORD_0 is supported by the bounded profile.");
        }
    }
    return true;
}

bool ValidateMaterialTextureReferences(const Json& materials, std::size_t textureCount,
                                      GlbSemanticError& error) {
    if (!materials.is_array()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB materials must be an array.");
    }
    for (std::size_t i = 0; i < materials.size(); ++i) {
        const auto& material = materials[i];
        if (!material.is_object()) {
            return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB material entry is not an object.");
        }
        const auto pbr = material.find("pbrMetallicRoughness");
        if (pbr != material.end()) {
            if (!pbr->is_object()) {
                return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                            "Material pbrMetallicRoughness must be an object.");
            }
            if (!ValidateTextureInfo(*pbr, "baseColorTexture", textureCount, error) ||
                !ValidateTextureInfo(*pbr, "metallicRoughnessTexture", textureCount, error)) return false;
        }
        if (!ValidateTextureInfo(material, "normalTexture", textureCount, error) ||
            !ValidateTextureInfo(material, "occlusionTexture", textureCount, error) ||
            !ValidateTextureInfo(material, "emissiveTexture", textureCount, error)) return false;
    }
    return true;
}

bool ValidateSamplerArray(const Json& document, GlbSemanticError& error) {
    const Json* samplers = nullptr;
    if (!GetArray(document, "samplers", samplers, false, error)) return false;
    if (samplers == nullptr) return true;
    const auto validEnum = [](const Json& sampler, const char* key,
                              const std::initializer_list<std::size_t>& allowed,
                              GlbSemanticError& output) {
        if (!sampler.contains(key)) return true;
        std::size_t value = 0;
        if (!ReadSize(sampler, key, value, true, output, GlbSemanticErrorCode::InvalidDocument)) return false;
        if (std::find(allowed.begin(), allowed.end(), value) == allowed.end()) {
            return Fail(output, GlbSemanticErrorCode::UnsupportedFeature,
                        std::string("Sampler field '") + key + "' has an unsupported enum value.");
        }
        return true;
    };
    for (const auto& sampler : *samplers) {
        if (!sampler.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB sampler entry is not an object.");
        if (!validEnum(sampler, "magFilter", {9728, 9729}, error) ||
            !validEnum(sampler, "minFilter", {9728, 9729, 9984, 9985, 9986, 9987}, error) ||
            !validEnum(sampler, "wrapS", {33071, 33648, 10497}, error) ||
            !validEnum(sampler, "wrapT", {33071, 33648, 10497}, error)) return false;
    }
    return true;
}

bool ValidateMinVersion(const Json& asset, GlbSemanticError& error) {
    const auto minVersion = asset.find("minVersion");
    if (minVersion == asset.end()) return true;
    if (!minVersion->is_string()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "asset.minVersion must be a version string.");
    }
    const std::string value = minVersion->get<std::string>();
    const auto dot = value.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 == value.size() || value.find('.', dot + 1) != std::string::npos) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "asset.minVersion must use a major.minor version form.");
    }
    const auto parsePart = [&](std::size_t start, std::size_t length, std::size_t& output) {
        output = 0;
        for (std::size_t i = start; i < start + length; ++i) {
            const char c = value[i];
            if (c < '0' || c > '9') return false;
            if (output > (std::numeric_limits<std::size_t>::max() - static_cast<std::size_t>(c - '0')) / 10) return false;
            output = output * 10 + static_cast<std::size_t>(c - '0');
        }
        return true;
    };
    std::size_t major = 0, minor = 0;
    if (!parsePart(0, dot, major) || !parsePart(dot + 1, value.size() - dot - 1, minor)) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "asset.minVersion is not a valid numeric major.minor version.");
    }
    if (major > 2 || (major == 2 && minor > 0)) {
        return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "asset.minVersion is newer than the supported glTF 2.0 profile.");
    }
    return true;
}

bool ValidateVertexSemantic(const std::string& semantic, const AccessorInfo& accessor,
                            GlbSemanticError& error) {
    if (semantic == "POSITION" || semantic == "NORMAL") {
        if (accessor.type != "VEC3" || accessor.componentType != 5126 || accessor.normalized) {
            return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                        semantic + " must be a non-normalized FLOAT VEC3 accessor.");
        }
        return true;
    }
    if (semantic == "TANGENT") {
        if (accessor.type != "VEC4" || accessor.componentType != 5126 || accessor.normalized) {
            return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                        "TANGENT must be a non-normalized FLOAT VEC4 accessor.");
        }
        return true;
    }
    if (semantic == "TEXCOORD_0") {
        const bool validFloat = accessor.type == "VEC2" && accessor.componentType == 5126 && !accessor.normalized;
        const bool validNormalizedInteger = accessor.type == "VEC2" && accessor.normalized &&
            (accessor.componentType == 5121 || accessor.componentType == 5123);
        if (!validFloat && !validNormalizedInteger) {
            return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                        "TEXCOORD_0 must be FLOAT VEC2 or normalized UNSIGNED_BYTE/UNSIGNED_SHORT VEC2.");
        }
        return true;
    }
    if (semantic == "COLOR_0") {
        const bool validFloat = (accessor.type == "VEC3" || accessor.type == "VEC4") &&
                                accessor.componentType == 5126 && !accessor.normalized;
        const bool validNormalizedInteger = (accessor.type == "VEC3" || accessor.type == "VEC4") &&
            accessor.normalized && (accessor.componentType == 5121 || accessor.componentType == 5123);
        if (!validFloat && !validNormalizedInteger) {
            return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                        "COLOR_0 must be FLOAT VEC3/VEC4 or normalized UNSIGNED_BYTE/UNSIGNED_SHORT VEC3/VEC4.");
        }
        return true;
    }
    return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                "Vertex semantic '" + semantic + "' is outside the supported POSITION/NORMAL/TANGENT/TEXCOORD_0/COLOR_0 profile.");
}

bool ValidateFiniteTransformFields(const Json& document, std::size_t meshCount,
                                   GlbSemanticError& error) {
    const auto nodesIt = document.find("nodes");
    const std::size_t nodeCount = nodesIt == document.end() ? 0 : nodesIt->size();
    if (nodesIt != document.end() && (!nodesIt->is_array() || nodesIt->size() > kMaxGlbSemanticArrayEntries)) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB nodes must be a bounded array.");
    }
    std::vector<std::size_t> parentCount(nodeCount, 0);
    if (nodesIt != document.end()) for (const auto& node : *nodesIt) {
        if (!node.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB node entry is not an object.");
        for (const char* key : {"skin", "camera", "weights"}) {
            if (node.contains(key)) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                                                std::string("Node field '") + key + "' is unsupported.");
        }
        if (node.contains("matrix") &&
            (node.contains("translation") || node.contains("rotation") || node.contains("scale"))) {
            return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                        "A node cannot combine matrix with translation, rotation, or scale.");
        }
        if (node.contains("mesh")) {
            std::size_t meshIndex = 0;
            if (!ReadSize(node, "mesh", meshIndex, true, error, GlbSemanticErrorCode::InvalidReference)) return false;
            if (meshIndex >= meshCount) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Node references an out-of-range mesh.");
        }
        if (node.contains("children")) {
            const auto& children = node["children"];
            if (!children.is_array() || children.size() > kMaxGlbSemanticArrayEntries) {
                return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Node children must be a bounded array.");
            }
            std::unordered_set<std::size_t> uniqueChildren;
            for (const auto& child : children) {
                if (!IsUnsignedInteger(child) || child.get<std::uint64_t>() >= nodeCount) {
                    return Fail(error, GlbSemanticErrorCode::InvalidReference, "Node contains an out-of-range child index.");
                }
                const auto childIndex = static_cast<std::size_t>(child.get<std::uint64_t>());
                if (!uniqueChildren.insert(childIndex).second) {
                    return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Node repeats a child index.");
                }
                if (++parentCount[childIndex] > 1) {
                    return Fail(error, GlbSemanticErrorCode::InvalidDocument, "A node cannot have multiple parents.");
                }
            }
        }
        for (const char* key : {"matrix", "translation", "rotation", "scale"}) {
            const auto it = node.find(key);
            if (it == node.end()) continue;
            if (!it->is_array()) {
                return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                            std::string("Node transform '") + key + "' must be an array.");
            }
            const std::size_t expected = std::string_view(key) == "matrix" ? 16 :
                                         std::string_view(key) == "translation" ? 3 :
                                         std::string_view(key) == "rotation" ? 4 : 3;
            if (it->size() != expected) {
                return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                            std::string("Node transform '") + key + "' has the wrong number of components.");
            }
            double quaternionNormSquared = 0.0;
            for (const auto& component : *it) {
                if (!component.is_number() || !std::isfinite(component.get<double>()) ||
                    std::abs(component.get<double>()) > std::numeric_limits<float>::max()) {
                    return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                                std::string("Node transform '") + key + "' contains a value that is not representable as a finite float.");
                }
                if (std::string_view(key) == "rotation") {
                    const double value = component.get<double>();
                    quaternionNormSquared += value * value;
                }
            }
            if (std::string_view(key) == "rotation" && std::abs(quaternionNormSquared - 1.0) > 1e-3) {
                return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                            "Node rotation quaternion must have unit length within the supported tolerance.");
            }
        }
    }
    if (nodesIt != document.end()) {
        // Iterative depth-first traversal avoids recursion on deep, but bounded,
        // input graphs. Parent counts above establish a forest unless a cycle
        // points back into the current path.
        std::vector<std::uint8_t> color(nodeCount, 0);
        std::vector<std::pair<std::size_t, std::size_t>> stack;
        for (std::size_t start = 0; start < nodeCount; ++start) {
            if (color[start] != 0) continue;
            color[start] = 1;
            stack.emplace_back(start, 0);
            while (!stack.empty()) {
                auto& frame = stack.back();
                const auto& node = (*nodesIt)[frame.first];
                const auto childrenIt = node.find("children");
                const std::size_t childCount = childrenIt == node.end() ? 0 : childrenIt->size();
                if (frame.second >= childCount) {
                    color[frame.first] = 2;
                    stack.pop_back();
                    continue;
                }
                const auto child = static_cast<std::size_t>((*childrenIt)[frame.second++].get<std::uint64_t>());
                if (color[child] == 1) {
                    return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Node graph contains a cycle.");
                }
                if (color[child] == 0) {
                    color[child] = 1;
                    stack.emplace_back(child, 0);
                }
            }
        }
    }
    const auto scenesIt = document.find("scenes");
    if (scenesIt != document.end()) {
        if (!scenesIt->is_array() || scenesIt->size() > kMaxGlbSemanticArrayEntries) {
            return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB scenes must be a bounded array.");
        }
        for (const auto& scene : *scenesIt) {
            if (!scene.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB scene entry is not an object.");
            const auto roots = scene.find("nodes");
            if (roots == scene.end()) continue;
            if (!roots->is_array() || roots->size() > kMaxGlbSemanticArrayEntries) {
                return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Scene nodes must be a bounded array.");
            }
            std::unordered_set<std::size_t> uniqueRoots;
            for (const auto& node : *roots) {
                if (!IsUnsignedInteger(node) || node.get<std::uint64_t>() >= nodeCount) {
                    return Fail(error, GlbSemanticErrorCode::InvalidReference, "Scene contains an out-of-range node index.");
                }
                const auto rootIndex = static_cast<std::size_t>(node.get<std::uint64_t>());
                if (!uniqueRoots.insert(rootIndex).second) {
                    return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Scene repeats a root node.");
                }
                if (parentCount[rootIndex] != 0) {
                    return Fail(error, GlbSemanticErrorCode::InvalidDocument, "A scene root cannot also be a child node.");
                }
            }
        }
        if (document.contains("scene")) {
            std::size_t activeScene = 0;
            if (!ReadSize(document, "scene", activeScene, true, error, GlbSemanticErrorCode::InvalidReference)) return false;
            if (activeScene >= scenesIt->size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Active scene references an out-of-range scene.");
        }
    } else if (document.contains("scene")) {
        return Fail(error, GlbSemanticErrorCode::InvalidReference, "Active scene is specified without a scenes array.");
    }
    return true;
}

} // namespace

bool ValidateEmbeddedGlbV2(const std::vector<std::uint8_t>& bytes,
                           const GlbContainerInfo& container,
                           GlbSemanticSummary& summary,
                           GlbSemanticError& error) {
    GlbContainerInfo checkedContainer;
    std::string framingError;
    if (!PreflightGlbContainer(bytes, checkedContainer, &framingError)) {
        return Fail(error, GlbSemanticErrorCode::InvalidContainerInfo,
                    "Supplied bytes do not pass bounded GLB framing: " + framingError);
    }
    if (checkedContainer.chunkCount != container.chunkCount ||
        checkedContainer.jsonChunkBytes != container.jsonChunkBytes ||
        checkedContainer.binaryChunkBytes != container.binaryChunkBytes ||
        checkedContainer.jsonChunkOffset != container.jsonChunkOffset) {
        return Fail(error, GlbSemanticErrorCode::InvalidContainerInfo,
                    "Supplied GLB container metadata does not describe the exact byte vector.");
    }
    if (container.chunkCount != 2 || container.binaryChunkBytes == 0) {
        return Fail(error, GlbSemanticErrorCode::InvalidContainerInfo,
                    "Bounded GLB profile requires exactly JSON and BIN chunks.");
    }
    if (container.jsonChunkBytes == 0 || container.jsonChunkBytes > kMaxGlbSemanticJsonBytes ||
        container.jsonChunkOffset > bytes.size() ||
        container.jsonChunkBytes > bytes.size() - container.jsonChunkOffset) {
        return Fail(error, container.jsonChunkBytes > kMaxGlbSemanticJsonBytes ?
                    GlbSemanticErrorCode::JsonTooLarge : GlbSemanticErrorCode::InvalidContainerInfo,
                    "GLB JSON chunk is empty, too large, or outside the supplied byte vector.");
    }
    const std::size_t jsonEnd = container.jsonChunkOffset + container.jsonChunkBytes;
    if (jsonEnd > bytes.size() || bytes.size() - jsonEnd < 8) {
        return Fail(error, GlbSemanticErrorCode::InvalidContainerInfo, "GLB BIN chunk header is missing.");
    }
    std::uint32_t actualBinBytes = 0;
    std::uint32_t actualBinType = 0;
    if (!ReadU32LE(bytes, jsonEnd, actualBinBytes) || !ReadU32LE(bytes, jsonEnd + 4, actualBinType) ||
        actualBinType != kGlbBinChunk || actualBinBytes != container.binaryChunkBytes ||
        actualBinBytes > bytes.size() - jsonEnd - 8 || jsonEnd + 8 + actualBinBytes != bytes.size()) {
        return Fail(error, GlbSemanticErrorCode::InvalidContainerInfo,
                    "GLB BIN chunk does not match the supplied container metadata.");
    }
    const std::size_t binOffset = jsonEnd + 8;
    const std::size_t binLength = actualBinBytes;

    const char* jsonBegin = reinterpret_cast<const char*>(bytes.data() + container.jsonChunkOffset);
    const char* jsonEndPtr = jsonBegin + container.jsonChunkBytes;
    Json document;
    try {
        document = Json::parse(jsonBegin, jsonEndPtr, nullptr, true, false);
    } catch (const std::exception& exception) {
        return Fail(error, GlbSemanticErrorCode::InvalidJson,
                    std::string("GLB JSON could not be parsed: ") + exception.what());
    }
    if (!document.is_object()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB JSON root must be an object.");
    }
    std::size_t visited = 0;
    if (!CheckNoExtensionsOrDeepJson(document, 0, visited, error)) return false;

    const auto assetIt = document.find("asset");
    if (assetIt == document.end() || !assetIt->is_object()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB has no asset object.");
    }
    const auto versionIt = assetIt->find("version");
    if (versionIt == assetIt->end() || !versionIt->is_string() || versionIt->get<std::string>() != "2.0") {
        return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Only glTF asset.version 2.0 is supported.");
    }
    if (!ValidateMinVersion(*assetIt, error)) return false;
    for (const char* key : {"extensionsUsed", "extensionsRequired"}) {
        const auto it = document.find(key);
        if (it != document.end()) {
            if (!it->is_array()) return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                                             std::string("Field '") + key + "' must be an array.");
            if (!it->empty()) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                                          std::string("GLB declares unsupported ") + key + ".");
        }
    }
    for (const char* key : {"animations", "skins", "cameras"}) {
        const auto it = document.find(key);
        if (it == document.end()) continue;
        if (!it->is_array() || it->size() > kMaxGlbSemanticArrayEntries) {
            return Fail(error, GlbSemanticErrorCode::InvalidDocument,
                        std::string("Field '") + key + "' must be a bounded array.");
        }
        if (!it->empty()) {
            return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                        std::string("glTF ") + key + " are outside the bounded static-mesh profile.");
        }
    }

    const Json* buffers = nullptr;
    if (!GetArray(document, "buffers", buffers, true, error)) return false;
    if (buffers == nullptr || buffers->size() != 1) {
        return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                    "Only one embedded GLB buffer is supported.");
    }
    const auto& buffer = (*buffers)[0];
    if (!buffer.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB buffer entry is not an object.");
    if (buffer.contains("uri")) {
        return Fail(error, GlbSemanticErrorCode::UnsupportedFeature,
                    "URI-backed buffers are unsupported; the bounded profile accepts embedded BIN data only.");
    }
    std::size_t declaredBufferBytes = 0;
    if (!ReadSize(buffer, "byteLength", declaredBufferBytes, true, error, GlbSemanticErrorCode::InvalidBinaryLayout)) return false;
    if (declaredBufferBytes == 0 || declaredBufferBytes > binLength || binLength - declaredBufferBytes > 3) {
        return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout,
                    "Embedded buffer byteLength must match the BIN chunk except for up to three padding bytes.");
    }

    const Json* views = nullptr;
    if (!GetArray(document, "bufferViews", views, true, error)) return false;
    if (views == nullptr || views->empty()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB requires at least one bufferView.");
    }
    std::vector<BufferViewInfo> viewInfo;
    viewInfo.reserve(views->size());
    for (std::size_t i = 0; i < views->size(); ++i) {
        const auto& view = (*views)[i];
        if (!view.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB bufferView entry is not an object.");
        std::size_t bufferIndex = 0, offset = 0, length = 0, stride = 0, target = 0;
        if (!ReadSize(view, "buffer", bufferIndex, true, error, GlbSemanticErrorCode::InvalidReference) ||
            !ReadSize(view, "byteOffset", offset, false, error, GlbSemanticErrorCode::InvalidBinaryLayout) ||
            !ReadSize(view, "byteLength", length, true, error, GlbSemanticErrorCode::InvalidBinaryLayout) ||
            !ReadSize(view, "byteStride", stride, false, error, GlbSemanticErrorCode::InvalidBinaryLayout) ||
            !ReadSize(view, "target", target, false, error, GlbSemanticErrorCode::InvalidDocument)) return false;
        if (bufferIndex != 0) return Fail(error, GlbSemanticErrorCode::InvalidReference, "bufferView references a non-existent buffer.");
        if (target != 0 && target != 34962 && target != 34963) {
            return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "bufferView target must be ARRAY_BUFFER or ELEMENT_ARRAY_BUFFER.");
        }
        if (length == 0 || offset > declaredBufferBytes || length > declaredBufferBytes - offset) {
            return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "bufferView exceeds the embedded buffer byteLength.");
        }
        if (stride != 0 && (stride < 4 || stride > 252 || stride % 4 != 0)) {
            return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "bufferView byteStride is outside the supported glTF range.");
        }
        if (stride != 0 && target == 34963) {
            return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "Index bufferViews cannot specify byteStride.");
        }
        viewInfo.push_back({offset, length, stride, target});
    }

    const Json* accessors = nullptr;
    if (!GetArray(document, "accessors", accessors, true, error)) return false;
    if (accessors == nullptr || accessors->empty()) {
        return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB requires at least one accessor.");
    }
    std::vector<AccessorInfo> accessorInfo;
    accessorInfo.reserve(accessors->size());
    for (const auto& accessor : *accessors) {
        if (!accessor.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB accessor entry is not an object.");
        if (accessor.contains("sparse")) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Sparse accessors are unsupported by the bounded profile.");
        std::size_t viewIndex = 0, localOffset = 0, count = 0, componentType = 0;
        if (!ReadSize(accessor, "bufferView", viewIndex, true, error, GlbSemanticErrorCode::InvalidReference) ||
            !ReadSize(accessor, "byteOffset", localOffset, false, error, GlbSemanticErrorCode::InvalidBinaryLayout) ||
            !ReadSize(accessor, "count", count, true, error, GlbSemanticErrorCode::InvalidGeometry) ||
            !ReadSize(accessor, "componentType", componentType, true, error, GlbSemanticErrorCode::InvalidGeometry)) return false;
        if (viewIndex >= viewInfo.size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Accessor references an out-of-range bufferView.");
        if (count == 0 || count > kMaxGlbSemanticAccessorElements) return Fail(error, GlbSemanticErrorCode::LimitsExceeded, "Accessor count is empty or exceeds the configured element limit.");
        if (componentType > std::numeric_limits<std::uint32_t>::max()) return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "Accessor componentType is out of range.");
        const auto componentBytes = ComponentBytes(static_cast<std::uint32_t>(componentType));
        if (componentBytes == 0) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Accessor uses an unsupported componentType.");
        const auto typeIt = accessor.find("type");
        if (typeIt == accessor.end() || !typeIt->is_string()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Accessor type must be a string.");
        const std::string type = typeIt->get<std::string>();
        const auto components = ComponentCount(type);
        if (components == 0) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Accessor uses an unsupported type.");
        bool normalized = false;
        const auto normalizedIt = accessor.find("normalized");
        if (normalizedIt != accessor.end()) {
            if (!normalizedIt->is_boolean()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Accessor normalized must be boolean.");
            normalized = normalizedIt->get<bool>();
        }
        const auto& view = viewInfo[viewIndex];
        const std::size_t elementBytes = componentBytes * components;
        const std::size_t stride = view.stride == 0 ? elementBytes : view.stride;
        if (stride < elementBytes || localOffset % componentBytes != 0 || view.offset % componentBytes != 0) {
            return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "Accessor component alignment or stride is invalid.");
        }
        if (count - 1 > (std::numeric_limits<std::size_t>::max() - elementBytes) / stride) {
            return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "Accessor byte range overflows this platform.");
        }
        const std::size_t requiredBytes = (count - 1) * stride + elementBytes;
        if (localOffset > view.length || requiredBytes > view.length - localOffset) {
            return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "Accessor data exceeds its bufferView.");
        }
        if (accessor.contains("min") || accessor.contains("max")) {
            for (const char* key : {"min", "max"}) {
                const auto bounds = accessor.find(key);
                if (bounds == accessor.end()) continue;
                if (!bounds->is_array() || bounds->size() != components) {
                    return Fail(error, GlbSemanticErrorCode::InvalidDocument, std::string("Accessor '") + key + "' has the wrong component count.");
                }
                for (const auto& component : *bounds) {
                    if (!component.is_number() || !std::isfinite(component.get<double>())) {
                        return Fail(error, GlbSemanticErrorCode::InvalidGeometry, std::string("Accessor '") + key + "' contains a non-finite value.");
                    }
                }
            }
        }
        accessorInfo.push_back({view.offset + localOffset, count, stride,
                                static_cast<std::uint32_t>(componentType), componentBytes,
                                components, type, normalized});
    }
    std::size_t numericScanWork = 0;
    for (const auto& accessor : accessorInfo) {
        if (accessor.componentType != 5126) continue;
        if (accessor.count > (kMaxGlbSemanticValueScans - numericScanWork) / accessor.components) {
            return Fail(error, GlbSemanticErrorCode::LimitsExceeded,
                        "Aggregate floating-point accessor validation exceeds the configured scalar-read limit.");
        }
        numericScanWork += accessor.count * accessor.components;
    }
    for (const auto& accessor : accessorInfo) {
        if (accessor.componentType != 5126) continue;
        for (std::size_t element = 0; element < accessor.count; ++element) {
            const std::size_t start = binOffset + accessor.offset + element * accessor.stride;
            for (std::size_t component = 0; component < accessor.components; ++component) {
                float value = 0.0f;
                if (!ReadFloatLE(bytes, start + component * sizeof(float), value) || !std::isfinite(value)) {
                    return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                                "Floating-point accessor data contains a non-finite or truncated value.");
                }
            }
        }
    }

    static const Json emptyArray = Json::array();
    const Json* materials = nullptr;
    if (!GetArray(document, "materials", materials, false, error)) return false;
    if (materials == nullptr) materials = &emptyArray;
    const Json* textures = nullptr;
    if (!GetArray(document, "textures", textures, false, error)) return false;
    if (textures == nullptr) textures = &emptyArray;
    const Json* images = nullptr;
    if (!GetArray(document, "images", images, false, error)) return false;
    if (images == nullptr) images = &emptyArray;
    if (materials->size() > kMaxGlbSemanticArrayEntries || textures->size() > kMaxGlbSemanticArrayEntries || images->size() > kMaxGlbSemanticArrayEntries) {
        return Fail(error, GlbSemanticErrorCode::LimitsExceeded, "Material or image-related arrays exceed the configured limit.");
    }
    for (const auto& image : *images) {
        if (!image.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB image entry is not an object.");
        if (image.contains("uri")) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "URI-backed images are unsupported; embed images in bufferViews.");
        std::size_t viewIndex = 0;
        if (!ReadSize(image, "bufferView", viewIndex, true, error, GlbSemanticErrorCode::InvalidReference)) return false;
        if (viewIndex >= viewInfo.size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Image references an out-of-range bufferView.");
        const auto mime = image.find("mimeType");
        if (mime == image.end() || !mime->is_string() ||
            (mime->get<std::string>() != "image/png" && mime->get<std::string>() != "image/jpeg")) {
            return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Embedded image mimeType must be image/png or image/jpeg.");
        }
    }
    if (!ValidateSamplerArray(document, error)) return false;
    for (const auto& texture : *textures) {
        if (!texture.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB texture entry is not an object.");
        std::size_t source = 0;
        if (!ReadSize(texture, "source", source, true, error, GlbSemanticErrorCode::InvalidReference)) return false;
        if (source >= images->size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Texture references an out-of-range image.");
        if (texture.contains("sampler")) {
            std::size_t sampler = 0;
            if (!ReadSize(texture, "sampler", sampler, true, error, GlbSemanticErrorCode::InvalidReference)) return false;
            const Json* samplers = nullptr;
            if (!GetArray(document, "samplers", samplers, false, error)) return false;
            if (samplers == nullptr || sampler >= samplers->size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Texture references an out-of-range sampler.");
        }
    }
    if (!ValidateMaterialTextureReferences(*materials, textures->size(), error)) return false;

    const Json* meshes = nullptr;
    if (!GetArray(document, "meshes", meshes, true, error)) return false;
    if (meshes == nullptr || meshes->empty()) {
        return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "GLB requires at least one mesh.");
    }
    if (meshes->size() > kMaxGlbSemanticMeshes) return Fail(error, GlbSemanticErrorCode::LimitsExceeded, "GLB mesh count exceeds the configured limit.");
    GlbSemanticSummary candidate;
    candidate.meshCount = meshes->size();
    candidate.accessorCount = accessors->size();
    candidate.bufferViewCount = views->size();
    candidate.materialCount = materials->size();
    candidate.textureCount = textures->size();
    candidate.imageCount = images->size();
    std::vector<std::uint8_t> indexAccessorScanned(accessorInfo.size(), 0);
    std::vector<std::uint32_t> indexAccessorMaximum(accessorInfo.size(), 0);
    for (const auto& mesh : *meshes) {
        if (!mesh.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB mesh entry is not an object.");
        if (mesh.contains("weights")) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Mesh morph weights are unsupported.");
        const auto primitivesIt = mesh.find("primitives");
        if (primitivesIt == mesh.end() || !primitivesIt->is_array() || primitivesIt->empty()) {
            return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "GLB mesh must contain a non-empty primitive array.");
        }
        if (primitivesIt->size() > kMaxGlbSemanticPrimitives - candidate.primitiveCount) {
            return Fail(error, GlbSemanticErrorCode::LimitsExceeded, "GLB primitive count exceeds the configured limit.");
        }
        for (const auto& primitive : *primitivesIt) {
            if (!primitive.is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "GLB primitive entry is not an object.");
            if (primitive.contains("targets")) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Primitive morph targets are unsupported.");
            std::size_t mode = 4;
            if (primitive.contains("mode") && !ReadSize(primitive, "mode", mode, true, error, GlbSemanticErrorCode::InvalidDocument)) return false;
            if (mode != 4) return Fail(error, GlbSemanticErrorCode::UnsupportedFeature, "Only TRIANGLES primitive mode 4 is supported.");
            const auto attributesIt = primitive.find("attributes");
            if (attributesIt == primitive.end() || !attributesIt->is_object()) return Fail(error, GlbSemanticErrorCode::InvalidDocument, "Primitive attributes must be an object.");
            const auto positionIt = attributesIt->find("POSITION");
            if (positionIt == attributesIt->end() || !IsUnsignedInteger(*positionIt)) return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "Primitive is missing a valid POSITION accessor.");
            const auto positionIndex64 = positionIt->get<std::uint64_t>();
            if (positionIndex64 >= accessorInfo.size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Primitive POSITION references an out-of-range accessor.");
            const std::size_t positionIndex = static_cast<std::size_t>(positionIndex64);
            const auto& positions = accessorInfo[positionIndex];
            if (positions.type != "VEC3" || positions.componentType != 5126 || positions.normalized) {
                return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "POSITION accessor must be dense, non-normalized FLOAT VEC3.");
            }
            if (positions.count < 3) return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "POSITION accessor must contain at least three vertices.");
            for (auto it = attributesIt->begin(); it != attributesIt->end(); ++it) {
                if (!IsUnsignedInteger(it.value())) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Primitive attribute accessor index must be a non-negative integer.");
                const auto index = it.value().get<std::uint64_t>();
                if (index >= accessorInfo.size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Primitive attribute references an out-of-range accessor.");
                const auto& attribute = accessorInfo[static_cast<std::size_t>(index)];
                if (!ValidateVertexSemantic(it.key(), attribute, error)) return false;
                if (it.key() != "POSITION" && attribute.count != positions.count) {
                    return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "Primitive attribute accessor counts do not match POSITION.");
                }
            }
            const auto indicesIt = primitive.find("indices");
            if (indicesIt != primitive.end()) {
                if (!IsUnsignedInteger(*indicesIt)) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Primitive indices must reference an accessor.");
                const auto index64 = indicesIt->get<std::uint64_t>();
                if (index64 >= accessorInfo.size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Primitive indices reference an out-of-range accessor.");
                const std::size_t indexAccessorIndex = static_cast<std::size_t>(index64);
                const auto& indices = accessorInfo[indexAccessorIndex];
                if (indices.type != "SCALAR" || (indices.componentType != 5121 && indices.componentType != 5123 && indices.componentType != 5125) || indices.normalized) {
                    return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "Indices accessor must be a non-normalized unsigned SCALAR.");
                }
                if (indices.count < 3 || indices.count % 3 != 0) return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "Triangle index count must be at least three and divisible by three.");
                if (indexAccessorScanned[indexAccessorIndex] == 0) {
                    if (indices.count > kMaxGlbSemanticValueScans - numericScanWork) {
                        return Fail(error, GlbSemanticErrorCode::LimitsExceeded,
                                    "Aggregate index validation exceeds the configured scalar-read limit.");
                    }
                    numericScanWork += indices.count;
                    std::uint32_t maximumIndex = 0;
                    for (std::size_t element = 0; element < indices.count; ++element) {
                        const std::size_t start = binOffset + indices.offset + element * indices.stride;
                        std::uint32_t value = 0;
                        if (indices.componentType == 5121) value = bytes[start];
                        else if (indices.componentType == 5123) {
                            std::uint16_t shortValue = 0;
                            if (!ReadU16LE(bytes, start, shortValue)) return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "Index data is truncated.");
                            value = shortValue;
                        } else if (!ReadU32LE(bytes, start, value)) {
                            return Fail(error, GlbSemanticErrorCode::InvalidBinaryLayout, "Index data is truncated.");
                        }
                        maximumIndex = std::max(maximumIndex, value);
                    }
                    indexAccessorMaximum[indexAccessorIndex] = maximumIndex;
                    indexAccessorScanned[indexAccessorIndex] = 1;
                }
                if (indexAccessorMaximum[indexAccessorIndex] >= positions.count) {
                    return Fail(error, GlbSemanticErrorCode::InvalidGeometry,
                                "Triangle index references a vertex outside POSITION.");
                }
            } else if (positions.count % 3 != 0) {
                return Fail(error, GlbSemanticErrorCode::InvalidGeometry, "Non-indexed TRIANGLES POSITION count must be divisible by three.");
            }
            if (primitive.contains("material")) {
                std::size_t materialIndex = 0;
                if (!ReadSize(primitive, "material", materialIndex, true, error, GlbSemanticErrorCode::InvalidReference)) return false;
                if (materialIndex >= materials->size()) return Fail(error, GlbSemanticErrorCode::InvalidReference, "Primitive references an out-of-range material.");
            }
            candidate.primitiveCount++;
        }
    }
    if (!ValidateFiniteTransformFields(document, meshes->size(), error)) return false;
    summary = candidate;
    error = {};
    return true;
}

} // namespace openrepublic::content
