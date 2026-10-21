#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

constexpr std::size_t MaxNmfGeometryBytes = 256u * 1024u * 1024u;
constexpr std::size_t MaxNmfGeometrySubmeshes = 16384;
constexpr std::size_t MaxNmfGeometryVertices = 4000000;
constexpr std::size_t MaxNmfGeometryIndices = 12000000;

struct NmfPosition {
    float x{};
    float y{};
    float z{};
};

struct NmfStaticSubmesh {
    std::uint32_t objectIndex{};
    std::uint32_t submeshIndex{};
    std::vector<NmfPosition> positions;
    std::vector<std::uint32_t> indices;
};

struct NmfStaticGeometry {
    std::vector<NmfStaticSubmesh> submeshes;
};

// Reads the bounded static triangle profile observed in one local NMF sample
// and the matching build-scoped ModelViewer loader. Only type-0 object records
// are accepted. The profile validates and discards known auxiliary vertex and
// partition streams; it does not recover their material or animation meaning.
// On failure, output is unchanged.
bool ParseNmfStaticGeometry(std::string_view bytes, NmfStaticGeometry& output,
                            std::string* error = nullptr);

// Checks the file size before reading. This is for trusted local probes, not
// an adversarial-file sandbox. On failure, output is unchanged.
bool LoadNmfStaticGeometry(const std::filesystem::path& path,
                           NmfStaticGeometry& output,
                           std::string* error = nullptr);

} // namespace openrepublic::content
