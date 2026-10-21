#pragma once

#include "openrepublic/content/mtl_syntax.h"

#include <array>

namespace openrepublic::content {

enum class MtlPreviewDiffuseColorStatus {
    Absent,
    Valid,
    Invalid,
};

struct MtlPreviewDiffuseColor {
    MtlPreviewDiffuseColorStatus status{MtlPreviewDiffuseColorStatus::Absent};
    std::array<float, 4> value{};
};

// Extracts the last directive whose name and parsed kind both identify
// $DIFFUSECOLOR. This is a preview conversion only; parsed MTL tokens remain
// unchanged. A matching directive that cannot be represented as four finite
// floats produces Invalid rather than falling back to an earlier value.
MtlPreviewDiffuseColor ExtractMtlDiffuseColorForPreview(const MtlSubmaterial& submaterial);

} // namespace openrepublic::content
