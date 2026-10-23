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
    // VERIFIED by the original import loader (static review, build context 23935965):
    // +0x1a4 is passed to the camera's set-position call and +0x1b0 to its set-look-at
    // call, each as three 32-bit floats (stored here as read, no unit/axis claim);
    // +0x1d8 == 1 gates reading the extended block that follows; +0x1e0 and +0x1dc are
    // the counts of the two lists of 0x80-byte text records that follow the 0x54-byte block
    // (the +0x1e0 list is read first). Record contents beyond "text" remain UNKNOWN.
    std::array<float, 3> cameraPosition{};
    std::array<float, 3> cameraLookAt{};
    std::uint32_t extendedBlockFlag = 0;
    std::uint32_t firstRecordListCount = 0;   // +0x1e0
    std::uint32_t secondRecordListCount = 0;  // +0x1dc
    // VERIFIED by consumer arithmetic in the original's hint/achievement checks (elapsed
    // days = (yearNow - yearThen) * 365 + dayNow - dayThen, with the loader storing +0x19c
    // as the year and +0x198 as the day): the current in-game date. Whether the day is
    // zero-based or 1-based, and the calendar's month layout, are UNKNOWN.
    std::int32_t gameDay = 0;   // +0x198
    std::int32_t gameYear = 0;  // +0x19c
};

// Requires version 121 to 124 (see the .cpp note) and at least 0x20c bytes;
// everything else fails unchanged.
bool ParseMapHeaderFixedBlock(std::string_view bytes, MapHeaderFixedBlock& output,
                              std::string* error = nullptr);

// Start-year option mapping from the original's new-game setup (VERIFIED by static
// review, build context 23935965): option 0..6 -> 1960, 1970, 1980, 1930, 1940, 1950,
// 1920; an option above 1000 is used directly as the year. Other values: no evidence,
// returns false. The start day-of-year depends on terrain type (see docs/GAME_SETUP.md).
bool StartYearFromOption(int option, int& year);

} // namespace openrepublic::content
