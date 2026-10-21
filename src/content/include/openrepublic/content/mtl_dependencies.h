#pragma once

#include "openrepublic/content/mtl_syntax.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace openrepublic::content {

// These are OpenRepublic resolver outcomes. They do not describe WRSR path
// semantics, texture-slot meanings, or whether an asset is loadable.
enum class MtlDependencyStatus {
    Resolved,
    Missing,
    Ambiguous,
    RejectedPath,
    InvalidRoot,
    FileSystemError,
};

enum class MtlDependencyRootStatus {
    Valid,
    InvalidRoot,
    FileSystemError,
};

enum class MtlDependencyManifestError {
    None,
    InvalidDocument,
    LimitsExceeded,
};

struct MtlTextureDependency {
    // The section order is the index in MtlDocument::submaterials.
    std::size_t submaterialIndex{};
    std::string submaterialNameToken;
    std::size_t sourceLine{};
    // Both source tokens are opaque. No texture-slot meaning is assigned.
    std::string textureIndexToken;
    std::string rawPathToken;
    MtlDependencyStatus status{MtlDependencyStatus::InvalidRoot};
    // Populated only for Resolved. This is an advisory path that passed a
    // containment check at resolution time, not a capability or security
    // boundary for a later open. The filesystem may change after this check;
    // any future reader must securely open beneath a trusted root and validate
    // the opened file/handle again. Do not assume a WRSR path contract.
    std::filesystem::path resolvedPath;
};

struct MtlDependencyRoot {
    MtlDependencyRootStatus status{MtlDependencyRootStatus::InvalidRoot};
    // Set only when status is Valid. The manifest does not echo invalid inputs.
    std::filesystem::path canonicalPath;
};

struct MtlDependencyManifest {
    std::vector<MtlDependencyRoot> roots;
    std::vector<MtlTextureDependency> textures;
};

// Bounds are applied even if a caller constructs an MtlDocument without using
// ParseMtlSyntax. Relative roots are rejected so resolution does not depend on
// process working directory. The function only inspects paths and never creates,
// changes, deletes, or opens texture files. Multiple roots containing the same
// relative file produce Ambiguous; it never chooses the first root.
constexpr std::size_t MaxMtlDependencyRoots = 32;
constexpr std::size_t MaxMtlDependencySections = 4096;
constexpr std::size_t MaxMtlDependencyDirectives = 65536;
constexpr std::size_t MaxMtlDependencyTokenBytes = 2048;
// Bounds the aggregate number of texture-path/root candidate inspections in
// one manifest build. Before candidate inspection, the builder counts safe
// texture references and unique valid canonical roots, then rejects when their
// product would exceed this budget. This permits up to 8192 safe texture
// references across 32 roots; invalid paths and duplicate roots do not count.
constexpr std::size_t MaxMtlDependencyCandidateProbes = 262144;

// Returns None and replaces output with a manifest complete only for recognized
// `$TEXTURE` directives (name and parsed kind both match). Unknown texture-like
// directives such as `$TEXTURE_MTL` are outside this manifest and are not
// resolved. Invalid document structure or exceeded bounds return an error and
// leave output unchanged. Invalid roots and filesystem failures are represented
// in the resulting root/texture statuses. Diagnostics are generic and never
// include source texture paths or file contents. Containment reflects the
// filesystem at resolution time only and does not make subsequent file reads
// safe; a future reader must validate/open securely beneath its trusted root.
MtlDependencyManifestError BuildMtlDependencyManifest(
    const MtlDocument& document,
    const std::vector<std::filesystem::path>& roots,
    MtlDependencyManifest& output,
    std::string* error = nullptr);

} // namespace openrepublic::content
