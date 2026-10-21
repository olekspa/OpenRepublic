#include "openrepublic/content/mtl_dependencies.h"

#include <algorithm>
#include <limits>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace openrepublic::content {
namespace {

namespace fs = std::filesystem;

struct RootInfo {
    MtlDependencyRootStatus status{MtlDependencyRootStatus::InvalidRoot};
    fs::path canonicalPath;
};

struct CandidateInfo {
    MtlDependencyStatus status{MtlDependencyStatus::Missing};
    fs::path canonicalPath;
};

bool HasNul(const std::string& value) {
    return value.find('\0') != std::string::npos;
}

bool IsDrivePrefix(const std::string& value) {
    return value.size() >= 2 &&
        ((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= 'a' && value[0] <= 'z')) &&
        value[1] == ':';
}

bool IsUnsafeRelativePath(const std::string& raw, fs::path& normalized) {
    if (raw.empty() || HasNul(raw) || raw.find(':') != std::string::npos ||
        raw.front() == '/' || raw.front() == '\\' || IsDrivePrefix(raw))
        return true;

    // Treat both separators as separators on every platform. This catches
    // Windows traversal and UNC spellings even in a POSIX-side analysis build.
    std::size_t componentStart = 0;
    for (std::size_t i = 0; i <= raw.size(); ++i) {
        if (i != raw.size() && raw[i] != '/' && raw[i] != '\\') continue;
        if (raw.compare(componentStart, i - componentStart, "..") == 0) return true;
        componentStart = i + 1;
    }

    std::string portable = raw;
    std::replace(portable.begin(), portable.end(), '\\', '/');
    normalized = fs::path(portable);
    return normalized.empty() || normalized.is_absolute() || normalized.has_root_name() ||
        normalized.has_root_directory();
}

#ifdef _WIN32
bool PathComponentEqual(const fs::path& left, const fs::path& right) {
    const auto& leftNative = left.native();
    const auto& rightNative = right.native();
    if (leftNative.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        rightNative.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    return ::CompareStringOrdinal(leftNative.data(), static_cast<int>(leftNative.size()),
        rightNative.data(), static_cast<int>(rightNative.size()), TRUE) == CSTR_EQUAL;
}
#else
bool PathComponentEqual(const fs::path& left, const fs::path& right) {
    return left.native() == right.native();
}
#endif

bool IsWithin(const fs::path& candidate, const fs::path& root) {
    auto rootIt = root.begin();
    auto candidateIt = candidate.begin();
    for (; rootIt != root.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end() || !PathComponentEqual(*rootIt, *candidateIt))
            return false;
    }
    return true;
}

bool PathsEqual(const fs::path& left, const fs::path& right) {
    auto leftIt = left.begin();
    auto rightIt = right.begin();
    for (; leftIt != left.end() && rightIt != right.end(); ++leftIt, ++rightIt) {
        if (!PathComponentEqual(*leftIt, *rightIt)) return false;
    }
    return leftIt == left.end() && rightIt == right.end();
}

RootInfo CanonicalizeRoot(const fs::path& root) {
    RootInfo result;
    if (!root.is_absolute()) {
        result.status = MtlDependencyRootStatus::InvalidRoot;
        return result;
    }

    std::error_code error;
    const auto status = fs::status(root, error);
    if (error) {
        result.status = error == std::errc::no_such_file_or_directory ||
            error == std::errc::not_a_directory
            ? MtlDependencyRootStatus::InvalidRoot
            : MtlDependencyRootStatus::FileSystemError;
        return result;
    }
    if (!fs::is_directory(status)) {
        result.status = MtlDependencyRootStatus::InvalidRoot;
        return result;
    }

    result.canonicalPath = fs::canonical(root, error);
    if (error) {
        result.status = MtlDependencyRootStatus::FileSystemError;
        result.canonicalPath.clear();
        return result;
    }
    result.status = MtlDependencyRootStatus::Valid;
    return result;
}

CandidateInfo InspectCandidate(const fs::path& canonicalRoot, const fs::path& relative) {
    CandidateInfo result;
    std::error_code error;
    const auto candidate = canonicalRoot / relative;

    // weakly_canonical resolves every existing prefix, including directory
    // symlinks, while still allowing us to classify a missing final leaf.
    auto weakPath = fs::weakly_canonical(candidate, error);
    if (error) {
        result.status = MtlDependencyStatus::FileSystemError;
        return result;
    }
    if (!IsWithin(weakPath, canonicalRoot)) {
        result.status = MtlDependencyStatus::RejectedPath;
        return result;
    }

    const auto status = fs::status(candidate, error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) {
            result.status = MtlDependencyStatus::Missing;
            return result;
        }
        result.status = MtlDependencyStatus::FileSystemError;
        return result;
    }
    if (!fs::exists(status) || !fs::is_regular_file(status)) {
        result.status = MtlDependencyStatus::Missing;
        return result;
    }

    result.canonicalPath = fs::canonical(candidate, error);
    if (error) {
        result.status = MtlDependencyStatus::FileSystemError;
        result.canonicalPath.clear();
        return result;
    }
    if (!IsWithin(result.canonicalPath, canonicalRoot)) {
        result.status = MtlDependencyStatus::RejectedPath;
        result.canonicalPath.clear();
        return result;
    }
    result.status = MtlDependencyStatus::Resolved;
    return result;
}

bool ValidateDocument(const MtlDocument& document, std::size_t& textureCount,
    MtlDependencyManifestError& failure) {
    textureCount = 0;
    if (!document.terminated || document.submaterials.empty()) {
        failure = MtlDependencyManifestError::InvalidDocument;
        return false;
    }
    if (document.submaterials.size() > MaxMtlDependencySections) {
        failure = MtlDependencyManifestError::LimitsExceeded;
        return false;
    }

    std::size_t directiveCount = 0;
    for (const auto& section : document.submaterials) {
        if (section.nameToken.empty() || section.nameToken.size() > MaxMtlDependencyTokenBytes ||
            HasNul(section.nameToken)) {
            failure = MtlDependencyManifestError::InvalidDocument;
            return false;
        }
        for (const auto& directive : section.directives) {
            if (++directiveCount > MaxMtlDependencyDirectives) {
                failure = MtlDependencyManifestError::LimitsExceeded;
                return false;
            }
            if (directive.name != "$TEXTURE" && directive.kind != MtlDirectiveKind::Texture)
                continue;
            if (directive.kind != MtlDirectiveKind::Texture || directive.name != "$TEXTURE" ||
                directive.arguments.size() != 2 || directive.arguments[0].empty() ||
                directive.arguments[0].size() > MaxMtlDependencyTokenBytes ||
                directive.arguments[1].size() > MaxMtlDependencyTokenBytes ||
                HasNul(directive.arguments[0])) {
                failure = MtlDependencyManifestError::InvalidDocument;
                return false;
            }
            if (++textureCount > MaxMtlDependencyDirectives) {
                failure = MtlDependencyManifestError::LimitsExceeded;
                return false;
            }
        }
    }
    failure = MtlDependencyManifestError::None;
    return true;
}

} // namespace

MtlDependencyManifestError BuildMtlDependencyManifest(
    const MtlDocument& document,
    const std::vector<fs::path>& roots,
    MtlDependencyManifest& output,
    std::string* error) {
    if (roots.size() > MaxMtlDependencyRoots) {
        if (error) *error = "dependency root count exceeds the configured limit";
        return MtlDependencyManifestError::LimitsExceeded;
    }

    std::size_t textureCount = 0;
    MtlDependencyManifestError validationError = MtlDependencyManifestError::None;
    if (!ValidateDocument(document, textureCount, validationError)) {
        if (error) *error = validationError == MtlDependencyManifestError::LimitsExceeded
            ? "MTL dependency manifest exceeds a configured bound"
            : "MTL dependency manifest input is not a valid bounded MTL document";
        return validationError;
    }

    MtlDependencyManifest candidate;
    candidate.roots.reserve(roots.size());
    std::vector<fs::path> validUniqueRoots;
    bool hasInvalidRoot = roots.empty();
    bool hasRootFileSystemError = false;
    for (const auto& root : roots) {
        const auto info = CanonicalizeRoot(root);
        candidate.roots.push_back({info.status, info.canonicalPath});
        if (info.status == MtlDependencyRootStatus::InvalidRoot) {
            hasInvalidRoot = true;
            continue;
        }
        if (info.status == MtlDependencyRootStatus::FileSystemError) {
            hasRootFileSystemError = true;
            continue;
        }
        const auto duplicate = std::find_if(validUniqueRoots.begin(), validUniqueRoots.end(),
            [&info](const fs::path& existing) { return PathsEqual(existing, info.canonicalPath); });
        if (duplicate == validUniqueRoots.end()) {
            validUniqueRoots.push_back(info.canonicalPath);
        }
    }

    // Preflight the worst-case candidate inspections before touching any
    // per-texture candidate paths. Invalid paths are not inspected, duplicate
    // canonical roots count only once, and root-error outcomes skip candidate
    // resolution entirely. Division avoids overflow in safeTextureCount * roots.
    if (!hasInvalidRoot && !hasRootFileSystemError && !validUniqueRoots.empty()) {
        std::size_t safeTextureCount = 0;
        for (const auto& section : document.submaterials) {
            for (const auto& directive : section.directives) {
                if (directive.kind != MtlDirectiveKind::Texture || directive.name != "$TEXTURE")
                    continue;
                fs::path ignored;
                if (!IsUnsafeRelativePath(directive.arguments[1], ignored)) ++safeTextureCount;
            }
        }
        if (safeTextureCount > MaxMtlDependencyCandidateProbes / validUniqueRoots.size()) {
            if (error) *error = "MTL dependency manifest exceeds the candidate inspection budget";
            return MtlDependencyManifestError::LimitsExceeded;
        }
    }

    candidate.textures.reserve(textureCount);
    for (std::size_t sectionIndex = 0; sectionIndex < document.submaterials.size(); ++sectionIndex) {
        const auto& section = document.submaterials[sectionIndex];
        for (const auto& directive : section.directives) {
            if (directive.kind != MtlDirectiveKind::Texture || directive.name != "$TEXTURE") continue;

            MtlTextureDependency dependency;
            dependency.submaterialIndex = sectionIndex;
            dependency.submaterialNameToken = section.nameToken;
            dependency.sourceLine = directive.line;
            dependency.textureIndexToken = directive.arguments[0];
            dependency.rawPathToken = directive.arguments[1];

            fs::path relative;
            if (IsUnsafeRelativePath(dependency.rawPathToken, relative)) {
                dependency.status = MtlDependencyStatus::RejectedPath;
                candidate.textures.push_back(std::move(dependency));
                continue;
            }
            if (hasRootFileSystemError) {
                dependency.status = MtlDependencyStatus::FileSystemError;
                candidate.textures.push_back(std::move(dependency));
                continue;
            }
            if (hasInvalidRoot) {
                dependency.status = MtlDependencyStatus::InvalidRoot;
                candidate.textures.push_back(std::move(dependency));
                continue;
            }

            std::vector<fs::path> matches;
            bool rejectedCandidate = false;
            bool candidateError = false;
            for (const auto& root : validUniqueRoots) {
                auto inspected = InspectCandidate(root, relative);
                if (inspected.status == MtlDependencyStatus::Resolved) {
                    matches.push_back(std::move(inspected.canonicalPath));
                } else if (inspected.status == MtlDependencyStatus::RejectedPath) {
                    rejectedCandidate = true;
                } else if (inspected.status == MtlDependencyStatus::FileSystemError) {
                    candidateError = true;
                }
            }
            if (candidateError) {
                dependency.status = MtlDependencyStatus::FileSystemError;
            } else if (rejectedCandidate) {
                dependency.status = MtlDependencyStatus::RejectedPath;
            } else if (matches.size() > 1) {
                dependency.status = MtlDependencyStatus::Ambiguous;
            } else if (matches.size() == 1) {
                dependency.status = MtlDependencyStatus::Resolved;
                dependency.resolvedPath = std::move(matches.front());
            } else {
                dependency.status = MtlDependencyStatus::Missing;
            }
            candidate.textures.push_back(std::move(dependency));
        }
    }

    output = std::move(candidate);
    if (error) error->clear();
    return MtlDependencyManifestError::None;
}

} // namespace openrepublic::content
