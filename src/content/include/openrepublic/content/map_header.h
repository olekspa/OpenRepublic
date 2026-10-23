#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace openrepublic::content {

// The only currently decoded portion of the observed header.bin profile.
// leadingValue and leadingTextCandidate preserve their position and encoding;
// neither is assigned a game semantic. The remainder of the file is opaque.
struct MapHeaderPrefix {
    std::uint32_t leadingValue = 0;
    std::u16string leadingTextCandidate;
};

// Reads a bounded little-endian u32 followed by a non-empty, NUL-terminated
// UTF-16LE candidate string. Maximum input is 64 KiB and maximum string length
// is 256 UTF-16 code units. This is a prefix decoder only; trailing bytes are
// intentionally uninterpreted. Output is unchanged on failure.
bool ParseMapHeaderPrefix(std::string_view bytes, MapHeaderPrefix& output,
                          std::string* error = nullptr);


// Fixed numeric block of the 0x20c-byte header prefix. VERIFIED layout for header
// version 124 by static review of the original writer (build context 23935965):
// u32 version at +0x000, UTF-16LE text at +0x004 (capacity 0x180 bytes), then 33
// little-endian 32-bit words at +0x184..+0x207 (floats, ints and bool-as-u32 mixed;
// only the types listed in docs/MAP_SURVEY.md are known), then flag bytes at
// +0x208, +0x209, +0x20a (+0x20b is padding). Word meanings are UNKNOWN and every
// word is returned raw. Other header versions are not covered by this evidence.
struct MapHeaderFixedBlock {
    static constexpr std::size_t kWordCount = 33;
    std::uint32_t version = 0;
    std::array<std::uint32_t, kWordCount> words{};  // words[i] is at +0x184 + 4*i
    std::array<std::uint8_t, 3> flagBytes{};        // +0x208, +0x209, +0x20a
};

// Requires version 124 and at least 0x20c bytes; everything else fails unchanged.
bool ParseMapHeaderFixedBlock(std::string_view bytes, MapHeaderFixedBlock& output,
                              std::string* error = nullptr);

} // namespace openrepublic::content
