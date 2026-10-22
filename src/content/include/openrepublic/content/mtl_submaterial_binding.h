#pragma once

#include "openrepublic/content/mtl_syntax.h"

#include <cstddef>
#include <string>
#include <vector>

namespace openrepublic::content {

enum class MtlSubmaterialBindingStatus {
    Matched,
    Missing,
    Ambiguous,
    // Produced only by SelectMtlSubmaterialsLikeLoader: no exact name matched
    // and the loader's default (first) section was selected.
    FallbackFirstSection,
};

struct MtlSubmaterialBinding {
    MtlSubmaterialBindingStatus status{MtlSubmaterialBindingStatus::Missing};
    // Valid only when status is Matched. This is an MTL section order index,
    // not a texture slot or an NMF table index.
    std::size_t sectionIndex{};
};

constexpr std::size_t MaxMtlSubmaterialBindingEntries = 4096;
constexpr std::size_t MaxMtlSubmaterialBindingTokenBytes = 2048;

// Matches each NMF table identifier to MTL section nameToken values using
// exact byte equality. Section order is irrelevant. Missing and duplicate MTL
// names are returned explicitly in the candidate result. Caller-constructed
// inputs are bounded and malformed/over-limit inputs leave output unchanged;
// the MTL document must be terminated and contain at least one section.
bool BindMtlSubmaterialsByName(const std::vector<std::string>& nmfIdentifiers,
                               const MtlDocument& document,
                               std::vector<MtlSubmaterialBinding>& output,
                               std::string* error = nullptr);

// Reproduces C3D_MATERIAL::SelectSubMaterial(const char*) as statically read in
// C3DDLL32.dll shipped with WRSR build 23935965 (VERIFIED static review; not
// re-checked on other builds): the first section is the default selection; a
// document with exactly one section always selects it regardless of name;
// otherwise the first section whose name is byte-equal wins (duplicates are not
// ambiguous), and an unmatched name keeps the first section. The mesh renderer
// passes each subset's NMF table identifier to that call. Statuses are Matched
// (single section or exact name) or FallbackFirstSection; sectionIndex is
// always valid. Inputs are bounded as in BindMtlSubmaterialsByName and failure
// leaves output unchanged.
bool SelectMtlSubmaterialsLikeLoader(const std::vector<std::string>& nmfIdentifiers,
                                     const MtlDocument& document,
                                     std::vector<MtlSubmaterialBinding>& output,
                                     std::string* error = nullptr);

} // namespace openrepublic::content
