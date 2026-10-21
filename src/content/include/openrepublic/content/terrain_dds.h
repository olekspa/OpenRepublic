#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace openrepublic::content {

// Raw row-major values only. No world transform, vertical scaling, offset, or
// orientation is implied by this data structure.
struct Heightmap {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<float> samples;
};

// Bounded parser for legacy single-level 2D DDS with FourCC 114 (R32F).
// Maximum accepted file size is 256 MiB and maximum sample count is 64M.
// On failure, output remains unchanged and error receives a diagnostic when
// non-null. The path overload checks file size before allocating its buffer.
bool LoadLegacyR32F(const std::vector<std::uint8_t>& bytes, Heightmap& output,
                    std::string* error = nullptr);
bool LoadLegacyR32F(const std::filesystem::path& path, Heightmap& output,
                    std::string* error = nullptr);

struct TerrainMetadata {
    float gridSize = 0.0f;
    float worldSizeX = 0.0f;
    float worldSizeY = 0.0f;
    float worldPositionX = 0.0f;
    float worldPositionY = 0.0f;
    float worldPositionZ = 0.0f;
    float heightScale = 0.0f;
    std::uint32_t heightmapWidth = 0;
    std::uint32_t heightmapHeight = 0;
};

// Parses the required keys $GRIDSIZE, $WORLDSIZE, $WORLDPOSITION,
// $HEIGHTSCALE, and $HEIGHTMAPSIZE as finite numeric metadata. This is
// intentionally syntactic: it does not apply heightScale/position to samples
// or infer row orientation. Unknown script.ini keys are ignored.
// Duplicate required keys, malformed values, and dimensions that differ from
// the supplied decoded heightmap are errors. On failure, output is unchanged.
bool ParseTerrainMetadata(const std::string& text, const Heightmap& heightmap,
                          TerrainMetadata& output, std::string* error = nullptr);

} // namespace openrepublic::content
