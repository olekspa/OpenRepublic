#pragma once

#include "openrepublic/content/glb_container.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace openrepublic::content {

// A deliberately narrow, embedded-resource glTF 2.0 profile used before the
// developer-only Wicked Editor importer. This is structural validation for
// trusted local inputs, not a general glTF reader or hostile-file sandbox.
struct GlbSemanticSummary {
    std::size_t meshCount = 0;
    std::size_t primitiveCount = 0;
    std::size_t accessorCount = 0;
    std::size_t bufferViewCount = 0;
    std::size_t materialCount = 0;
    std::size_t textureCount = 0;
    std::size_t imageCount = 0;
};

enum class GlbSemanticErrorCode {
    None,
    InvalidContainerInfo,
    JsonTooLarge,
    InvalidJson,
    InvalidDocument,
    UnsupportedFeature,
    InvalidReference,
    InvalidBinaryLayout,
    InvalidGeometry,
    LimitsExceeded,
};

struct GlbSemanticError {
    GlbSemanticErrorCode code = GlbSemanticErrorCode::None;
    std::string message;
};

inline constexpr std::size_t kMaxGlbSemanticJsonBytes = 16u * 1024u * 1024u;
inline constexpr std::size_t kMaxGlbSemanticMeshes = 4096;
inline constexpr std::size_t kMaxGlbSemanticPrimitives = 16384;
inline constexpr std::size_t kMaxGlbSemanticArrayEntries = 65536;
inline constexpr std::size_t kMaxGlbSemanticAccessorElements = 4u * 1024u * 1024u;
inline constexpr std::size_t kMaxGlbSemanticValueScans = 16u * 1024u * 1024u;

// Validates the JSON and embedded BIN relationship described by a previously
// checked GLB v2 container. On failure, summary is left unchanged and error is
// populated. Only one embedded buffer, triangle primitives, dense accessors,
// buffer-view PNG/JPEG images, scalar/vector accessors, finite FLOAT geometry,
// and unsigned SCALAR indices are accepted. Aggregate finite-float and index
// validation is capped by kMaxGlbSemanticValueScans scalar reads. Unknown glTF
// extensions and URI-backed resources are rejected. This verifies image
// references and MIME declarations but does not decode image pixels or establish
// material fidelity.
bool ValidateEmbeddedGlbV2(const std::vector<std::uint8_t>& bytes,
                           const GlbContainerInfo& container,
                           GlbSemanticSummary& summary,
                           GlbSemanticError& error);

} // namespace openrepublic::content
