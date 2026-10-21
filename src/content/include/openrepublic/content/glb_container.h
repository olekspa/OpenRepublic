#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace openrepublic::content {

// This validates only the GLB v2 container framing. It does not parse JSON,
// validate glTF semantics, constrain external resources, prove importer safety,
// or establish licensing/redistribution rights.
struct GlbContainerInfo {
    std::uint32_t chunkCount = 0;
    std::uint32_t jsonChunkBytes = 0;
    std::uint32_t binaryChunkBytes = 0;
    std::size_t jsonChunkOffset = 0;
};

inline constexpr std::size_t kMaxGlbContainerBytes = 256u * 1024u * 1024u;
inline constexpr std::uint32_t kMaxGlbChunks = 4096;

bool PreflightGlbContainer(const std::vector<std::uint8_t>& bytes,
                           GlbContainerInfo& output,
                           std::string* error = nullptr,
                           std::size_t maxBytes = kMaxGlbContainerBytes);
bool ReadBoundedGlbContainer(const std::filesystem::path& path,
                             std::vector<std::uint8_t>& output,
                             std::string* error = nullptr,
                             std::size_t maxBytes = kMaxGlbContainerBytes);
bool PreflightGlbContainer(const std::filesystem::path& path,
                           GlbContainerInfo& output,
                           std::string* error = nullptr,
                           std::size_t maxBytes = kMaxGlbContainerBytes);

} // namespace openrepublic::content
