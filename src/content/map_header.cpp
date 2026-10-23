#include "openrepublic/content/map_header.h"

#include <cstddef>
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

} // namespace openrepublic::content
