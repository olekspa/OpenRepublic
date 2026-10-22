#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace openrepublic::content {

constexpr std::size_t MaxTextTableBytes = 64u * 1024u * 1024u;
constexpr std::size_t MaxTextTableEntries = 1u << 20;

// Parses the localized text-table layout observed in one WRSR build's `.btf`
// language files (OBSERVED, one build, English only):
//   header: three big-endian u32 values - entry count, total file size, and an
//           unexplained value that is not used here;
//   index:  `count` records of {u32 id, u32 character offset, u16 character
//           length}, big-endian;
//   text:   UTF-16 big-endian characters starting at 12 + 10*count, each
//           string addressed by character offset and length.
// Strings are converted to UTF-8. The file-size header must match the input;
// every record must lie inside the text area and ids must be unique. Unpaired
// surrogates are replaced with U+FFFD. No claim is made about other builds or
// languages. On failure output is unchanged.
bool ParseTextTable(std::string_view bytes, std::map<std::uint32_t, std::string>& output,
                    std::string* error = nullptr);

} // namespace openrepublic::content
