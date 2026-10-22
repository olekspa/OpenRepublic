#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/terrain_preview.h"

#include <cmath>
#include <cstring>
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
    TestTransformAndRegion();
    if (failures != 0) {
        std::cerr << failures << " map-placement test(s) failed\n";
        return 1;
    }
    std::cout << "All map-placement tests passed\n";
    return 0;
}
