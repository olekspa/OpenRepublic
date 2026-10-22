#include "openrepublic/content/tree_placements.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kHeaderBytes = 8;
constexpr std::size_t kNameBytes = 64;
constexpr std::size_t kSpeciesHeaderBytes = kNameBytes + 4;
constexpr std::size_t kRecordBytes = 17;
constexpr std::size_t kTrailerBytes = 4;
constexpr std::uintmax_t kMaxFileBytes = 64u * 1024u * 1024u;
constexpr double kMaxCoordinate = 1'000'000.0;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

std::uint32_t ReadU32LE(std::string_view bytes, std::size_t offset) {
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

float ReadF32LE(std::string_view bytes, std::size_t offset) {
    const std::uint32_t bits = ReadU32LE(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool IsModelNameChar(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

} // namespace

bool ExtractTreePlacements(std::string_view bytes, TreePlacements& output,
                           std::string* error) {
    if (bytes.size() < kHeaderBytes + kTrailerBytes)
        return Fail(error, "trees.bin is shorter than its header and trailer");

    const std::uint32_t speciesCount = ReadU32LE(bytes, 0);
    if (speciesCount > MaxTreeSpecies)
        return Fail(error, "tree species count exceeds the limit");

    TreePlacements candidate;
    candidate.opaqueHeaderValue = ReadU32LE(bytes, 4);
    candidate.species.reserve(speciesCount);
    std::size_t cursor = kHeaderBytes;
    std::uint64_t totalCount = 0;

    for (std::uint32_t typeIndex = 0; typeIndex < speciesCount; ++typeIndex) {
        if (cursor > bytes.size() || kSpeciesHeaderBytes > bytes.size() - cursor)
            return Fail(error, "tree species " + std::to_string(typeIndex) + " header is truncated");

        std::size_t nameLength = 0;
        while (nameLength < kNameBytes && bytes[cursor + nameLength] != '\0') {
            if (!IsModelNameChar(static_cast<unsigned char>(bytes[cursor + nameLength])))
                return Fail(error, "tree species " + std::to_string(typeIndex) + " has an invalid model name");
            ++nameLength;
        }
        if (nameLength == 0 || nameLength == kNameBytes)
            return Fail(error, "tree species " + std::to_string(typeIndex) + " name is empty or unterminated");
        const std::uint32_t count = ReadU32LE(bytes, cursor + kNameBytes);
        totalCount += count;
        if (totalCount > MaxTreePlacements)
            return Fail(error, "tree placement count exceeds the limit");
        candidate.species.push_back({std::string(bytes.data() + cursor, nameLength), count});
        cursor += kSpeciesHeaderBytes;

        const std::size_t countSize = static_cast<std::size_t>(count);
        if (countSize > (bytes.size() - cursor) / kRecordBytes)
            return Fail(error, "tree species " + std::to_string(typeIndex) + " records are truncated");
        const std::size_t recordStart = cursor;
        candidate.placements.reserve(static_cast<std::size_t>(totalCount));
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t recordOffset = recordStart + static_cast<std::size_t>(i) * kRecordBytes;
            const float x = ReadF32LE(bytes, recordOffset);
            const float y = ReadF32LE(bytes, recordOffset + 4);
            const float z = ReadF32LE(bytes, recordOffset + 8);
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                std::fabs(static_cast<double>(x)) > kMaxCoordinate ||
                std::fabs(static_cast<double>(y)) > kMaxCoordinate ||
                std::fabs(static_cast<double>(z)) > kMaxCoordinate)
                return Fail(error, "tree species " + std::to_string(typeIndex) +
                                       " has a non-finite or implausible position");
            TreePlacement placement;
            placement.typeIndex = typeIndex;
            placement.x = x;
            placement.y = y;
            placement.z = z;
            placement.recordOffset = recordOffset;
            candidate.placements.push_back(placement);
        }
        cursor += countSize * kRecordBytes;
    }

    if (cursor > bytes.size() || bytes.size() - cursor != kTrailerBytes)
        return Fail(error, "trees.bin has an unsupported trailing or missing payload");
    candidate.opaqueTrailer = ReadU32LE(bytes, cursor);
    candidate.diagnostic =
        "Partial decode: XYZ and species names are available; yaw, scale, and five record bytes remain unknown";

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

bool LoadTreePlacements(const std::filesystem::path& mapDirectory,
                        TreePlacements& output, std::string* error) {
    const auto path = mapDirectory / "trees.bin";
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxFileBytes)
        return Fail(error, "trees.bin is missing or exceeds the 64 MiB limit");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    std::ifstream in(path, std::ios::binary);
    in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!in || in.gcount() != static_cast<std::streamsize>(bytes.size()))
        return Fail(error, "trees.bin could not be read completely");
    return ExtractTreePlacements(bytes, output, error);
}

} // namespace openrepublic::content
