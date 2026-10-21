#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

constexpr std::size_t MaxNmfGeometryBytes = 256u * 1024u * 1024u;
constexpr std::size_t MaxNmfGeometryObjects = 4096;
constexpr std::size_t MaxNmfGeometryVertices = 4000000;
constexpr std::size_t MaxNmfGeometryIndices = 12000000;

struct NmfPosition {
    float x{};
    float y{};
    float z{};
};

struct NmfNormal {
    float x{};
    float y{};
    float z{};
};

struct NmfAabb {
    NmfPosition minimum{};
    NmfPosition maximum{};
};

struct NmfGeometrySubset {
    // Build-scoped draw range and submaterial-table index used by the original
    // loader/render path. Other records remain opaque auxiliary provenance.
    std::uint32_t indexOffset{};
    std::uint32_t indexCount{};
    std::uint16_t submaterialIndex{};
    std::uint16_t auxiliaryIndexCount{};
    std::vector<std::uint16_t> auxiliaryIndices;
};

struct NmfStaticLod {
    std::uint32_t lodSizeField{};
    std::uint32_t subsetCount{};
    std::uint32_t headerField0x10{};
    std::uint32_t vertexFlags{};
    std::uint32_t headerField0x18{};
    std::vector<NmfPosition> positions;
    // In build 23935965, flag 0x8 is loaded as the _NRM FLOAT3 vertex semantic.
    // Bits 0x1 and 0x2 have no vertex-element or input-stream branch in the
    // inspected loader and remain semantically unknown; all raw bits are retained.
    // Preserve source normals as-is; other builds and shading parity remain unknown.
    std::vector<NmfNormal> vertexNormals;
    std::vector<std::uint32_t> indices;
    std::vector<NmfGeometrySubset> subsets;
};

struct NmfStaticObject {
    std::uint32_t objectIndex{};
    std::uint32_t objectType{};
    // Preserved as observed metadata; it is not used as a type-0 data bound.
    std::uint32_t objectSizeField{};
    // Raw bytes are retained because the name encoding is not established.
    std::array<std::uint8_t, 64> nameBytes{};
    std::int16_t parentIndex{};
    std::int16_t childCount{};
    // Raw sequential float components; matrix storage/use convention is unknown.
    std::array<float, 16> nodeMatrixA{};
    std::array<float, 16> nodeMatrixB{};
    NmfAabb bounds{};
    NmfStaticLod lod;
};

struct NmfStaticGeometry {
    // Header identifiers are retained so observed subset indices stay linked
    // to their table entries without assigning MTL value or texture semantics.
    std::vector<std::string> submaterialIdentifiers;
    std::vector<NmfStaticObject> objects;
};

// Reads a narrow static triangle layout observed in one local NMF sample and
// the matching build-scoped ModelViewer loader. Only type-0 records with one
// LOD are accepted. Root/no-child and identity-matrix gates are sample-only;
// raw metadata is preserved, matrices are not applied, and nonzero +0x10/+0x18
// LOD fields are rejected based on an inferred morph interpretation. Recognized
// auxiliary vertex streams are bounds-checked then discarded. Subset draw
// ranges and their submaterial-table indices are retained based on the
// build-scoped loader/render evidence; auxiliary subset indices remain opaque.
// In build 23935965, flag 0x8 is the loader's _NRM FLOAT3 stream. Raw values are
// retained without applying the loader's optional coordinate-reflection mode.
// Other builds and MTL value/texture semantics remain unsupported.
// On failure, output is unchanged.
bool ParseNmfStaticGeometry(std::string_view bytes, NmfStaticGeometry& output,
                            std::string* error = nullptr);

// Checks the file size before reading. This is for trusted local probes, not
// an adversarial-file sandbox. On failure, output is unchanged.
bool LoadNmfStaticGeometry(const std::filesystem::path& path, NmfStaticGeometry& output,
                           std::string* error = nullptr);

} // namespace openrepublic::content
