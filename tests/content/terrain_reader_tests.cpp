#include "openrepublic/content/terrain_dds.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    const auto path = std::filesystem::temp_directory_path() / "openrepublic_synthetic_r32f_test.dds";
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
    std::error_code ec;
    std::filesystem::remove(path, ec);
    Check(!ec, "temporary synthetic DDS is removed");
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

} // namespace

int main() {
    TestValidDds();
    TestDdsFailures();
    TestFileDdsReader();
    TestTerrainMetadata();
    if (failures != 0) {
        std::cerr << failures << " terrain reader check(s) failed\n";
        return 1;
    }
    std::cout << "All synthetic terrain reader checks passed\n";
    return 0;
}
