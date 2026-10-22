#include "openrepublic/content/map_placement.h"
#include "building_record_walk.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kRecordHeaderBytes = 0x548;
constexpr std::size_t kMatrixOffset = 0x31c;
constexpr std::size_t kMaxNameBytes = 63;
constexpr double kMaxCoordinate = 1000000.0;
constexpr std::uintmax_t kMaxSmallFileBytes = 64u * 1024u;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

bool IsNameChar(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

float ReadFloat(const char* p) {
    float value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

// Returns true when a record header starts at `offset`.
bool TryRecord(std::string_view bytes, std::size_t offset, PlacedBuilding& record) {
    if (offset + kRecordHeaderBytes > bytes.size()) return false;
    std::size_t length = 0;
    while (length <= kMaxNameBytes && offset + length < bytes.size() &&
           IsNameChar(static_cast<unsigned char>(bytes[offset + length])))
        ++length;
    if (length < 3 || length > kMaxNameBytes || bytes[offset + length] != '\0') return false;

    std::array<float, 16> m{};
    for (std::size_t i = 0; i < m.size(); ++i) {
        m[i] = ReadFloat(bytes.data() + offset + kMatrixOffset + i * 4);
        if (!std::isfinite(m[i])) return false;
    }
    const bool affine = std::fabs(m[5] - 1.0f) < 1e-3f && std::fabs(m[15] - 1.0f) < 1e-3f &&
                        std::fabs(m[1]) < 1e-4f && std::fabs(m[3]) < 1e-4f &&
                        std::fabs(m[7]) < 1e-4f && std::fabs(m[9]) < 1e-4f &&
                        std::fabs(m[11]) < 1e-4f && std::fabs(m[4]) < 1e-4f &&
                        std::fabs(m[6]) < 1e-4f &&
                        std::fabs(m[0] * m[0] + m[2] * m[2] - 1.0f) < 1e-2f &&
                        std::fabs(m[8] * m[8] + m[10] * m[10] - 1.0f) < 1e-2f;
    if (!affine) return false;
    for (std::size_t i = 12; i < 15; ++i) {
        if (std::fabs(m[i]) > kMaxCoordinate) return false;
    }
    record.typeName.assign(bytes.data() + offset, length);
    record.matrix = m;
    record.recordOffset = offset;
    return true;
}

bool ReadSmallFile(const std::filesystem::path& path, std::string& text, std::string* error,
                   const char* label) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxSmallFileBytes)
        return Fail(error, std::string(label) + " is missing or exceeds the 64 KiB limit");
    std::ifstream in(path, std::ios::binary);
    text.assign(static_cast<std::size_t>(size), '\0');
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (!in || in.gcount() != static_cast<std::streamsize>(text.size()))
        return Fail(error, std::string(label) + " could not be read completely");
    return true;
}

} // namespace

double PlacedBuilding::YawRadians() const {
    return std::atan2(-static_cast<double>(matrix[2]), static_cast<double>(matrix[0]));
}

bool ExtractPlacedBuildings(std::string_view bytes, std::vector<PlacedBuilding>& output,
                            std::string* error) {
    if (bytes.size() < 4) return Fail(error, "placement file is shorter than its count header");
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data(), sizeof(count));
    if (count > MaxPlacedBuildings) return Fail(error, "placement count exceeds the limit");

    std::vector<PlacedBuilding> found;
    found.reserve(count);
    std::size_t offset = 4;
    while (offset + kRecordHeaderBytes <= bytes.size() && found.size() < count) {
        PlacedBuilding record;
        if (TryRecord(bytes, offset, record)) {
            found.push_back(std::move(record));
            offset += kRecordHeaderBytes;
        } else {
            ++offset;
        }
    }
    if (found.size() != count)
        return Fail(error, "located " + std::to_string(found.size()) + " of " + std::to_string(count) +
                               " placement records");
    output = std::move(found);
    if (error) error->clear();
    return true;
}

bool ExtractPlacedBuildingsExact(std::string_view bytes, std::uint32_t formatVersion,
                                std::vector<PlacedBuilding>& output, std::string* error) {
    std::vector<std::size_t> offsets;
    if (!detail::WalkBuildingRecordOffsets(bytes, formatVersion, offsets, error)) return false;
    std::vector<PlacedBuilding> candidate;
    candidate.reserve(offsets.size());
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        PlacedBuilding record;
        if (!TryRecord(bytes, offsets[index], record))
            return Fail(error, "invalid placement at exact building record " + std::to_string(index));
        record.sourceBuildingTableIndex = static_cast<std::uint32_t>(index);
        candidate.push_back(std::move(record));
    }
    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

TerrainTransform MakeMapTerrainTransform(const TerrainMetadata& metadata) {
    TerrainTransform transform;
    transform.sizeX = metadata.worldSizeX;
    transform.sizeZ = metadata.worldSizeY;
    transform.originX = metadata.worldPositionX - transform.sizeX * 0.5;
    transform.originZ = metadata.worldPositionZ - transform.sizeZ * 0.5;
    transform.baseHeight = metadata.worldPositionY;
    transform.heightScale = metadata.heightScale;
    transform.orientation = RasterOrientation::Identity;
    return transform;
}

bool LoadMapPlacements(const std::filesystem::path& mapDirectory,
                       std::vector<PlacedBuilding>& output, std::string* error) {
    std::error_code ec;
    const auto path = mapDirectory / "buildings_game.bin";
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > 512u * 1024u * 1024u)
        return Fail(error, "buildings_game.bin is missing or exceeds the 512 MiB limit");
    std::string data(static_cast<std::size_t>(bytes), '\0');
    std::ifstream in(path, std::ios::binary);
    in.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!in || in.gcount() != static_cast<std::streamsize>(data.size()))
        return Fail(error, "buildings_game.bin could not be read completely");

    // The native package loader reads the first word of this fixed header
    // prefix as its save-format version, preferring header.bin over headerE.bin.
    auto headerPath = mapDirectory / "header.bin";
    const bool hasHeader = std::filesystem::exists(headerPath, ec);
    if (ec) return Fail(error, "package header status could not be read");
    if (!hasHeader) headerPath = mapDirectory / "headerE.bin";
    const bool hasVersionHeader = std::filesystem::exists(headerPath, ec);
    if (ec) return Fail(error, "package header status could not be read");
    if (hasVersionHeader) {
        std::array<unsigned char, 0x20c> header{};
        std::ifstream headerInput(headerPath, std::ios::binary);
        headerInput.read(reinterpret_cast<char*>(header.data()), header.size());
        if (!headerInput || headerInput.gcount() != static_cast<std::streamsize>(header.size()))
            return Fail(error, "package header is unreadable or shorter than its fixed prefix");
        const auto version = static_cast<std::uint32_t>(header[0]) |
                             (static_cast<std::uint32_t>(header[1]) << 8) |
                             (static_cast<std::uint32_t>(header[2]) << 16) |
                             (static_cast<std::uint32_t>(header[3]) << 24);
        if (version == 121 || version == 124)
            return ExtractPlacedBuildingsExact(data, version, output, error);
    }
    return ExtractPlacedBuildings(data, output, error);
}

std::vector<std::size_t> SelectPlacementsInRadius(const std::vector<PlacedBuilding>& placements,
                                                  std::size_t centerIndex, double radiusMeters) {
    std::vector<std::size_t> selected;
    if (centerIndex >= placements.size()) return selected;
    const double r2 = radiusMeters * radiusMeters;
    for (std::size_t i = 0; i < placements.size(); ++i) {
        const double dx = placements[i].X() - placements[centerIndex].X();
        const double dz = placements[i].Z() - placements[centerIndex].Z();
        if (dx * dx + dz * dz <= r2) selected.push_back(i);
    }
    return selected;
}

std::size_t FindDensestPlacement(const std::vector<PlacedBuilding>& placements, double radiusMeters) {
    std::size_t best = 0;
    std::size_t bestCount = 0;
    for (std::size_t i = 0; i < placements.size(); ++i) {
        const std::size_t count = SelectPlacementsInRadius(placements, i, radiusMeters).size();
        if (count > bestCount) {
            best = i;
            bestCount = count;
        }
    }
    return best;
}

bool ResolveMapPlacement(const std::filesystem::path& contentRoot,
                         const std::filesystem::path& mapDirectory, std::size_t index,
                         MapPlacementRequest& output, std::string* error, bool requireAssets) {
    MapPlacementRequest candidate;
    candidate.mapDirectory = mapDirectory;

    if (!LoadLegacyR32F(mapDirectory / "heightmap.dds", candidate.heightmap, error)) return false;

    std::string script;
    if (!ReadSmallFile(mapDirectory / "script.ini", script, error, "map script.ini")) return false;
    if (!ParseTerrainMetadata(script, candidate.heightmap, candidate.metadata, error)) return false;

    std::error_code ec;
    std::vector<PlacedBuilding> all;
    if (!LoadMapPlacements(mapDirectory, all, error)) return false;
    if (index >= all.size()) return Fail(error, "placement index is outside the map's building list");
    candidate.placement = all[index];
    candidate.placementIndex = index;
    candidate.placementCount = all.size();
    candidate.allPlacements = std::move(all);
    {
        // Roads are optional: a missing or unreadable file leaves hasRoads false.
        const auto roadPath = mapDirectory / "road.bin";
        const auto roadBytes = std::filesystem::file_size(roadPath, ec);
        if (!ec && roadBytes <= 256u * 1024u * 1024u) {
            std::string data(static_cast<std::size_t>(roadBytes), '\0');
            std::ifstream in(roadPath, std::ios::binary);
            in.read(data.data(), static_cast<std::streamsize>(data.size()));
            if (in && in.gcount() == static_cast<std::streamsize>(data.size())) {
                candidate.hasRoads = ExtractRoadNetwork(data, candidate.roads, nullptr);
            }
        }
    }

    if (!requireAssets) {
        output = std::move(candidate);
        if (error) error->clear();
        return true;
    }
    const auto iniPath = contentRoot / "buildings" / "buildingtypes.ini";
    const auto iniBytes = std::filesystem::file_size(iniPath, ec);
    if (ec || iniBytes > MaxBuildingTypesInputBytes)
        return Fail(error, "buildings/buildingtypes.ini is missing or exceeds the 8 MiB limit");
    std::string iniText(static_cast<std::size_t>(iniBytes), '\0');
    {
        std::ifstream in(iniPath, std::ios::binary);
        in.read(iniText.data(), static_cast<std::streamsize>(iniText.size()));
        if (!in || in.gcount() != static_cast<std::streamsize>(iniText.size()))
            return Fail(error, "buildings/buildingtypes.ini could not be read completely");
    }
    BuildingTypesDocument types;
    if (!ParseBuildingTypes(iniText, types, error)) return false;
    const BuildingTypeDefinition* type = FindBuildingType(types, candidate.placement.typeName);
    if (type == nullptr)
        return Fail(error, "placed type '" + candidate.placement.typeName +
                               "' has no entry in buildings/buildingtypes.ini");
    if (!ResolveBuildingTypeAssets(*type, 0, contentRoot, candidate.assets, error)) return false;

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
