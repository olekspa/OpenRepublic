#pragma once

#include "openrepublic/content/building_types.h"
#include "openrepublic/content/road_network.h"
#include "openrepublic/content/terrain_dds.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

// One building instance found in a map package's `buildings_game.bin`.
struct PlacedBuilding {
    std::string typeName;
    // Row-major 4x4 matrix in the D3D row-vector convention: world = local * M.
    // Translation is elements 12..14 (x, y, z); y is the base height on the map.
    std::array<float, 16> matrix{};
    std::size_t recordOffset = 0;

    double X() const { return matrix[12]; }
    double Y() const { return matrix[13]; }
    double Z() const { return matrix[14]; }
    // Rotation about +Y in radians, atan2(-m[2], m[0]); positive follows the
    // D3D RotationY convention. Both are derived from the matrix, not stored.
    double YawRadians() const;
};

constexpr std::size_t MaxPlacedBuildings = 65536;

// Extracts the building placements from `buildings_game.bin` bytes.
//
// FORMAT STATUS: INFERRED. The file starts with a little-endian u32 count and
// then one variable-length record per building; each record begins with a
// 0x548-byte raw object snapshot holding the NUL-terminated type name at offset
// 0 and the world matrix at offset 0x31c (matrix and name locations observed in
// one build and cross-checked against terrain heights and flat footprints; the
// remaining record fields and the tail sizes were not decoded). Instead of
// walking the unparsed tails, records are located by scanning for a plausible
// name followed by a valid affine Y-rotation matrix at the observed offset, and
// the call fails unless exactly `count` records are found. Records whose
// translation lies outside +-1,000,000 are rejected as implausible. On failure
// output is unchanged.
bool ExtractPlacedBuildings(std::string_view bytes, std::vector<PlacedBuilding>& output,
                            std::string* error = nullptr);

// Builds the TerrainTransform for a map package from its parsed `script.ini`:
// the horizontal extent is centred on the world position, the base height is
// the world position's Y and the height scale is `$HEIGHTSCALE`. Raster
// orientation is Identity (rows ascend with world Z, columns with world X).
// VERIFIED for one map package (28 building heights agree with median error
// 0.02 m); it is not checked on other maps.
TerrainTransform MakeMapTerrainTransform(const TerrainMetadata& metadata);

// Reads and extracts `<map>/buildings_game.bin`. Fails like ExtractPlacedBuildings
// or when the file is missing or larger than 64 MiB. On failure output is unchanged.
bool LoadMapPlacements(const std::filesystem::path& mapDirectory,
                       std::vector<PlacedBuilding>& output, std::string* error = nullptr);

// Indices of the placements whose origin lies within `radiusMeters` (horizontal
// distance) of placement `centerIndex`, in file order, including the centre.
std::vector<std::size_t> SelectPlacementsInRadius(const std::vector<PlacedBuilding>& placements,
                                                  std::size_t centerIndex, double radiusMeters);

// Index of the placement with the most neighbours (itself included) within
// `radiusMeters`; ties resolve to the lowest index. Returns 0 for an empty list.
std::size_t FindDensestPlacement(const std::vector<PlacedBuilding>& placements, double radiusMeters);

// Everything the preview needs to show one placed building on its terrain.
struct MapPlacementRequest {
    std::filesystem::path mapDirectory;
    Heightmap heightmap;
    TerrainMetadata metadata;
    PlacedBuilding placement;
    // Every placement in the package (the selected one is allPlacements[placementIndex]).
    std::vector<PlacedBuilding> allPlacements;
    // The package's road junction graph when `road.bin` is present and readable.
    RoadNetwork roads;
    bool hasRoads = false;
    std::size_t placementIndex = 0;
    std::size_t placementCount = 0;
    BuildingTypeAssetPaths assets;
};

// Reads `<map>/script.ini`, `<map>/heightmap.dds`, `<map>/
// buildings_game.bin` and `<contentRoot>/buildings/buildingtypes.ini`, selects
// placement `index` and resolves the placed type's model and MATERIAL under the
// content root. Fails with a diagnostic if the type has no building-type entry
// (for example the map's airspace markers). On failure output is unchanged.
// With `requireAssets` false the placed type's model/material are not resolved
// (`assets` stays empty); a region view uses this because its centre may be an
// unresolvable type such as an editor scratch placement.
bool ResolveMapPlacement(const std::filesystem::path& contentRoot,
                         const std::filesystem::path& mapDirectory, std::size_t index,
                         MapPlacementRequest& output, std::string* error = nullptr,
                         bool requireAssets = true);

} // namespace openrepublic::content
