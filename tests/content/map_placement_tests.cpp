#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/terrain_preview.h"

#include <cmath>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void PutFloat(std::string& block, std::size_t offset, float value) {
    std::memcpy(block.data() + offset, &value, sizeof(value));
}

// One 0x548-byte record header with a name and a Y-rotation world matrix.
std::string MakeRecord(const std::string& name, float yaw, float x, float y, float z) {
    std::string block(0x548, '\0');
    std::memcpy(block.data(), name.c_str(), name.size() + 1);
    const std::size_t m = 0x31c;
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    // D3D RotationY: _11=c, _13=-s, _31=s, _33=c.
    PutFloat(block, m + 0 * 4, c);
    PutFloat(block, m + 2 * 4, -s);
    PutFloat(block, m + 5 * 4, 1.0f);
    PutFloat(block, m + 8 * 4, s);
    PutFloat(block, m + 10 * 4, c);
    PutFloat(block, m + 12 * 4, x);
    PutFloat(block, m + 13 * 4, y);
    PutFloat(block, m + 14 * 4, z);
    PutFloat(block, m + 15 * 4, 1.0f);
    return block;
}

std::string MakeFile(std::uint32_t count, const std::string& tail) {
    std::string bytes(4, '\0');
    std::memcpy(bytes.data(), &count, sizeof(count));
    bytes += MakeRecord("customs_post", 0.5f, -8000.25f, 27.5f, -5797.0f);
    // A decoy that looks like a name but has no valid matrix, inside the tail.
    bytes += tail + std::string("decoy_name\0", 11) + std::string(64, '\x7f');
    bytes += MakeRecord("pylon2", -1.25f, 120.0f, 3.0f, 44.5f);
    bytes += tail;
    return bytes;
}

void PutU32(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<char>((value >> (8 * i)) & 255);
}

std::string MakeExactFile(std::uint32_t version) {
    auto record = MakeRecord("authored_factory", 0.5f, 12.0f, 3.0f, 18.0f);
    PutU32(record, 0x514, 1);
    std::string extension(400, '\0');
    const std::uint32_t scalars = version == 121 ? 7 : 9;
    PutU32(extension, 0x18, scalars);
    std::string bytes(4, '\0');
    PutU32(bytes, 0, 1);
    return bytes + record + extension + std::string(128 + scalars * 4, '\0');
}

void TestExactExtractionAndLoad() {
    std::vector<PlacedBuilding> found;
    std::string error;
    for (const auto version : {121u, 124u}) {
        const bool parsed = ExtractPlacedBuildingsExact(MakeExactFile(version), version, found, &error);
        Check(parsed, "strict versioned placement fixture parses: " + error);
        if (!parsed || found.size() != 1) return;
        Check(found.size() == 1 && found[0].sourceBuildingTableIndex == 0 &&
                  found[0].recordOffset == 4 && found[0].typeName == "authored_factory" && found[0].X() == 12.0,
              "exact placement exposes its source table slot and record offset");
    }
    auto firstRecord = MakeRecord("real_first", 0.0f, 1.0f, 2.0f, 3.0f);
    PutU32(firstRecord, 0x400, 8);
    PutU32(firstRecord, 0x514, 1);
    std::string firstExtension(400, '\0');
    PutU32(firstExtension, 0x18, 7);
    auto opaqueTail = MakeRecord("plausible_tail", 0.0f, 4.0f, 5.0f, 6.0f);
    opaqueTail.resize(8 * 188, '\0');
    std::string decoyFile(4, '\0');
    PutU32(decoyFile, 0, 2);
    decoyFile += firstRecord + firstExtension + opaqueTail + std::string(128 + 7 * 4, '\0');
    const auto secondRecordOffset = decoyFile.size();
    decoyFile += MakeExactFile(121).substr(4);
    Check(ExtractPlacedBuildings(decoyFile, found, &error) && found.size() == 2 &&
              found[1].typeName == "plausible_tail" && !found[1].sourceBuildingTableIndex,
          "a complete plausible tail decoy demonstrates the candidate scanner's identity limit");
    Check(ExtractPlacedBuildingsExact(decoyFile, 121, found, &error) && found.size() == 2 &&
              found[1].typeName == "authored_factory" && found[1].sourceBuildingTableIndex == 1 &&
              found[1].recordOffset == secondRecordOffset,
          "strict extraction ignores the complete tail decoy and identifies the actual second record");
    if (!ExtractPlacedBuildingsExact(MakeExactFile(124), 124, found, &error) || found.size() != 1) {
        Check(false, "restore strict fixture after decoy regression");
        return;
    }
    const auto keep = found;
    auto invalid = MakeExactFile(124);
    PutFloat(invalid, 4 + 0x31c + 5 * 4, 1.5f);
    Check(!ExtractPlacedBuildingsExact(invalid, 124, found, &error) && found.size() == keep.size() &&
              found[0].sourceBuildingTableIndex == keep[0].sourceBuildingTableIndex,
          "invalid transform at a strict boundary fails transactionally");
    Check(!ExtractPlacedBuildingsExact(MakeExactFile(124), 125, found, &error),
          "unknown version is not assigned a verified source identity");
    Check(ExtractPlacedBuildings(MakeFile(2, ""), found, &error) && found.size() == 2 &&
              !found[0].sourceBuildingTableIndex && !found[1].sourceBuildingTableIndex,
          "candidate scanner never assigns native source table identity");

    const auto tempBase = std::filesystem::temp_directory_path();
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root;
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        const auto path = tempBase / ("OpenRepublic-placement-header-tests-" +
                                     std::to_string(nonce) + "-" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directory(path, ec)) { root = path; break; }
        if (ec && ec != std::errc::file_exists) break;
    }
    Check(!root.empty(), "create an exclusive placement test directory");
    if (root.empty()) return;
    const auto write = [&](const char* name, const std::string& bytes) {
        std::ofstream stream(root / name, std::ios::binary);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        Check(static_cast<bool>(stream), "write authored placement fixture");
    };
    const auto header = [](std::uint32_t version) {
        std::string bytes(0x20c, '\0');
        PutU32(bytes, 0, version);
        return bytes;
    };
    write("buildings_game.bin", MakeExactFile(124));
    write("headerE.bin", header(124));
    Check(LoadMapPlacements(root, found, &error) && found.size() == 1 &&
              found[0].sourceBuildingTableIndex == 0,
          "package headerE fallback selects strict extraction");
    write("header.bin", header(121));
    write("buildings_game.bin", MakeExactFile(121));
    Check(LoadMapPlacements(root, found, &error) && found.size() == 1 &&
              found[0].sourceBuildingTableIndex == 0,
          "primary package header takes precedence over headerE");
    write("buildings_game.bin", MakeExactFile(121) + "trailing");
    Check(!LoadMapPlacements(root, found, &error) && found.size() == 1 &&
              found[0].sourceBuildingTableIndex == 0,
          "supported malformed profile cannot silently recover through candidate scanning");
    write("header.bin", header(125));
    Check(LoadMapPlacements(root, found, &error) && found.size() == 1 && !found[0].sourceBuildingTableIndex,
          "unsupported package version retains explicitly unidentified candidate extraction");
    write("header.bin", "short");
    Check(!LoadMapPlacements(root, found, &error) && !found[0].sourceBuildingTableIndex,
          "truncated primary header fails instead of falling through to headerE or scanning");
    std::filesystem::remove(root / "header.bin");
    std::filesystem::remove(root / "headerE.bin");
    Check(LoadMapPlacements(root, found, &error) && found.size() == 1 && !found[0].sourceBuildingTableIndex,
          "headerless authored legacy inputs retain candidate behavior");
    std::filesystem::remove(root / "buildings_game.bin");
    std::filesystem::remove(root);
}

void TestExtraction() {
    const std::string tail(200, '\xAB');
    std::vector<PlacedBuilding> found;
    std::string error;
    Check(ExtractPlacedBuildings(MakeFile(2, tail), found, &error), "two records are extracted: " + error);
    Check(found.size() == 2 && found[0].typeName == "customs_post" && found[1].typeName == "pylon2",
          "type names are read in file order");
    Check(found.size() == 2 && found[0].X() == -8000.25 && found[0].Y() == 27.5 && found[0].Z() == -5797.0,
          "translation is read from the matrix");
    Check(found.size() == 2 && std::fabs(found[0].YawRadians() - 0.5) < 1e-5 &&
              std::fabs(found[1].YawRadians() + 1.25) < 1e-5,
          "yaw is recovered with the D3D RotationY sign");

    std::vector<PlacedBuilding> keep(1);
    Check(!ExtractPlacedBuildings(MakeFile(3, tail), keep, &error) && !error.empty() && keep.size() == 1,
          "a count mismatch fails and leaves output unchanged");
    Check(!ExtractPlacedBuildings("abc", keep, &error), "a short file fails");
    std::string huge(4, '\0');
    const std::uint32_t tooMany = static_cast<std::uint32_t>(MaxPlacedBuildings + 1);
    std::memcpy(huge.data(), &tooMany, sizeof(tooMany));
    Check(!ExtractPlacedBuildings(huge, keep, &error), "an oversized count is rejected");

    std::string skewed = MakeFile(2, tail);
    PutFloat(skewed, 4 + 0x31c + 5 * 4, 1.5f); // first record loses its affine shape
    Check(!ExtractPlacedBuildings(skewed, keep, &error), "a non-affine matrix is not accepted as a record");
}

void TestRegionSelection() {
    std::vector<PlacedBuilding> list(4);
    const double xs[] = {0.0, 30.0, 60.0, 1000.0};
    for (std::size_t i = 0; i < list.size(); ++i) {
        list[i].matrix[12] = static_cast<float>(xs[i]);
    }
    const auto near = SelectPlacementsInRadius(list, 1, 40.0);
    Check(near.size() == 3 && near[0] == 0 && near[1] == 1 && near[2] == 2,
          "placements within the radius are selected in file order, including the centre");
    Check(SelectPlacementsInRadius(list, 9, 40.0).empty(), "an invalid centre selects nothing");
    Check(FindDensestPlacement(list, 40.0) == 1, "the densest placement is the middle of the cluster");
    Check(FindDensestPlacement({}, 40.0) == 0, "an empty list returns index 0");
}

void TestTransformAndRegion() {
    TerrainMetadata metadata;
    metadata.worldSizeX = 20000.0f;
    metadata.worldSizeY = 20000.0f;
    metadata.worldPositionY = -230.0f;
    metadata.heightScale = 800.0f;
    const TerrainTransform transform = MakeMapTerrainTransform(metadata);
    Check(transform.originX == -10000.0 && transform.originZ == -10000.0 && transform.sizeX == 20000.0 &&
              transform.baseHeight == -230.0 && transform.heightScale == 800.0 &&
              transform.orientation == RasterOrientation::Identity,
          "the map transform is centred on the world position");

    Heightmap map;
    map.width = 3;
    map.height = 3;
    map.samples = {0.0f, 0.0f, 0.0f, 0.25f, 0.25f, 0.25f, 0.5f, 0.5f, 0.5f}; // rows rise with Z
    TerrainPreviewGrid grid;
    std::string error;
    Check(BuildTerrainRegionGrid(map, transform, -5000.0, -5000.0, 10000.0, 10000.0, 3, 3, grid, &error),
          "a centred region grid builds: " + error);
    // Region rows at z = -5000, 0, 5000 -> raster fractions 0.25, 0.5, 0.75 -> raw 0.125, 0.25, 0.375.
    Check(grid.heights.size() == 9 && std::fabs(grid.heights[0] - (0.125f * 800.0f - 230.0f)) < 1e-3f &&
              std::fabs(grid.heights[3] - (0.25f * 800.0f - 230.0f)) < 1e-3f &&
              std::fabs(grid.heights[6] - (0.375f * 800.0f - 230.0f)) < 1e-3f,
          "region heights are sampled from the full terrain with rows ascending in Z");
    TerrainPreviewGrid keep;
    keep.width = 9;
    Check(!BuildTerrainRegionGrid(map, transform, 9000.0, 0.0, 2000.0, 100.0, 3, 3, keep, &error) &&
              keep.width == 9,
          "a region outside the terrain fails and leaves output unchanged");
    Check(!BuildTerrainRegionGrid(map, transform, 0.0, 0.0, 0.0, 100.0, 3, 3, keep, &error),
          "a zero-size region is rejected");
}

} // namespace

int main() {
    TestExtraction();
    TestExactExtractionAndLoad();
    TestRegionSelection();
    TestTransformAndRegion();
    if (failures != 0) {
        std::cerr << failures << " map-placement test(s) failed\n";
        return 1;
    }
    std::cout << "All map-placement tests passed\n";
    return 0;
}
