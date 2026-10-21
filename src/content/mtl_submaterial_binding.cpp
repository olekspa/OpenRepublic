#include "openrepublic/content/mtl_submaterial_binding.h"

#include <unordered_map>
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

struct SectionLookup {
    std::size_t index{};
    bool duplicate{};
};

} // namespace

bool BindMtlSubmaterialsByName(const std::vector<std::string>& nmfIdentifiers,
                               const MtlDocument& document,
                               std::vector<MtlSubmaterialBinding>& output,
                               std::string* error) {
    if (nmfIdentifiers.size() > MaxMtlSubmaterialBindingEntries ||
        document.submaterials.size() > MaxMtlSubmaterialBindingEntries)
        return Fail(error, "submaterial binding exceeds the 4096-entry limit");
    if (!document.terminated || document.submaterials.empty())
        return Fail(error, "submaterial binding requires a terminated MTL document with at least one section");

    std::unordered_map<std::string, SectionLookup> sectionsByName;
    sectionsByName.reserve(document.submaterials.size());
    for (std::size_t sectionIndex = 0; sectionIndex < document.submaterials.size(); ++sectionIndex) {
        const std::string& name = document.submaterials[sectionIndex].nameToken;
        if (name.empty() || name.size() > MaxMtlSubmaterialBindingTokenBytes)
            return Fail(error, "MTL submaterial identifier is empty or exceeds the 2048-byte limit");

        const auto inserted = sectionsByName.emplace(name, SectionLookup{sectionIndex, false});
        if (!inserted.second) inserted.first->second.duplicate = true;
    }

    for (const std::string& identifier : nmfIdentifiers) {
        if (identifier.empty() || identifier.size() > MaxMtlSubmaterialBindingTokenBytes)
            return Fail(error, "NMF submaterial identifier is empty or exceeds the 2048-byte limit");
    }

    std::vector<MtlSubmaterialBinding> candidate;
    candidate.reserve(nmfIdentifiers.size());
    for (const std::string& identifier : nmfIdentifiers) {
        const auto found = sectionsByName.find(identifier);
        if (found == sectionsByName.end()) {
            candidate.push_back({MtlSubmaterialBindingStatus::Missing, 0});
        } else if (found->second.duplicate) {
            candidate.push_back({MtlSubmaterialBindingStatus::Ambiguous, 0});
        } else {
            candidate.push_back({MtlSubmaterialBindingStatus::Matched, found->second.index});
        }
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
