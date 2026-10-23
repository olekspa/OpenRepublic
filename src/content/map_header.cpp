#include "openrepublic/content/map_header.h"

#include <cstddef>
#include <cstring>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kMaximumInputBytes = 64u * 1024u;
constexpr std::size_t kMaximumTextCodeUnits = 256;

void SetError(std::string* error, const char* message) {
    if (error) *error = message;
}

bool Fail(std::string* error, const char* message) {
    SetError(error, message);
    return false;
}

std::uint16_t ReadU16LE(std::string_view bytes, std::size_t offset) {
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    return static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8);
}

} // namespace

bool ParseMapHeaderPrefix(std::string_view bytes, MapHeaderPrefix& output,
                          std::string* error) {
    if (bytes.size() > kMaximumInputBytes)
        return Fail(error, "map header prefix input exceeds the 64 KiB limit");
    if (bytes.size() < 8)
        return Fail(error, "map header prefix requires a leading value and text terminator");

    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    MapHeaderPrefix candidate;
    candidate.leadingValue = static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);

    const std::size_t availableCodeUnits = (bytes.size() - 4) / 2;
    const std::size_t scanCount = availableCodeUnits < kMaximumTextCodeUnits + 1
        ? availableCodeUnits : kMaximumTextCodeUnits + 1;
    bool terminated = false;
    for (std::size_t i = 0; i < scanCount; ++i) {
        const std::uint16_t unit = ReadU16LE(bytes, 4 + i * 2);
        if (unit == 0) {
            if (i == 0) return Fail(error, "map header prefix text candidate is empty");
            terminated = true;
            break;
        }
        if (unit >= 0xd800u && unit <= 0xdbffu) {
            if (i + 1 >= scanCount)
                return Fail(error, "map header prefix has an incomplete UTF-16 surrogate pair");
            const std::uint16_t low = ReadU16LE(bytes, 4 + (i + 1) * 2);
            if (low < 0xdc00u || low > 0xdfffu)
                return Fail(error, "map header prefix has an invalid UTF-16 surrogate pair");
            candidate.leadingTextCandidate.push_back(static_cast<char16_t>(unit));
            candidate.leadingTextCandidate.push_back(static_cast<char16_t>(low));
            ++i;
            continue;
        }
        if (unit >= 0xdc00u && unit <= 0xdfffu)
            return Fail(error, "map header prefix has an unpaired UTF-16 low surrogate");
        candidate.leadingTextCandidate.push_back(static_cast<char16_t>(unit));
    }
    if (!terminated)
        return Fail(error, "map header prefix text candidate lacks a bounded UTF-16LE terminator");

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}


bool ParseMapHeaderFixedBlock(std::string_view bytes, MapHeaderFixedBlock& output,
                              std::string* error) {
    constexpr std::size_t kFixedSize = 0x20c;
    constexpr std::size_t kWordsOffset = 0x184;
    if (bytes.size() > kMaximumInputBytes)
        return Fail(error, "map header fixed block input exceeds the 64 KiB limit");
    if (bytes.size() < kFixedSize)
        return Fail(error, "map header fixed block requires at least 0x20c bytes");
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    auto readU32 = [&](std::size_t offset) {
        return static_cast<std::uint32_t>(data[offset]) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 3]) << 24);
    };
    MapHeaderFixedBlock candidate;
    candidate.version = readU32(0);
    if (candidate.version != 124)
        return Fail(error, "map header fixed block layout is only evidenced for version 124");
    for (std::size_t i = 0; i < MapHeaderFixedBlock::kWordCount; ++i)
        candidate.words[i] = readU32(kWordsOffset + i * 4);
    for (std::size_t i = 0; i < candidate.flagBytes.size(); ++i)
        candidate.flagBytes[i] = data[0x208 + i];
    auto asFloat = [](std::uint32_t bits) { float f; static_assert(sizeof(f) == sizeof(bits)); std::memcpy(&f, &bits, sizeof(f)); return f; };
    for (std::size_t i = 0; i < 3; ++i) {
        candidate.cameraPosition[i] = asFloat(readU32(0x1a4 + i * 4));
        candidate.cameraLookAt[i] = asFloat(readU32(0x1b0 + i * 4));
    }
    candidate.extendedBlockFlag = readU32(0x1d8);
    candidate.firstRecordListCount = readU32(0x1e0);
    candidate.secondRecordListCount = readU32(0x1dc);
    candidate.gameDay = static_cast<std::int32_t>(readU32(0x198));
    candidate.gameYear = static_cast<std::int32_t>(readU32(0x19c));
    output = candidate;
    if (error) error->clear();
    return true;
}

bool StartYearFromOption(int option, int& year) {
    static constexpr int kYears[7] = {1960, 1970, 1980, 1930, 1940, 1950, 1920};
    if (option >= 0 && option <= 6) { year = kYears[option]; return true; }
    if (option > 1000) { year = option; return true; }
    return false;
}

} // namespace openrepublic::content
