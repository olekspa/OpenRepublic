#include "openrepublic/content/terrain_dds.h"
#include "openrepublic/content/terrain_preview.h"

#include <algorithm>
#include <cstdint>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace openrepublic::content;

namespace {

constexpr std::size_t kHeaderBytes = 128;
constexpr std::uint32_t kFlags = 0x00000001u | 0x00000002u | 0x00000004u | 0x00000008u | 0x00001000u;
int failures = 0;

void PutU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset + 0] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16u);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24u);
}

std::vector<std::uint8_t> MakeDds(std::uint32_t width = 2, std::uint32_t height = 2,
                                  const std::vector<std::uint32_t>& sampleBits = {0x3f800000u, 0x40000000u,
                                                                                  0x40400000u, 0x40800000u}) {
    std::vector<std::uint8_t> bytes(kHeaderBytes + sampleBits.size() * sizeof(std::uint32_t), 0);
    bytes[0] = 'D'; bytes[1] = 'D'; bytes[2] = 'S'; bytes[3] = ' ';
    PutU32(bytes, 4, 124);       // DDS_HEADER.dwSize
    PutU32(bytes, 8, kFlags);    // required flags
    PutU32(bytes, 12, height);
    PutU32(bytes, 16, width);
    PutU32(bytes, 20, width * 4u);
    PutU32(bytes, 24, 0);        // depth
    PutU32(bytes, 28, 0);        // mip count
    PutU32(bytes, 76, 32);       // DDS_PIXELFORMAT.dwSize
    PutU32(bytes, 80, 4);        // DDPF_FOURCC
    PutU32(bytes, 84, 114);      // D3DFMT_R32F
    PutU32(bytes, 108, 0x1000); // DDSCAPS_TEXTURE
    for (std::size_t i = 0; i < sampleBits.size(); ++i)
        PutU32(bytes, kHeaderBytes + i * sizeof(std::uint32_t), sampleBits[i]);
    return bytes;
}

std::vector<std::uint8_t> MakeRgbaDds(std::uint32_t width = 2, std::uint32_t height = 1) {
    std::vector<std::uint8_t> bytes(kHeaderBytes + static_cast<std::size_t>(width) * height * 4, 0);
    bytes[0] = 'D'; bytes[1] = 'D'; bytes[2] = 'S'; bytes[3] = ' ';
    PutU32(bytes, 4, 124);
    PutU32(bytes, 8, kFlags);
    PutU32(bytes, 12, height);
    PutU32(bytes, 16, width);
    PutU32(bytes, 20, width * 4u);
    PutU32(bytes, 24, 0);
    PutU32(bytes, 28, 0);
    PutU32(bytes, 76, 32);
    PutU32(bytes, 80, 0x41); // DDPF_RGB | DDPF_ALPHAPIXELS
    PutU32(bytes, 84, 0);    // no FourCC
    PutU32(bytes, 88, 32);   // RGBA8
    PutU32(bytes, 92, 0x000000ffu);
    PutU32(bytes, 96, 0x0000ff00u);
    PutU32(bytes, 100, 0x00ff0000u);
    PutU32(bytes, 104, 0xff000000u);
    PutU32(bytes, 108, 0x1000);
    return bytes;
}

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void ExpectDdsFailure(const std::vector<std::uint8_t>& bytes, const std::string& label) {
    Heightmap output{1, 1, {42.0f}};
    std::string error;
    Check(!LoadLegacyR32F(bytes, output, &error) && !error.empty(), label + " rejected with diagnostic");
    Check(output.width == 1 && output.height == 1 && output.samples == std::vector<float>{42.0f},
          label + " leaves caller output unchanged");
}

const std::string kValidMetadata =
    "# synthetic metadata; no transform semantics are assumed\n"
    "$GRIDSIZE 32\n"
    "$WORLDSIZE 20000 20000\n"
    "$WORLDPOSITION 0 -120 0\n"
    "$HEIGHTSCALE 600\n"
    "$HEIGHTMAPSIZE 2 2\n";

void TestValidDds() {
    const auto bytes = MakeDds();
    Heightmap output;
    std::string error;
    Check(LoadLegacyR32F(bytes, output, &error), "valid synthetic R32F DDS decodes");
    Check(error.empty(), "success clears optional diagnostic");
    Check(output.width == 2 && output.height == 2, "width and height are returned");
    Check(output.samples == std::vector<float>({1.0f, 2.0f, 3.0f, 4.0f}), "samples decode as raw row-major floats");
}

void TestRgbaDds() {
    auto bytes = MakeRgbaDds();
    const std::array<std::uint8_t, 8> sample{{1, 2, 3, 4, 250, 128, 17, 255}};
    std::copy(sample.begin(), sample.end(), bytes.begin() + static_cast<std::ptrdiff_t>(kHeaderBytes));
    Rgba8Raster raster;
    std::string error;
    Check(LoadLegacyRgba8(bytes, raster, &error), "valid synthetic RGBA8 DDS decodes");
    Check(error.empty(), "RGBA8 success clears optional diagnostic");
    Check(raster.width == 2 && raster.height == 1, "RGBA8 dimensions are returned");
    Check(raster.pixels == std::vector<std::uint8_t>(sample.begin(), sample.end()),
          "RGBA8 channel bytes are preserved in row-major order");

    auto expectFailure = [&](std::vector<std::uint8_t> invalid, const std::string& label) {
        Rgba8Raster unchanged{1, 1, {9, 8, 7, 6}};
        error.clear();
        Check(!LoadLegacyRgba8(invalid, unchanged, &error) && !error.empty(), label + " rejected with diagnostic");
        Check(unchanged.width == 1 && unchanged.height == 1 && unchanged.pixels == std::vector<std::uint8_t>{9, 8, 7, 6},
              label + " leaves caller output unchanged");
    };
    bytes = MakeRgbaDds();
    PutU32(bytes, 92, 0x000000f0u);
    expectFailure(bytes, "unsupported channel masks");
    bytes = MakeRgbaDds();
    PutU32(bytes, 20, 7);
    expectFailure(bytes, "invalid RGBA8 row pitch");
    bytes = MakeRgbaDds();
    bytes.push_back(0);
    expectFailure(bytes, "RGBA8 trailing payload byte");
    bytes = MakeRgbaDds();
    PutU32(bytes, 28, 2);
    expectFailure(bytes, "RGBA8 mip chain");
}

void TestDdsFailures() {
    auto bytes = MakeDds();
    bytes.resize(127);
    ExpectDdsFailure(bytes, "truncated legacy header");

    bytes = MakeDds();
    bytes[0] = 'X';
    ExpectDdsFailure(bytes, "invalid DDS magic");

    bytes = MakeDds();
    PutU32(bytes, 4, 123);
    ExpectDdsFailure(bytes, "malformed DDS header size");

    bytes = MakeDds();
    PutU32(bytes, 8, kFlags & ~0x00000008u);
    ExpectDdsFailure(bytes, "missing pitch flag");

    bytes = MakeDds();
    PutU32(bytes, 84, 0x30315844u); // "DX10"
    ExpectDdsFailure(bytes, "unsupported FourCC");

    bytes = MakeDds(0, 2, {});
    ExpectDdsFailure(bytes, "zero dimension");

    bytes = MakeDds();
    PutU32(bytes, 20, 7);
    ExpectDdsFailure(bytes, "invalid row pitch");

    bytes = MakeDds();
    bytes.push_back(0);
    ExpectDdsFailure(bytes, "trailing bytes");

    bytes = MakeDds(1, 1, {0x7fc00000u}); // quiet NaN
    ExpectDdsFailure(bytes, "NaN sample");
    bytes = MakeDds(1, 1, {0x7f800000u}); // +infinity
    ExpectDdsFailure(bytes, "infinite sample");

    bytes = MakeDds(1, 1, {0x3f800000u});
    PutU32(bytes, 28, 2);
    ExpectDdsFailure(bytes, "multiple mip levels");
    bytes = MakeDds(1, 1, {0x3f800000u});
    PutU32(bytes, 24, 1);
    ExpectDdsFailure(bytes, "volume depth");
    bytes = MakeDds(1, 1, {0x3f800000u});
    PutU32(bytes, 112, 0x200);
    ExpectDdsFailure(bytes, "cubemap topology");
    bytes = MakeDds(1, 1, {0x3f800000u});
    PutU32(bytes, 112, 0x400); // face flag without the general cubemap bit
    ExpectDdsFailure(bytes, "cubemap face topology");

    // Dimensions are rejected before payload allocation or pitch use.
    ExpectDdsFailure(MakeDds(8193u, 8192u, {}), "more than 64M samples");
    ExpectDdsFailure(MakeDds(8192u, 8192u, {}), "declared payload above file-byte cap");
    ExpectDdsFailure(MakeDds(0xffffffffu, 0xffffffffu, {}), "overflow/oversize dimensions");

    // Exercise the input-size guard itself. The allocation is bounded at just
    // over the documented 256 MiB ceiling and consists only of synthetic zeroes.
    std::vector<std::uint8_t> oversized(static_cast<std::size_t>(256ull * 1024ull * 1024ull + 1ull), 0);
    ExpectDdsFailure(oversized, "input larger than 256 MiB");
}

void TestFileDdsReader() {
    // Client and headless CTest trees may run at the same time. Exclusively
    // reserve a directory before writing so the two processes cannot share a fixture.
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path directory;
    std::error_code ec;
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        const auto candidate = std::filesystem::temp_directory_path() /
            ("openrepublic-terrain-test-" + std::to_string(nonce) + "-" + std::to_string(attempt));
        if (std::filesystem::create_directory(candidate, ec)) {
            directory = candidate;
            break;
        }
        if (ec) break;
    }
    Check(!directory.empty(), "temporary terrain fixture directory is reserved exclusively");
    if (directory.empty()) return;
    const auto path = directory / "synthetic_r32f.dds";
    const auto bytes = MakeDds(1, 1, {0x41200000u});
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    Heightmap output;
    std::string error;
    Check(LoadLegacyR32F(path, output, &error), "synthetic DDS path overload reads file");
    Check(output.width == 1 && output.height == 1 && output.samples == std::vector<float>{10.0f},
          "path overload returns decoded raw value");
    std::filesystem::remove(path, ec);
    Check(!ec, "temporary synthetic DDS is removed");
    std::filesystem::remove(directory, ec);
    Check(!ec, "temporary terrain fixture directory is removed");
}

bool ParseMetadata(const std::string& text, const Heightmap& input,
                   TerrainMetadata& output, std::string& error) {
    return ParseTerrainMetadata(text, input, output, &error);
}

void ExpectMetadataFailure(const std::string& text, const std::string& label,
                           const Heightmap& heightmap = Heightmap{2, 2, {}}) {
    TerrainMetadata output;
    output.gridSize = 999.0f;
    std::string error;
    Check(!ParseMetadata(text, heightmap, output, error) && !error.empty(), label + " rejected with diagnostic");
    Check(output.gridSize == 999.0f, label + " leaves caller metadata unchanged");
}

void TestTerrainMetadata() {
    Heightmap heightmap{2, 2, {1.0f, 2.0f, 3.0f, 4.0f}};
    TerrainMetadata metadata;
    std::string error;
    Check(ParseMetadata(kValidMetadata, heightmap, metadata, error), "valid five-key metadata parses");
    Check(error.empty(), "metadata success clears optional diagnostic");
    Check(metadata.gridSize == 32.0f && metadata.worldSizeX == 20000.0f && metadata.worldSizeY == 20000.0f &&
          metadata.worldPositionY == -120.0f && metadata.heightScale == 600.0f &&
          metadata.heightmapWidth == 2 && metadata.heightmapHeight == 2,
          "metadata values preserved without applying transform semantics");

    ExpectMetadataFailure("$GRIDSIZE 32\n$WORLDSIZE 20000 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 600\n",
                          "missing required heightmap-size key");
    ExpectMetadataFailure(kValidMetadata + "$GRIDSIZE 32\n", "duplicate required key");
    ExpectMetadataFailure("$GRIDSIZE NaN\n$WORLDSIZE 20000 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 600\n$HEIGHTMAPSIZE 2 2\n",
                          "non-finite malformed numeric value");
    ExpectMetadataFailure("$GRIDSIZE 32 extra\n$WORLDSIZE 20000 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 600\n$HEIGHTMAPSIZE 2 2\n",
                          "wrong value count");
    ExpectMetadataFailure("$GRIDSIZE 32\n$WORLDSIZE 20000 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 600\n$HEIGHTMAPSIZE 3 2\n",
                          "mismatched heightmap dimensions");
    ExpectMetadataFailure("$GRIDSIZE 0\n$WORLDSIZE 20000 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 600\n$HEIGHTMAPSIZE 2 2\n",
                          "out-of-range zero grid size");
    ExpectMetadataFailure("$GRIDSIZE 1e-50\n$WORLDSIZE 20000 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 600\n$HEIGHTMAPSIZE 2 2\n",
                          "grid size that underflows to zero as float");
    ExpectMetadataFailure("$GRIDSIZE 32\n$WORLDSIZE 1e-50 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 600\n$HEIGHTMAPSIZE 2 2\n",
                          "world size that underflows to zero as float");
    ExpectMetadataFailure("$GRIDSIZE 32\n$WORLDSIZE 20000 20000\n$WORLDPOSITION 0 -120 0\n$HEIGHTSCALE 1e-50\n$HEIGHTMAPSIZE 2 2\n",
                          "height scale that underflows to zero as float");
    ExpectMetadataFailure(kValidMetadata, "heightmap with no decoded dimensions", Heightmap{});
}

void TestTerrainSampling() {
    // Synthetic row-major ramp: X increases by 10 and each later raster row
    // increases by 100. The transform deliberately uses nonzero/negative
    // origins and no game-derived constants.
    const Heightmap ramp{3, 2, {0.0f, 10.0f, 20.0f, 100.0f, 110.0f, 120.0f}};
    TerrainTransform transform;
    transform.originX = -10.0;
    transform.originZ = 20.0;
    transform.baseHeight = 5.0;
    transform.sizeX = 20.0;
    transform.sizeZ = 10.0;
    transform.heightScale = 2.0;
    transform.orientation = RasterOrientation::Identity;
    double output = -777.0;
    std::string error;
    Check(SampleTerrainHeight(ramp, transform, 0.0, 22.5, output, &error),
          "bilinear sample succeeds for interior ramp point");
    Check(output == 75.0, "interior bilinear sample applies caller base and height scale");
    Check(error.empty(), "terrain sample success clears diagnostic");

    transform.orientation = RasterOrientation::FlipZ;
    Check(SampleTerrainHeight(ramp, transform, 0.0, 22.5, output, &error),
          "opposite raster row orientation succeeds");
    Check(output == 175.0, "caller-selected row orientation reverses Z interpolation");
    Check(SampleTerrainHeight(ramp, transform, -10.0, 20.0, output, &error) && output == 205.0,
          "inclusive minimum extent maps to the corresponding raster corner");
    Check(SampleTerrainHeight(ramp, transform, 10.0, 30.0, output, &error) && output == 45.0,
          "inclusive maximum extent maps to the opposite raster corner");
    TerrainTransform fractional;
    fractional.originX = 0.0;
    fractional.originZ = 0.0;
    fractional.sizeX = 1.0;
    fractional.sizeZ = 1.0;
    fractional.heightScale = 1.0;
    fractional.orientation = RasterOrientation::Identity;
    Check(SampleTerrainHeight(Heightmap{2, 2, {0.0f, 10.0f, 20.0f, 50.0f}}, fractional,
                              0.25, 0.75, output, &error) && output == 21.25,
          "fractional X and Z interpolation both contribute to bilinear result");

    TerrainTransform nonBinaryEdge;
    nonBinaryEdge.originX = 0.1;
    nonBinaryEdge.originZ = 0.1;
    nonBinaryEdge.sizeX = 0.2;
    nonBinaryEdge.sizeZ = 0.2;
    nonBinaryEdge.heightScale = 1.0;
    nonBinaryEdge.orientation = RasterOrientation::FlipZ;
    const double maxEdgeX = nonBinaryEdge.originX + nonBinaryEdge.sizeX;
    const double maxEdgeZ = nonBinaryEdge.originZ + nonBinaryEdge.sizeZ;
    Check(SampleTerrainHeight(ramp, nonBinaryEdge, maxEdgeX, maxEdgeZ, output, &error) && output == 20.0,
          "reversed row orientation samples max-Z edge with non-binary-representable extents");
    const Heightmap unusedInvalid{3, 2, {0.0f, 10.0f, std::numeric_limits<float>::quiet_NaN(),
                                        100.0f, 110.0f, 120.0f}};
    Check(SampleTerrainHeight(unusedInvalid, transform, -10.0, 20.0, output, &error) && output == 205.0,
          "non-finite sample outside the current corner footprint does not block an O(1) query");

    const auto expectFailure = [&](const Heightmap& map, const TerrainTransform& xform,
                                   double x, double z, const std::string& label) {
        output = -777.0;
        error.clear();
        Check(!SampleTerrainHeight(map, xform, x, z, output, &error) && !error.empty(),
              label + " rejected with diagnostic");
        Check(output == -777.0, label + " leaves caller output unchanged");
    };
    expectFailure(ramp, transform, 10.001, 25.0, "coordinate beyond inclusive extent");
    auto invalid = transform;
    invalid.sizeX = 0.0;
    expectFailure(ramp, invalid, 0.0, 25.0, "zero horizontal extent");
    invalid = transform;
    invalid.heightScale = std::numeric_limits<double>::infinity();
    expectFailure(ramp, invalid, 0.0, 25.0, "infinite height scale");
    invalid = transform;
    invalid.originX = std::numeric_limits<double>::max();
    invalid.sizeX = std::numeric_limits<double>::max();
    expectFailure(ramp, invalid, invalid.originX, 25.0, "non-finite world extent from finite inputs");
    invalid = transform;
    invalid.originX = 1.0e20;
    invalid.sizeX = 1.0;
    expectFailure(ramp, invalid, invalid.originX, 25.0, "positive X extent collapsed by floating-point precision");
    invalid = transform;
    invalid.originZ = 1.0e20;
    invalid.sizeZ = 1.0;
    expectFailure(ramp, invalid, 0.0, invalid.originZ, "positive Z extent collapsed by floating-point precision");
    invalid = transform;
    invalid.heightScale = std::numeric_limits<double>::max();
    expectFailure(Heightmap{1, 1, {std::numeric_limits<float>::max()}}, invalid,
                  0.0, 25.0, "height result overflow");
    invalid = transform;
    invalid.orientation = static_cast<RasterOrientation>(99);
    expectFailure(ramp, invalid, 0.0, 25.0, "unknown row orientation");
    expectFailure(Heightmap{3, 2, {1.0f}}, transform, 0.0, 25.0,
                  "sample count inconsistent with raster dimensions");
    expectFailure(Heightmap{2, 1, {0.0f, std::numeric_limits<float>::quiet_NaN()}}, transform,
                  0.0, 25.0, "non-finite raster value");
    expectFailure(ramp, transform, std::numeric_limits<double>::quiet_NaN(), 25.0,
                  "non-finite query coordinate");

    // Single-cell rasters remain well-defined over their explicitly supplied
    // horizontal extent, including when a zero height scale requests flat output.
    TerrainTransform flat = transform;
    flat.heightScale = 0.0;
    flat.baseHeight = -12.5;
    Check(SampleTerrainHeight(Heightmap{1, 1, {99.0f}}, flat, 0.0, 25.0, output, &error) &&
          output == -12.5, "single-cell raster with zero height scale yields flat terrain");

    const std::array<RasterOrientation, 8> orientations = {
        RasterOrientation::Identity,
        RasterOrientation::FlipX,
        RasterOrientation::FlipZ,
        RasterOrientation::Rotate180,
        RasterOrientation::Transpose,
        RasterOrientation::TransposeFlipX,
        RasterOrientation::TransposeFlipZ,
        RasterOrientation::TransposeRotate180,
    };
    const std::array<std::array<double, 4>, 8> expectedCorners = {{
        {{0.0, 2.0, 3.0, 5.0}},
        {{2.0, 0.0, 5.0, 3.0}},
        {{3.0, 5.0, 0.0, 2.0}},
        {{5.0, 3.0, 2.0, 0.0}},
        {{0.0, 3.0, 2.0, 5.0}},
        {{3.0, 0.0, 5.0, 2.0}},
        {{2.0, 5.0, 0.0, 3.0}},
        {{5.0, 2.0, 3.0, 0.0}},
    }};
    const Heightmap asymmetric{3, 2, {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f}};
    TerrainTransform orientationTransform;
    orientationTransform.sizeX = 1.0;
    orientationTransform.sizeZ = 1.0;
    orientationTransform.heightScale = 1.0;
    for (std::size_t i = 0; i < orientations.size(); ++i) {
        orientationTransform.orientation = orientations[i];
        Check(RasterOrientationSwapsAxes(orientations[i]) == (i >= 4),
              "orientation classification identifies raster-axis swaps");
        const bool cornersMatch =
            SampleTerrainHeight(asymmetric, orientationTransform, 0.0, 0.0, output, &error) && output == expectedCorners[i][0] &&
            SampleTerrainHeight(asymmetric, orientationTransform, 1.0, 0.0, output, &error) && output == expectedCorners[i][1] &&
            SampleTerrainHeight(asymmetric, orientationTransform, 0.0, 1.0, output, &error) && output == expectedCorners[i][2] &&
            SampleTerrainHeight(asymmetric, orientationTransform, 1.0, 1.0, output, &error) && output == expectedCorners[i][3];
        Check(cornersMatch, "caller-selected raster orientation maps asymmetric rectangular corners");
    }
}

void TestTerrainPreviewGrid() {
    const Heightmap ramp{3, 2, {0.0f, 10.0f, 20.0f, 100.0f, 110.0f, 120.0f}};
    TerrainTransform transform;
    transform.originX = -10.0;
    transform.originZ = 20.0;
    transform.baseHeight = 5.0;
    transform.sizeX = 20.0;
    transform.sizeZ = 10.0;
    transform.heightScale = 2.0;
    transform.orientation = RasterOrientation::Identity;

    TerrainPreviewGrid preview;
    std::string error = "stale error";
    Check(BuildTerrainPreviewGrid(ramp, transform, 5, 3, preview, &error),
          "non-square preview grid samples synthetic non-square heightmap");
    Check(error.empty(), "preview success clears diagnostic");
    Check(preview.width == 5 && preview.height == 3 && preview.heights.size() == 15,
          "preview reports dimensions and row-major sample count");
    Check(preview.worldXCoordinates == std::vector<double>({-10.0, -5.0, 0.0, 5.0, 10.0}) &&
          preview.worldZCoordinates == std::vector<double>({20.0, 25.0, 30.0}),
          "preview retains explicit ascending caller world coordinates including nonzero-origin endpoints");
    Check(preview.heights == std::vector<float>({5.0f, 15.0f, 25.0f, 35.0f, 45.0f,
                                                  105.0f, 115.0f, 125.0f, 135.0f, 145.0f,
                                                  205.0f, 215.0f, 225.0f, 235.0f, 245.0f}),
          "preview samples inclusive corners and bilinear X/Z interpolation in increasing world Z");
    Check(preview.minHeight == 5.0f && preview.maxHeight == 245.0f,
          "preview reports sampled height range");

    transform.orientation = RasterOrientation::FlipZ;
    Check(BuildTerrainPreviewGrid(ramp, transform, 5, 3, preview, &error),
          "preview supports caller-selected reversed source row order");
    Check(preview.heights == std::vector<float>({205.0f, 215.0f, 225.0f, 235.0f, 245.0f,
                                                  105.0f, 115.0f, 125.0f, 135.0f, 145.0f,
                                                  5.0f, 15.0f, 25.0f, 35.0f, 45.0f}),
          "reversed raster rows still produce output rows in increasing world Z");
    Check(preview.minHeight == 5.0f && preview.maxHeight == 245.0f,
          "reversed row order preserves sampled range");

    const TerrainPreviewGrid beforeFailure = preview;
    const auto expectFailure = [&](const Heightmap& map, const TerrainTransform& xform,
                                   std::uint32_t width, std::uint32_t height,
                                   const std::string& label) {
        error.clear();
        Check(!BuildTerrainPreviewGrid(map, xform, width, height, preview, &error) && !error.empty(),
              label + " rejected with diagnostic");
        Check(preview.width == beforeFailure.width && preview.height == beforeFailure.height &&
              preview.worldXCoordinates == beforeFailure.worldXCoordinates &&
              preview.worldZCoordinates == beforeFailure.worldZCoordinates && preview.heights == beforeFailure.heights &&
              preview.minHeight == beforeFailure.minHeight &&
              preview.maxHeight == beforeFailure.maxHeight,
              label + " leaves existing output unchanged");
    };
    expectFailure(ramp, transform, 1, 3, "preview width below surface minimum");
    expectFailure(ramp, transform, 3, 1, "preview height below surface minimum");
    expectFailure(ramp, transform, 4097, 2, "preview dimension above configured maximum");
    expectFailure(ramp, transform, 4096, 4096, "preview sample count above configured maximum");
    auto invalidTransform = transform;
    invalidTransform.sizeZ = 0.0;
    expectFailure(ramp, invalidTransform, 5, 3, "invalid transform extent");
    invalidTransform = transform;
    invalidTransform.originX = 1.0e20;
    invalidTransform.sizeX = 32768.0;
    expectFailure(ramp, invalidTransform, 5, 3,
                  "requested preview resolution collapses adjacent X coordinates");
    expectFailure(Heightmap{3, 2, {0.0f}}, transform, 5, 3,
                  "heightmap dimensions inconsistent with decoded samples");
    expectFailure(Heightmap{3, 2, {0.0f, 10.0f, 20.0f, 100.0f,
                                  std::numeric_limits<float>::quiet_NaN(), 120.0f}},
                  transform, 5, 3, "non-finite sample encountered during full-grid traversal");
}

} // namespace

int main() {
    TestValidDds();
    TestRgbaDds();
    TestDdsFailures();
    TestFileDdsReader();
    TestTerrainMetadata();
    TestTerrainSampling();
    TestTerrainPreviewGrid();
    if (failures != 0) {
        std::cerr << failures << " terrain reader check(s) failed\n";
        return 1;
    }
    std::cout << "All synthetic terrain reader checks passed\n";
    return 0;
}
