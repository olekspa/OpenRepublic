#include "openrepublic/content/map_placement.h"

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

bool ResolveMapPlacement(const std::filesystem::path& contentRoot,
                         const std::filesystem::path& mapDirectory, std::size_t index,
                         MapPlacementRequest& output, std::string* error) {
    MapPlacementRequest candidate;
    candidate.mapDirectory = mapDirectory;

    if (!LoadLegacyR32F(mapDirectory / "heightmap.dds", candidate.heightmap, error)) return false;

    std::string script;
    if (!ReadSmallFile(mapDirectory / "script.ini", script, error, "map script.ini")) return false;
    if (!ParseTerrainMetadata(script, candidate.heightmap, candidate.metadata, error)) return false;

    std::error_code ec;
    const auto placementBytes = std::filesystem::file_size(mapDirectory / "buildings_game.bin", ec);
    if (ec || placementBytes > 64u * 1024u * 1024u)
        return Fail(error, "buildings_game.bin is missing or exceeds the 64 MiB limit");
    std::string placements(static_cast<std::size_t>(placementBytes), '\0');
    {
        std::ifstream in(mapDirectory / "buildings_game.bin", std::ios::binary);
        in.read(placements.data(), static_cast<std::streamsize>(placements.size()));
        if (!in || in.gcount() != static_cast<std::streamsize>(placements.size()))
            return Fail(error, "buildings_game.bin could not be read completely");
    }
    std::vector<PlacedBuilding> all;
    if (!ExtractPlacedBuildings(placements, all, error)) return false;
    if (index >= all.size()) return Fail(error, "placement index is outside the map's building list");
    candidate.placement = all[index];
    candidate.placementIndex = index;
    candidate.placementCount = all.size();

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
