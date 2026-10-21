#include "openrepublic/content/terrain_dds.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kDdsHeaderBytes = 128;
constexpr std::uint64_t kMaxSamples = 64ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxFileBytes = 256ull * 1024ull * 1024ull;
constexpr std::uint32_t kDdsdCaps = 0x00000001u;
constexpr std::uint32_t kDdsdHeight = 0x00000002u;
constexpr std::uint32_t kDdsdWidth = 0x00000004u;
constexpr std::uint32_t kDdsdPitch = 0x00000008u;
constexpr std::uint32_t kDdsdPixelFormat = 0x00001000u;
constexpr std::uint32_t kDdsdMipMapCount = 0x00020000u;
constexpr std::uint32_t kDdsdLinearSize = 0x00080000u;
constexpr std::uint32_t kDdsdDepth = 0x00800000u;
constexpr std::uint32_t kDdpfFourCc = 0x00000004u;
constexpr std::uint32_t kDdsCapsTexture = 0x00001000u;
constexpr std::uint32_t kDdsCapsComplex = 0x00000008u;
constexpr std::uint32_t kDdsCapsMipMap = 0x00400000u;
constexpr std::uint32_t kDdsCaps2CubeMapFlags = 0x0000fe00u;
constexpr std::uint32_t kDdsCaps2Volume = 0x00200000u;
constexpr std::uint32_t kD3DFmtR32F = 114u;
constexpr std::size_t kMaxMetadataBytes = 64u * 1024u;
constexpr double kMaxMetadataMagnitude = 1.0e9;

void SetError(std::string* error, const std::string& message) {
    if (error) *error = message;
}

std::uint32_t ReadU32LE(const std::uint8_t* data) noexcept {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8u) |
           (static_cast<std::uint32_t>(data[2]) << 16u) |
           (static_cast<std::uint32_t>(data[3]) << 24u);
}

bool Fail(std::string* error, const char* message) {
    SetError(error, message);
    return false;
}

bool ParseDds(const std::vector<std::uint8_t>& bytes, Heightmap& output, std::string* error) {
    if (bytes.size() > kMaxFileBytes) return Fail(error, "DDS input exceeds the 256 MiB file limit");
    if (bytes.size() < kDdsHeaderBytes) return Fail(error, "DDS is shorter than its 128-byte legacy header");
    if (bytes[0] != 'D' || bytes[1] != 'D' || bytes[2] != 'S' || bytes[3] != ' ')
        return Fail(error, "DDS magic is missing");

    const auto* h = bytes.data() + 4;
    const std::uint32_t headerSize = ReadU32LE(h + 0);
    const std::uint32_t flags = ReadU32LE(h + 4);
    const std::uint32_t height = ReadU32LE(h + 8);
    const std::uint32_t width = ReadU32LE(h + 12);
    const std::uint32_t pitch = ReadU32LE(h + 16);
    const std::uint32_t depth = ReadU32LE(h + 20);
    const std::uint32_t mipCount = ReadU32LE(h + 24);
    const std::uint32_t pixelFormatSize = ReadU32LE(h + 72);
    const std::uint32_t pixelFormatFlags = ReadU32LE(h + 76);
    const std::uint32_t fourCc = ReadU32LE(h + 80);
    const std::uint32_t caps = ReadU32LE(h + 104);
    const std::uint32_t caps2 = ReadU32LE(h + 108);

    if (headerSize != 124u || pixelFormatSize != 32u)
        return Fail(error, "DDS legacy header or pixel-format structure has an invalid size");
    constexpr std::uint32_t requiredFlags = kDdsdCaps | kDdsdHeight | kDdsdWidth | kDdsdPitch | kDdsdPixelFormat;
    if ((flags & requiredFlags) != requiredFlags || (flags & (kDdsdLinearSize | kDdsdDepth)) != 0)
        return Fail(error, "DDS flags do not describe a pitched 2D texture");
    if (pixelFormatFlags != kDdpfFourCc || fourCc != kD3DFmtR32F)
        return Fail(error, "only legacy FourCC D3DFMT_R32F (114) is supported");
    if (width == 0 || height == 0) return Fail(error, "DDS width and height must be nonzero");
    if (depth != 0 || mipCount > 1 || ((flags & kDdsdMipMapCount) != 0 && mipCount != 1))
        return Fail(error, "volume DDS files and mip chains are unsupported");
    if ((caps & kDdsCapsTexture) == 0 || (caps & (kDdsCapsComplex | kDdsCapsMipMap)) != 0 ||
        (caps2 & (kDdsCaps2CubeMapFlags | kDdsCaps2Volume)) != 0)
        return Fail(error, "cubemap, volume, complex, or non-texture DDS topology is unsupported");

    const std::uint64_t sampleCount = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (sampleCount == 0 || sampleCount > kMaxSamples)
        return Fail(error, "DDS dimensions exceed the 64M-sample limit");
    const std::uint64_t expectedPitch = static_cast<std::uint64_t>(width) * sizeof(float);
    if (expectedPitch > std::numeric_limits<std::uint32_t>::max() || pitch != expectedPitch)
        return Fail(error, "DDS row pitch must equal width times four bytes");
    if (sampleCount > (std::numeric_limits<std::uint64_t>::max() - kDdsHeaderBytes) / sizeof(float))
        return Fail(error, "DDS payload size overflows the supported size calculation");
    const std::uint64_t expectedBytes = kDdsHeaderBytes + sampleCount * sizeof(float);
    if (expectedBytes > kMaxFileBytes) return Fail(error, "DDS payload exceeds the 256 MiB file limit");
    if (bytes.size() != expectedBytes)
        return Fail(error, "DDS payload length does not exactly match its dimensions");

    static_assert(sizeof(float) == sizeof(std::uint32_t), "R32F requires 32-bit float storage");
    static_assert(std::numeric_limits<float>::is_iec559, "R32F requires IEEE-754 float support");
    Heightmap candidate;
    candidate.width = width;
    candidate.height = height;
    candidate.samples.resize(static_cast<std::size_t>(sampleCount));
    for (std::size_t i = 0; i < candidate.samples.size(); ++i) {
        const std::uint32_t bits = ReadU32LE(bytes.data() + kDdsHeaderBytes + i * sizeof(float));
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) return Fail(error, "R32F payload contains a NaN or infinity sample");
        candidate.samples[i] = value;
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

bool ParseNumbers(const std::vector<std::string>& tokens, std::size_t count,
                  std::vector<double>& values) {
    if (tokens.size() != count) return false;
    values.clear();
    values.reserve(count);
    for (const auto& token : tokens) {
        try {
            std::size_t consumed = 0;
            const double value = std::stod(token, &consumed);
            if (consumed != token.size() || !std::isfinite(value) || std::abs(value) > kMaxMetadataMagnitude)
                return false;
            values.push_back(value);
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool ParseUInt32(const std::string& token, std::uint32_t& value) {
    if (token.empty()) return false;
    std::uint64_t parsed = 0;
    for (const char c : token) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<unsigned>(c - '0');
        if (parsed > (std::numeric_limits<std::uint32_t>::max() - digit) / 10u) return false;
        parsed = parsed * 10u + digit;
    }
    value = static_cast<std::uint32_t>(parsed);
    return value != 0;
}

} // namespace

bool LoadLegacyR32F(const std::vector<std::uint8_t>& bytes, Heightmap& output, std::string* error) {
    return ParseDds(bytes, output, error);
}

bool LoadLegacyR32F(const std::filesystem::path& path, Heightmap& output, std::string* error) {
    std::error_code ec;
    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec) return Fail(error, "could not determine DDS file size");
    if (fileSize > kMaxFileBytes) return Fail(error, "DDS file exceeds the 256 MiB file limit");
    if (fileSize < kDdsHeaderBytes) return Fail(error, "DDS file is shorter than its 128-byte legacy header");

    std::ifstream stream(path, std::ios::binary);
    if (!stream) return Fail(error, "could not open DDS file");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.peek() != std::char_traits<char>::eof())
        return Fail(error, "DDS file changed during read or could not be read completely");
    return ParseDds(bytes, output, error);
}

bool ParseTerrainMetadata(const std::string& text, const Heightmap& heightmap,
                          TerrainMetadata& output, std::string* error) {
    if (text.size() > kMaxMetadataBytes) return Fail(error, "terrain metadata exceeds the 64 KiB input limit");

    TerrainMetadata candidate;
    bool haveGrid = false, haveWorldSize = false, haveWorldPosition = false;
    bool haveHeightScale = false, haveHeightmapSize = false;
    std::istringstream input(text);
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.resize(comment);
        std::istringstream row(line);
        std::string key;
        if (!(row >> key)) continue;
        std::vector<std::string> tokens;
        std::string token;
        while (row >> token) tokens.push_back(token);

        bool* seen = nullptr;
        std::size_t expectedCount = 0;
        if (key == "$GRIDSIZE") { seen = &haveGrid; expectedCount = 1; }
        else if (key == "$WORLDSIZE") { seen = &haveWorldSize; expectedCount = 2; }
        else if (key == "$WORLDPOSITION") { seen = &haveWorldPosition; expectedCount = 3; }
        else if (key == "$HEIGHTSCALE") { seen = &haveHeightScale; expectedCount = 1; }
        else if (key == "$HEIGHTMAPSIZE") { seen = &haveHeightmapSize; expectedCount = 2; }
        else continue;

        if (*seen) {
            return Fail(error, ("duplicate required terrain metadata key at line " + std::to_string(lineNumber)).c_str());
        }
        *seen = true;
        if (key == "$HEIGHTMAPSIZE") {
            if (tokens.size() != 2 || !ParseUInt32(tokens[0], candidate.heightmapWidth) ||
                !ParseUInt32(tokens[1], candidate.heightmapHeight))
                return Fail(error, ("malformed $HEIGHTMAPSIZE at line " + std::to_string(lineNumber)).c_str());
            continue;
        }

        std::vector<double> values;
        if (!ParseNumbers(tokens, expectedCount, values))
            return Fail(error, ("malformed numeric metadata at line " + std::to_string(lineNumber)).c_str());
        if (key == "$GRIDSIZE") {
            if (values[0] <= 0.0) return Fail(error, "$GRIDSIZE must be positive");
            candidate.gridSize = static_cast<float>(values[0]);
            if (!std::isfinite(candidate.gridSize) || candidate.gridSize <= 0.0f)
                return Fail(error, "$GRIDSIZE must remain positive and finite as a float");
        } else if (key == "$WORLDSIZE") {
            if (values[0] <= 0.0 || values[1] <= 0.0) return Fail(error, "$WORLDSIZE values must be positive");
            candidate.worldSizeX = static_cast<float>(values[0]);
            candidate.worldSizeY = static_cast<float>(values[1]);
            if (!std::isfinite(candidate.worldSizeX) || !std::isfinite(candidate.worldSizeY) ||
                candidate.worldSizeX <= 0.0f || candidate.worldSizeY <= 0.0f)
                return Fail(error, "$WORLDSIZE values must remain positive and finite as floats");
        } else if (key == "$WORLDPOSITION") {
            candidate.worldPositionX = static_cast<float>(values[0]);
            candidate.worldPositionY = static_cast<float>(values[1]);
            candidate.worldPositionZ = static_cast<float>(values[2]);
            if (!std::isfinite(candidate.worldPositionX) || !std::isfinite(candidate.worldPositionY) ||
                !std::isfinite(candidate.worldPositionZ))
                return Fail(error, "$WORLDPOSITION values must remain finite as floats");
        } else if (key == "$HEIGHTSCALE") {
            if (values[0] <= 0.0) return Fail(error, "$HEIGHTSCALE must be positive");
            candidate.heightScale = static_cast<float>(values[0]);
            if (!std::isfinite(candidate.heightScale) || candidate.heightScale <= 0.0f)
                return Fail(error, "$HEIGHTSCALE must remain positive and finite as a float");
        }
    }

    if (!(haveGrid && haveWorldSize && haveWorldPosition && haveHeightScale && haveHeightmapSize))
        return Fail(error, "terrain metadata is missing one or more required keys");
    if (heightmap.width == 0 || heightmap.height == 0 ||
        candidate.heightmapWidth != heightmap.width || candidate.heightmapHeight != heightmap.height)
        return Fail(error, "$HEIGHTMAPSIZE does not match the decoded heightmap dimensions");

    output = candidate;
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
