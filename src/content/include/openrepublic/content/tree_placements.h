#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

// Decoded tree origin from one trees.bin species group.
// Type index and XYZ are OBSERVED in package bytes for game build 23935965.
// Yaw, scale, and the remaining five bytes of each record are UNKNOWN and are
// deliberately not interpreted. X/Y/Z are returned unmodified from their raw
// floats. Interpreting them as map metres in the centered +/-10 km world frame
// is INFERRED from sample ranges and separate one-map terrain/building evidence;
// tree orientation, raster alignment, and vertical datum are not verified.
struct TreePlacement {
    std::uint32_t typeIndex = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::optional<double> yawRadians;
    std::optional<double> scale;
    std::size_t recordOffset = 0;
};

struct TreeSpeciesGroup {
    std::string modelName;
    std::uint32_t placementCount = 0;
};

struct TreePlacements {
    std::vector<TreeSpeciesGroup> species;
    std::vector<TreePlacement> placements;
    // The four bytes following the final record are retained without assigning
    // semantics. Current package samples contain this trailer.
    std::uint32_t opaqueTrailer = 0;
    // Header word at +4 is retained as an opaque value; its relation to the
    // payload has not been established.
    std::uint32_t opaqueHeaderValue = 0;
    std::string diagnostic;
};

constexpr std::uint32_t MaxTreeSpecies = 256;
constexpr std::uint32_t MaxTreePlacements = 3'000'000;

// Decodes the observed build-23935965 trees.bin profile transactionally.
// The current profile is: u32 species count, opaque u32, then for each species
// a 64-byte species prefix containing a NUL-terminated ASCII model name,
// u32 count, and count 17-byte
// records. The first 12 bytes of each record are finite little-endian XYZ
// floats; the final five bytes remain opaque. Four trailing bytes are retained.
// The bytes after the name terminator in the 64-byte prefix are opaque. This
// profile is OBSERVED from private local package samples, not confirmed against
// the game's serializer; unsupported layouts fail with a diagnostic.
bool ExtractTreePlacements(std::string_view bytes, TreePlacements& output,
                           std::string* error = nullptr);

// Reads <mapDirectory>/trees.bin (up to 64 MiB) and delegates to the bounded
// in-memory decoder. On failure, output is unchanged.
bool LoadTreePlacements(const std::filesystem::path& mapDirectory,
                        TreePlacements& output, std::string* error = nullptr);

} // namespace openrepublic::content
