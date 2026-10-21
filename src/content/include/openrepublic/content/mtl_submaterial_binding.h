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

} // namespace openrepublic::content
