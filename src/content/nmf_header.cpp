#include "openrepublic/content/nmf_header.h"

#include <cstddef>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kNmfHeaderBytes = 20;
constexpr std::size_t kSubmaterialRecordBytes = 64;
constexpr std::size_t kSubmaterialCountOffset = 8;
constexpr std::size_t kObjectRecordCountOffset = 12;

void SetError(std::string* error, const char* message) {
    if (error) *error = message;
}

bool Fail(std::string* error, const char* message) {
    SetError(error, message);
    return false;
}

bool ReadU32LE(std::string_view bytes, std::size_t offset, std::uint32_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(std::uint32_t)) return false;
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    value = static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
    return true;
}

} // namespace

bool ParseNmfHeaderProfile(std::string_view bytes, NmfHeaderProfile& output,
                           std::string* error) {
    if (bytes.size() < kNmfHeaderBytes)
        return Fail(error, "NMF profile requires at least 20 header bytes");

    std::uint32_t submaterialCount = 0;
    std::uint32_t objectRecordCount = 0;
    if (!ReadU32LE(bytes, kSubmaterialCountOffset, submaterialCount) ||
        !ReadU32LE(bytes, kObjectRecordCountOffset, objectRecordCount))
        return Fail(error, "NMF profile header count fields are truncated");
    if (submaterialCount > MaxNmfHeaderSubmaterials)
        return Fail(error, "NMF submaterial count exceeds the 4096-record limit");
    if (objectRecordCount > MaxNmfHeaderObjectRecords)
        return Fail(error, "NMF object-record count exceeds the 4096-record limit");

    const std::size_t remaining = bytes.size() - kNmfHeaderBytes;
    if (static_cast<std::size_t>(submaterialCount) > remaining / kSubmaterialRecordBytes)
        return Fail(error, "NMF submaterial table extends beyond the provided bytes");

    NmfHeaderProfile candidate;
    candidate.objectRecordCount = objectRecordCount;
    candidate.submaterialIdentifiers.reserve(submaterialCount);
    for (std::uint32_t i = 0; i < submaterialCount; ++i) {
        const std::size_t offset = kNmfHeaderBytes + static_cast<std::size_t>(i) * kSubmaterialRecordBytes;
        const std::string_view record = bytes.substr(offset, kSubmaterialRecordBytes);
        const std::size_t terminator = record.find('\0');
        if (terminator == std::string_view::npos)
            return Fail(error, "NMF submaterial identifier has no terminator in its 64-byte record");
        if (terminator == 0)
            return Fail(error, "NMF submaterial identifier is empty");
        for (std::size_t character = 0; character < terminator; ++character) {
            const auto byte = static_cast<unsigned char>(record[character]);
            if (byte < 0x20u || byte > 0x7eu)
                return Fail(error, "NMF submaterial identifier is outside the printable ASCII profile");
        }
        candidate.submaterialIdentifiers.emplace_back(record.substr(0, terminator));
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
