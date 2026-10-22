#include "openrepublic/content/mtl_preview_texture.h"

#include <charconv>
#include <system_error>
#include <utility>

namespace openrepublic::content {
namespace {

void SetError(std::string* error, const char* message) {
    if (error) *error = message;
}

bool Fail(std::string* error, const char* message) {
    SetError(error, message);
    return false;
}

bool ParseTextureIndex(const std::string& token, int& value) {
    if (token.empty() || token.front() == '-') return false;
    const char* begin = token.data();
    const char* end = begin + token.size();
    if (*begin == '+') {
        ++begin;
        if (begin == end) return false;
    }
    for (const char* cursor = begin; cursor != end; ++cursor) {
        if (*cursor < '0' || *cursor > '9') return false;
    }
    int parsed = 0;
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) return false;
    value = parsed;
    return true;
}

bool IsResolvedPath(const MtlTextureDependency& dependency) {
    return dependency.status == MtlDependencyStatus::Resolved &&
        !dependency.resolvedPath.empty() && dependency.resolvedPath.is_absolute();
}

} // namespace

bool BuildMtlPreviewBaseColorTextures(
    const MtlDocument& document,
    const MtlDependencyManifest& dependencies,
    std::vector<MtlPreviewBaseColorTexture>& output,
    std::string* error) {
    if (!document.terminated || document.submaterials.empty())
        return Fail(error, "MTL preview texture input is not a terminated document");

    std::size_t expectedDependencyCount = 0;
    for (const auto& section : document.submaterials) {
        for (const auto& directive : section.directives) {
            if (directive.name == "$TEXTURE" && directive.kind == MtlDirectiveKind::Texture)
                ++expectedDependencyCount;
        }
    }
    if (dependencies.textures.size() != expectedDependencyCount)
        return Fail(error, "MTL preview texture manifest does not cover the source document");

    std::vector<MtlPreviewBaseColorTexture> candidate(document.submaterials.size());
    std::vector<std::size_t> slotZeroCounts(document.submaterials.size(), 0);
    std::vector<const MtlTextureDependency*> slotZeroDependencies(document.submaterials.size(), nullptr);
    std::vector<bool> invalidIndices(document.submaterials.size(), false);

    std::size_t dependencyIndex = 0;
    for (std::size_t sectionIndex = 0; sectionIndex < document.submaterials.size(); ++sectionIndex) {
        const auto& section = document.submaterials[sectionIndex];
        for (const auto& directive : section.directives) {
            if (directive.name != "$TEXTURE" || directive.kind != MtlDirectiveKind::Texture)
                continue;
            if (directive.arguments.size() != 2 || dependencyIndex >= dependencies.textures.size())
                return Fail(error, "MTL preview texture source shape does not match its manifest");

            const auto& dependency = dependencies.textures[dependencyIndex++];
            if (dependency.submaterialIndex != sectionIndex ||
                dependency.submaterialNameToken != section.nameToken ||
                dependency.sourceLine != directive.line ||
                dependency.textureIndexToken != directive.arguments[0] ||
                dependency.rawPathToken != directive.arguments[1])
                return Fail(error, "MTL preview texture manifest does not match source order");

            int textureIndex = -1;
            if (!ParseTextureIndex(dependency.textureIndexToken, textureIndex)) {
                invalidIndices[sectionIndex] = true;
                continue;
            }
            if (textureIndex != 0) continue;

            ++slotZeroCounts[sectionIndex];
            slotZeroDependencies[sectionIndex] = &dependency;
        }
    }
    if (dependencyIndex != dependencies.textures.size())
        return Fail(error, "MTL preview texture manifest contains extra entries");

    for (std::size_t sectionIndex = 0; sectionIndex < candidate.size(); ++sectionIndex) {
        if (slotZeroCounts[sectionIndex] > 1 ||
            (slotZeroCounts[sectionIndex] == 1 && invalidIndices[sectionIndex])) {
            candidate[sectionIndex].status = MtlPreviewBaseColorTextureStatus::Ambiguous;
            continue;
        }
        if (slotZeroCounts[sectionIndex] == 1) {
            const auto* dependency = slotZeroDependencies[sectionIndex];
            if (dependency != nullptr && IsResolvedPath(*dependency)) {
                candidate[sectionIndex].status = MtlPreviewBaseColorTextureStatus::Resolved;
                candidate[sectionIndex].resolvedPath = dependency->resolvedPath;
            } else {
                candidate[sectionIndex].status = MtlPreviewBaseColorTextureStatus::Unresolved;
            }
            continue;
        }
        if (invalidIndices[sectionIndex])
            candidate[sectionIndex].status = MtlPreviewBaseColorTextureStatus::InvalidSlotIndex;
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
