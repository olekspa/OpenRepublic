#include "openrepublic/content/mtl_syntax.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kMaxInputBytes = 4u * 1024u * 1024u;
constexpr std::size_t kMaxLineBytes = 4096u;
constexpr std::size_t kMaxTokenBytes = 2048u;
constexpr std::size_t kMaxTokensPerLine = 128u;
constexpr std::size_t kMaxDirectives = 65536u;
constexpr std::size_t kMaxSubmaterials = 4096u;

void SetError(std::string* error, const std::string& message) {
    if (error) *error = message;
}

bool Fail(std::string* error, const std::string& message) {
    SetError(error, message);
    return false;
}

bool IsWhitespace(char value) {
    return value == ' ' || value == '\t' || value == '\v' || value == '\f';
}

bool Tokenize(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear();
    std::size_t cursor = 0;
    while (cursor < line.size()) {
        while (cursor < line.size() && IsWhitespace(line[cursor])) ++cursor;
        if (cursor == line.size()) break;
        const std::size_t start = cursor;
        while (cursor < line.size() && !IsWhitespace(line[cursor])) ++cursor;
        const std::size_t tokenBytes = cursor - start;
        if (tokenBytes > kMaxTokenBytes || tokens.size() == kMaxTokensPerLine) return false;
        tokens.emplace_back(line.data() + start, tokenBytes);
    }
    return true;
}

bool IsFiniteNumericToken(const std::string& token) {
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
    if (cursor != token.size()) return false;

    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    return end == token.c_str() + token.size() && errno != ERANGE && std::isfinite(value);
}

bool ValidateShape(const std::string& name, const std::vector<std::string>& args,
                   MtlDirectiveKind& kind) {
    std::size_t numericCount = 0;
    if (name == "$TEXTURE") {
        if (args.size() != 2 || !IsFiniteNumericToken(args[0])) return false;
        kind = MtlDirectiveKind::Texture;
        return true;
    }
    if (name == "$DIFFUSECOLOR") {
        kind = MtlDirectiveKind::DiffuseColor;
        numericCount = 4;
    } else if (name == "$SPECULARCOLOR") {
        kind = MtlDirectiveKind::SpecularColor;
        numericCount = 4;
    } else if (name == "$AMBIENTCOLOR") {
        kind = MtlDirectiveKind::AmbientColor;
        numericCount = 4;
    } else if (name == "$SPECULARPOWER") {
        kind = MtlDirectiveKind::SpecularPower;
        numericCount = 1;
    } else {
        kind = MtlDirectiveKind::Unknown;
        return true;
    }
    if (args.size() != numericCount) return false;
    for (const auto& argument : args) {
        if (!IsFiniteNumericToken(argument)) return false;
    }
    return true;
}

} // namespace

bool ParseMtlSyntax(const std::string& text, MtlDocument& output, std::string* error) {
    if (text.size() > kMaxInputBytes)
        return Fail(error, "MTL syntax input exceeds the 4 MiB limit");
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xefu &&
        static_cast<unsigned char>(text[1]) == 0xbbu &&
        static_cast<unsigned char>(text[2]) == 0xbfu)
        return Fail(error, "UTF-8 BOM is outside the observed MTL syntax profile");
    if (text.find('\0') != std::string::npos)
        return Fail(error, "MTL syntax input contains an embedded NUL byte");

    MtlDocument candidate;
    bool ended = false;
    std::size_t directiveCount = 0;
    std::size_t lineNumber = 0;
    std::istringstream input(text);
    std::string line;
    std::vector<std::string> tokens;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > kMaxLineBytes)
            return Fail(error, "MTL line " + std::to_string(lineNumber) + " exceeds the 4096-byte limit");
        if (!Tokenize(line, tokens))
            return Fail(error, "MTL line " + std::to_string(lineNumber) + " exceeds token limits");
        if (tokens.empty()) continue;
        if (ended)
            return Fail(error, "non-blank content follows $END at line " + std::to_string(lineNumber));
        if (++directiveCount > kMaxDirectives)
            return Fail(error, "MTL syntax exceeds the 65536-directive limit");

        const std::string& name = tokens.front();
        if (name.empty() || name.front() != '$' || name.size() == 1)
            return Fail(error, "expected a dollar-prefixed directive at line " + std::to_string(lineNumber));
        std::vector<std::string> arguments(tokens.begin() + 1, tokens.end());

        if (name == "$SUBMATERIAL") {
            if (arguments.size() != 1)
                return Fail(error, "malformed $SUBMATERIAL at line " + std::to_string(lineNumber));
            if (candidate.submaterials.size() == kMaxSubmaterials)
                return Fail(error, "MTL syntax exceeds the 4096-submaterial limit");
            MtlSubmaterial section;
            section.nameToken = std::move(arguments.front());
            section.line = lineNumber;
            candidate.submaterials.push_back(std::move(section));
            continue;
        }

        if (name == "$END") {
            if (!arguments.empty())
                return Fail(error, "$END must not have arguments at line " + std::to_string(lineNumber));
            if (candidate.submaterials.empty())
                return Fail(error, "$END appears before any $SUBMATERIAL section");
            candidate.terminated = true;
            candidate.endLine = lineNumber;
            ended = true;
            continue;
        }

        if (candidate.submaterials.empty())
            return Fail(error, "directive appears before the first $SUBMATERIAL at line " +
                               std::to_string(lineNumber));
        MtlDirective directive;
        directive.name = name;
        directive.arguments = std::move(arguments);
        directive.line = lineNumber;
        if (!ValidateShape(directive.name, directive.arguments, directive.kind))
            return Fail(error, "malformed " + name + " directive at line " + std::to_string(lineNumber));
        candidate.submaterials.back().directives.push_back(std::move(directive));
    }

    if (!candidate.terminated)
        return Fail(error, "MTL syntax has no terminal $END directive");
    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

bool ApplyMtlDirectoryToRelativeTextures(MtlDocument& document, const std::string& mtlDirectory,
                                         std::size_t* converted, std::string* error) {
    if (mtlDirectory.empty() || mtlDirectory.size() > kMaxTokenBytes ||
        mtlDirectory.find('\0') != std::string::npos)
        return Fail(error, "MTL directory must be non-empty, NUL-free and at most 2048 bytes");

    MtlDocument candidate = document;
    std::size_t count = 0;
    for (auto& section : candidate.submaterials) {
        for (auto& directive : section.directives) {
            if (directive.name != "$TEXTURE_MTL" || directive.arguments.size() != 2 ||
                !IsFiniteNumericToken(directive.arguments[0]))
                continue;
            std::string combined = mtlDirectory + "/" + directive.arguments[1];
            if (combined.size() > kMaxTokenBytes)
                return Fail(error, "combined $TEXTURE_MTL path exceeds the 2048-byte token limit");
            directive.name = "$TEXTURE";
            directive.kind = MtlDirectiveKind::Texture;
            directive.arguments[1] = std::move(combined);
            ++count;
        }
    }
    document = std::move(candidate);
    if (converted) *converted = count;
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
