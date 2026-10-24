#include "workers_record_walk.h"

#include <limits>
#include <utility>

namespace openrepublic::content::detail {
namespace {

constexpr std::size_t kMaximumInputBytes = 512u * 1024u * 1024u;
constexpr std::uint32_t kMaximumRecords = 1u << 20;
constexpr std::size_t kBaseBytes = 0x728;
constexpr std::size_t kGateByteOffset = 0x700;
constexpr std::size_t kOptionalBlockFlagOffset = 0x70c;

std::uint32_t ReadU32(std::string_view bytes, std::size_t offset) {
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

bool Fail(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
    return false;
}

} // namespace

bool WalkWorkerRecordOffsets(std::string_view bytes,
                             std::vector<WorkerRecordSpan>& records,
                             std::string* error) {
    if (bytes.size() > kMaximumInputBytes)
        return Fail(error, "workers.bin exceeds the supported input bound");
    if (bytes.size() < 4)
        return Fail(error, "workers.bin is missing its 4-byte count");

    const std::uint32_t count = ReadU32(bytes, 0);
    if (count > kMaximumRecords)
        return Fail(error, "worker count exceeds the supported bound");
    if (count > (bytes.size() - 4) / kBaseBytes)
        return Fail(error, "worker count cannot fit the remaining input");

    std::vector<WorkerRecordSpan> parsed;
    parsed.reserve(count);
    std::size_t cursor = 4;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t start = cursor;
        if (kBaseBytes > bytes.size() - cursor)
            return Fail(error, "truncated worker base record at index " + std::to_string(i));
        cursor += kBaseBytes;

        const bool hasNineByteTail =
            static_cast<unsigned char>(bytes[start + kGateByteOffset]) != 0;
        const bool has128ByteTail =
            static_cast<unsigned char>(bytes[start + kOptionalBlockFlagOffset]) != 0;
        const std::size_t tailBytes = (hasNineByteTail ? 9 : 0) + (has128ByteTail ? 128 : 0);
        if (tailBytes > bytes.size() - cursor)
            return Fail(error, "truncated worker optional tail at index " + std::to_string(i));
        cursor += tailBytes;
        parsed.push_back({start, cursor - start});
    }
    if (cursor != bytes.size())
        return Fail(error, "workers.bin has trailing bytes after the counted records");

    records.swap(parsed);
    if (error != nullptr) error->clear();
    return true;
}

} // namespace openrepublic::content::detail
