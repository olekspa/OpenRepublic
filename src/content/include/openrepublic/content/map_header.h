#pragma once

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

} // namespace openrepublic::content
