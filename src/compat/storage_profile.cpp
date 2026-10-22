#include "openrepublic/compat/storage_profile.h"

#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace openrepublic::compat {
namespace {

constexpr std::size_t kMaxProfileBytes = 64u * 1024u;
constexpr std::size_t kMaxProfileRows = 4096;
constexpr std::size_t kMaxNameBytes = 96;
constexpr std::string_view kHeader = "OPENREPUBLIC_STORAGE_PROFILE 1";

bool IsSpace(char c) { return c == ' ' || c == '\t'; }

std::vector<std::string_view> Tokens(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t cursor = 0;
    while (cursor < line.size()) {
        while (cursor < line.size() && IsSpace(line[cursor]))
            ++cursor;
        if (cursor == line.size())
            break;
        const std::size_t start = cursor;
        while (cursor < line.size() && !IsSpace(line[cursor]))
            ++cursor;
        tokens.push_back(line.substr(start, cursor - start));
    }
    return tokens;
}

bool ValidName(std::string_view name) {
    if (name.empty() || name.size() > kMaxNameBytes)
        return false;
    for (const unsigned char c : name) {
        const bool alphaNumeric = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9');
        if (!alphaNumeric && c != '_' && c != '-' && c != '.')
            return false;
    }
    return true;
}

bool ParsePositiveDecimal(std::string_view token, double &value) {
    // The profile format accepts ordinary decimal notation, including an
    // optional fractional part, but not locale-specific spellings or NaN/Inf.
    std::size_t cursor = 0;
    bool sawDigit = false;
    while (cursor < token.size() && token[cursor] >= '0' && token[cursor] <= '9') {
        sawDigit = true;
        ++cursor;
    }
    if (cursor < token.size() && token[cursor] == '.') {
        ++cursor;
        const std::size_t fractionStart = cursor;
        while (cursor < token.size() && token[cursor] >= '0' && token[cursor] <= '9') {
            sawDigit = true;
            ++cursor;
        }
        if (!sawDigit && cursor == fractionStart)
            return false;
    }
    if (!sawDigit || cursor != token.size())
        return false;

    const auto result = std::from_chars(token.data(), token.data() + token.size(), value,
                                        std::chars_format::fixed);
    return result.ec == std::errc{} && result.ptr == token.data() + token.size() &&
           std::isfinite(value) && value > 0.0;
}

bool Fail(std::string &error, std::size_t line, const std::string &message) {
    error = "line " + std::to_string(line) + ": " + message;
    return false;
}

} // namespace

bool ParseStorageCapacityProfile(std::string_view text, StorageCapacityFactors &factors,
                                 std::string &error) {
    if (text.size() > kMaxProfileBytes) {
        error = "storage profile exceeds the 65536 byte limit";
        return false;
    }

    StorageCapacityFactors candidate;
    std::size_t lineNumber = 0;
    std::size_t cursor = 0;
    bool headerSeen = false;
    std::size_t rows = 0;
    while (cursor < text.size() || (text.empty() && lineNumber == 0)) {
        ++lineNumber;
        const std::size_t newline = text.find('\n', cursor);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        std::string_view line = text.substr(cursor, end - cursor);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        cursor = newline == std::string_view::npos ? text.size() : newline + 1;

        if (!headerSeen) {
            if (line != kHeader)
                return Fail(error, lineNumber, "expected OPENREPUBLIC_STORAGE_PROFILE 1 header");
            headerSeen = true;
            continue;
        }

        const auto tokens = Tokens(line);
        if (tokens.empty() || tokens.front().front() == '#')
            continue;
        if (tokens.size() != 4 || tokens[0] != "FACTOR")
            return Fail(error, lineNumber, "expected FACTOR <transportClass> <resource> <positive decimal>");
        if (!ValidName(tokens[1]) || !ValidName(tokens[2]))
            return Fail(error, lineNumber, "transport class and resource must be valid names");
        if (++rows > kMaxProfileRows)
            return Fail(error, lineNumber, "storage profile exceeds the 4096 row limit");
        double factor = 0.0;
        if (!ParsePositiveDecimal(tokens[3], factor))
            return Fail(error, lineNumber, "factor must be a positive finite decimal");
        const auto key = std::make_pair(std::string(tokens[1]), std::string(tokens[2]));
        if (!candidate.emplace(key, factor).second)
            return Fail(error, lineNumber, "duplicate transport class and resource pair");
    }
    if (!headerSeen)
        return Fail(error, 1, "expected OPENREPUBLIC_STORAGE_PROFILE 1 header");

    factors.swap(candidate);
    error.clear();
    return true;
}

bool LoadStorageCapacityProfile(const std::filesystem::path &path, StorageCapacityFactors &factors,
                                std::string &error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "could not open storage profile file";
        return false;
    }
    std::string text;
    text.resize(kMaxProfileBytes + 1);
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    const std::streamsize count = input.gcount();
    if (input.bad() || (input.fail() && !input.eof())) {
        error = "storage profile file read failed";
        return false;
    }
    if (count < 0 || static_cast<std::size_t>(count) > kMaxProfileBytes) {
        error = "storage profile exceeds the 65536 byte limit";
        return false;
    }
    text.resize(static_cast<std::size_t>(count));
    return ParseStorageCapacityProfile(text, factors, error);
}

} // namespace openrepublic::compat
