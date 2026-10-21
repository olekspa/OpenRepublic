#include "openrepublic/content/mtl_preview_color.h"

#include <charconv>
#include <cmath>
#include <system_error>

namespace openrepublic::content {
namespace {

bool MatchesMtlNumericSyntax(const std::string& token) {
    if (token.empty()) return false;
    std::size_t cursor = 0;
    if (token[cursor] == '+' || token[cursor] == '-') ++cursor;
    bool haveDigits = false;
    while (cursor < token.size() && token[cursor] >= '0' && token[cursor] <= '9') {
        haveDigits = true;
        ++cursor;
    }
    if (cursor < token.size() && token[cursor] == '.') {
        ++cursor;
        while (cursor < token.size() && token[cursor] >= '0' && token[cursor] <= '9') {
            haveDigits = true;
            ++cursor;
        }
    }
    if (!haveDigits) return false;
    if (cursor < token.size() && (token[cursor] == 'e' || token[cursor] == 'E')) {
        ++cursor;
        if (cursor < token.size() && (token[cursor] == '+' || token[cursor] == '-')) ++cursor;
        const std::size_t exponentStart = cursor;
        while (cursor < token.size() && token[cursor] >= '0' && token[cursor] <= '9') ++cursor;
        if (cursor == exponentStart) return false;
    }
    return cursor == token.size();
}

bool ParsePreviewFloat(const std::string& token, float& value) {
    if (!MatchesMtlNumericSyntax(token)) return false;

    // Floating from_chars follows the printf grammar, which excludes a leading
    // plus even though the MTL syntax profile accepts one.
    const char* begin = token.data();
    const char* end = begin + token.size();
    if (*begin == '+') {
        ++begin;
        if (begin == end) return false;
    }

    float parsed = 0.0f;
    const auto result = std::from_chars(begin, end, parsed, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

} // namespace

MtlPreviewDiffuseColor ExtractMtlDiffuseColorForPreview(const MtlSubmaterial& submaterial) {
    MtlPreviewDiffuseColor result;
    for (const MtlDirective& directive : submaterial.directives) {
        if (directive.name != "$DIFFUSECOLOR" ||
            directive.kind != MtlDirectiveKind::DiffuseColor)
            continue;

        result.status = MtlPreviewDiffuseColorStatus::Invalid;
        if (directive.arguments.size() != result.value.size()) continue;

        std::array<float, 4> parsed{};
        bool valid = true;
        for (std::size_t index = 0; index < parsed.size(); ++index) {
            if (!ParsePreviewFloat(directive.arguments[index], parsed[index])) {
                valid = false;
                break;
            }
        }
        if (valid) {
            result.status = MtlPreviewDiffuseColorStatus::Valid;
            result.value = parsed;
        }
    }
    return result;
}

} // namespace openrepublic::content
