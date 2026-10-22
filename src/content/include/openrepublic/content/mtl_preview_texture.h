#pragma once

#include "openrepublic/content/mtl_dependencies.h"

#include <filesystem>
#include <string>
#include <vector>

namespace openrepublic::content {

// This is a deliberately narrow OpenRepublic preview adapter. It selects one
// unique integer `$TEXTURE 0` reference per MTL section. Treating that source
// slot as a base-color image on UV set 0 is an experiment, not a recovered
// WRSR material contract. Other texture slots remain unmapped.
enum class MtlPreviewBaseColorTextureStatus {
    NoSlotZero,
    Resolved,
    Unresolved,
    Ambiguous,
    InvalidSlotIndex,
};

struct MtlPreviewBaseColorTexture {
    MtlPreviewBaseColorTextureStatus status{MtlPreviewBaseColorTextureStatus::NoSlotZero};
    std::filesystem::path resolvedPath;
};

// Builds one preview candidate per MTL section from a dependency manifest
// produced for the same document. Only a unique integer slot-0 reference that
// resolved beneath exactly one caller-supplied root returns a path. On an
// invalid or mismatched manifest, output is unchanged.
bool BuildMtlPreviewBaseColorTextures(
    const MtlDocument& document,
    const MtlDependencyManifest& dependencies,
    std::vector<MtlPreviewBaseColorTexture>& output,
    std::string* error = nullptr);

} // namespace openrepublic::content
