#include "openrepublic/content/text_table.h"

#include <utility>

namespace openrepublic::content {
namespace {

bool Fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

std::uint32_t ReadU32(const unsigned char* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

std::uint16_t ReadU16(const unsigned char* p) {
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

void AppendUtf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

bool ParseTextTable(std::string_view bytes, std::map<std::uint32_t, std::string>& output,
                    std::string* error) {
    if (bytes.size() > MaxTextTableBytes) return Fail(error, "text table exceeds the 64 MiB limit");
    if (bytes.size() < 12) return Fail(error, "text table is shorter than its header");
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    const std::uint32_t count = ReadU32(data);
    if (ReadU32(data + 4) != bytes.size())
        return Fail(error, "text table size field does not match the input");
    if (count > MaxTextTableEntries) return Fail(error, "text table entry count exceeds the limit");
    const std::size_t textStart = 12 + static_cast<std::size_t>(count) * 10;
    if (textStart > bytes.size()) return Fail(error, "text table index is truncated");
    const std::size_t textChars = (bytes.size() - textStart) / 2;

    std::map<std::uint32_t, std::string> candidate;
    for (std::uint32_t i = 0; i < count; ++i) {
        const unsigned char* record = data + 12 + static_cast<std::size_t>(i) * 10;
        const std::uint32_t id = ReadU32(record);
        const std::uint32_t offset = ReadU32(record + 4);
        const std::uint16_t length = ReadU16(record + 8);
        if (offset > textChars || length > textChars - offset)
            return Fail(error, "text table record lies outside the text area");
        const unsigned char* text = data + textStart + static_cast<std::size_t>(offset) * 2;
        std::string utf8;
        for (std::uint16_t c = 0; c < length; ++c) {
            std::uint32_t unit = ReadU16(text + static_cast<std::size_t>(c) * 2);
            if (unit >= 0xD800 && unit <= 0xDBFF && c + 1 < length) {
                const std::uint32_t low = ReadU16(text + static_cast<std::size_t>(c + 1) * 2);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                    ++c;
                } else {
                    unit = 0xFFFD;
                }
            } else if (unit >= 0xD800 && unit <= 0xDFFF) {
                unit = 0xFFFD;
            }
            AppendUtf8(utf8, unit);
        }
        if (!candidate.emplace(id, std::move(utf8)).second)
            return Fail(error, "text table contains a duplicate id");
    }
    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
