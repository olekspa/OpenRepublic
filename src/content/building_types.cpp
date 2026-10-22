#include "openrepublic/content/building_types.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kMaxLineBytes = 4096;
constexpr std::size_t kMaxTokenBytes = 2048;
constexpr std::size_t kMaxTokensPerLine = 128;
constexpr std::size_t kMaxTotalDirectives = 262144;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
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
        if (cursor - start > kMaxTokenBytes || tokens.size() == kMaxTokensPerLine) return false;
        tokens.emplace_back(line.data() + start, cursor - start);
    }
    return true;
}

bool ParseFinite(const std::string& token, double& value) {
    if (token.empty()) return false;
    errno = 0;
    char* end = nullptr;
    value = std::strtod(token.c_str(), &end);
    return end == token.c_str() + token.size() && errno != ERANGE && std::isfinite(value);
}

bool IsUnsafeRelativeToken(const std::string& raw) {
    if (raw.empty() || raw.find('\0') != std::string::npos || raw.find(':') != std::string::npos ||
        raw.front() == '/' || raw.front() == '\\')
        return true;
    std::size_t componentStart = 0;
    for (std::size_t i = 0; i <= raw.size(); ++i) {
        if (i != raw.size() && raw[i] != '/' && raw[i] != '\\') continue;
        if (raw.compare(componentStart, i - componentStart, "..") == 0) return true;
        componentStart = i + 1;
    }
    return false;
}

bool WithinRoot(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto rootIt = root.begin();
    auto pathIt = path.begin();
    for (; rootIt != root.end(); ++rootIt, ++pathIt) {
        if (pathIt == path.end() || *rootIt != *pathIt) return false;
    }
    return true;
}

bool ResolveOne(const std::string& token, const std::filesystem::path& canonicalRoot,
                std::filesystem::path& output, std::string* error, const char* label) {
    if (IsUnsafeRelativeToken(token))
        return Fail(error, std::string(label) + " path is not a safe content-relative path");
    std::string portable = token;
    std::replace(portable.begin(), portable.end(), '\\', '/');
    std::error_code ec;
    const auto candidate = canonicalRoot / std::filesystem::path(portable);
    if (!std::filesystem::is_regular_file(candidate, ec) || ec)
        return Fail(error, std::string(label) + " file was not found under the content root");
    const auto canonical = std::filesystem::canonical(candidate, ec);
    if (ec || !WithinRoot(canonical, canonicalRoot))
        return Fail(error, std::string(label) + " file resolves outside the content root");
    output = canonical;
    return true;
}

} // namespace

bool ParseBuildingTypes(const std::string& text, BuildingTypesDocument& output, std::string* error) {
    if (text.size() > MaxBuildingTypesInputBytes)
        return Fail(error, "building-type input exceeds the 8 MiB limit");
    if (text.find('\0') != std::string::npos)
        return Fail(error, "building-type input contains an embedded NUL byte");

    BuildingTypesDocument candidate;
    std::set<std::string> names;
    BuildingTypeDefinition* open = nullptr;
    std::set<std::string> seenSingles;
    std::size_t totalDirectives = 0;
    std::size_t lineNumber = 0;
    std::istringstream input(text);
    std::string line;
    std::vector<std::string> tokens;
    const auto where = [&]() { return " at line " + std::to_string(lineNumber); };

    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > kMaxLineBytes) return Fail(error, "line exceeds the 4096-byte limit" + where());
        if (!Tokenize(line, tokens)) return Fail(error, "line exceeds token limits" + where());
        if (tokens.empty()) continue;
        if (tokens.front().rfind("--", 0) == 0) {
            ++candidate.commentLikeLines;
            continue;
        }
        if (candidate.terminated) return Fail(error, "content follows $END" + where());
        if (++totalDirectives > kMaxTotalDirectives)
            return Fail(error, "building-type input exceeds the directive limit");

        const std::string& name = tokens.front();
        const std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        if (name == "$TYPE") {
            if (open) return Fail(error, "$TYPE inside an unterminated block" + where());
            if (args.size() != 1) return Fail(error, "malformed $TYPE" + where());
            if (candidate.types.size() == MaxBuildingTypes)
                return Fail(error, "building-type count exceeds the 8192 limit");
            if (!names.insert(args[0]).second)
                return Fail(error, "duplicate building type name" + where());
            BuildingTypeDefinition type;
            type.name = args[0];
            type.line = lineNumber;
            candidate.types.push_back(std::move(type));
            open = &candidate.types.back();
            seenSingles.clear();
            continue;
        }
        if (name == "$END") {
            if (open) return Fail(error, "$END inside an unterminated block" + where());
            if (!args.empty()) return Fail(error, "$END must not have arguments" + where());
            candidate.terminated = true;
            continue;
        }
        if (!open) return Fail(error, "directive outside a $TYPE block" + where());
        if (name == "END") {
            if (!args.empty()) return Fail(error, "END must not have arguments" + where());
            open = nullptr;
            continue;
        }
        if (open->otherDirectives.size() >= MaxBuildingTypeDirectives)
            return Fail(error, "building type exceeds its directive limit" + where());

        const bool single = name == "MODEL" || name == "MODEL_LOD" || name == "MODEL_LOD2" ||
                            name == "MATERIAL" || name == "MODELEMISSIVE" ||
                            name == "MATERIALEMISSIVE";
        if (!single) {
            open->otherDirectives.push_back({name, args, lineNumber});
            continue;
        }
        if (!seenSingles.insert(name).second)
            return Fail(error, "repeated " + name + " directive" + where());
        const bool lodLike = name == "MODEL_LOD" || name == "MODEL_LOD2";
        if (lodLike ? (args.empty() || args.size() > 2) : args.size() != 1)
            return Fail(error, "wrong argument count for " + name + where());
        if (lodLike) {
            BuildingTypeModelReference reference;
            reference.path = args[0];
            if (args.size() == 2) {
                double number = 0;
                if (!ParseFinite(args[1], number))
                    return Fail(error, "non-finite trailing number for " + name + where());
                reference.trailingNumber = number;
            }
            reference.loaderNumber = reference.trailingNumber
                ? *reference.trailingNumber
                : (name == "MODEL_LOD" ? 1000.0 : 2000.0);
            (name == "MODEL_LOD" ? open->lod1 : open->lod2) = std::move(reference);
        } else if (name == "MODEL") {
            open->model = args[0];
        } else if (name == "MATERIAL") {
            open->material = args[0];
        } else if (name == "MODELEMISSIVE") {
            open->emissiveModel = args[0];
        } else {
            open->emissiveMaterial = args[0];
        }
    }
    if (open) return Fail(error, "building type is missing its END line");

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

const BuildingTypeDefinition* FindBuildingType(const BuildingTypesDocument& document,
                                               const std::string& name) {
    for (const auto& type : document.types) {
        if (type.name == name) return &type;
    }
    return nullptr;
}

bool ResolveBuildingTypeAssets(const BuildingTypeDefinition& type, int lod,
                               const std::filesystem::path& contentRoot,
                               BuildingTypeAssetPaths& output, std::string* error) {
    if (lod < 0 || lod > 2) return Fail(error, "model level must be 0, 1 or 2");
    if (!contentRoot.is_absolute()) return Fail(error, "content root must be absolute");
    std::error_code ec;
    const auto canonicalRoot = std::filesystem::canonical(contentRoot, ec);
    if (ec || !std::filesystem::is_directory(canonicalRoot, ec) || ec)
        return Fail(error, "content root is not an existing directory");

    std::string modelToken;
    if (lod == 0) {
        modelToken = type.model;
    } else if (const auto& reference = lod == 1 ? type.lod1 : type.lod2) {
        modelToken = reference->path;
    } else {
        return Fail(error, "building type has no model for the requested level");
    }
    if (modelToken.empty()) return Fail(error, "building type has no model for the requested level");

    BuildingTypeAssetPaths candidate;
    if (!ResolveOne(modelToken, canonicalRoot, candidate.model, error, "model")) return false;
    if (!type.material.empty() &&
        !ResolveOne(type.material, canonicalRoot, candidate.material, error, "material"))
        return false;

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
