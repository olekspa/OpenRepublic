#include "openrepublic/content/mtl_submaterial_binding.h"

#include <iostream>
#include <string>
#include <vector>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void TestReorderedSectionsMatchByExactName() {
    const std::vector<std::string> nmfIdentifiers{"wall", "window"};
    MtlDocument document;
    document.submaterials = {
        MtlSubmaterial{"window", {}, 1},
        MtlSubmaterial{"wall", {}, 2},
    };
    document.terminated = true;

    std::vector<MtlSubmaterialBinding> bindings;
    std::string error = "stale";
    Check(BindMtlSubmaterialsByName(nmfIdentifiers, document, bindings, &error),
          "reordered identifiers bind successfully");
    Check(error.empty(), "successful binding clears optional diagnostic");
    Check(bindings.size() == 2, "one result is returned per NMF table entry");
    if (bindings.size() == 2) {
        Check(bindings[0].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[0].sectionIndex == 1,
              "wall maps to its reordered MTL section");
        Check(bindings[1].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[1].sectionIndex == 0,
              "window maps to its reordered MTL section");
    }
}

void TestMissingAndAmbiguousSections() {
    const std::vector<std::string> nmfIdentifiers{"wall", "window", "roof"};
    MtlDocument document;
    document.submaterials = {
        MtlSubmaterial{"window", {}, 1},
        MtlSubmaterial{"wall", {}, 2},
        MtlSubmaterial{"wall", {}, 3},
    };
    document.terminated = true;

    std::vector<MtlSubmaterialBinding> bindings;
    Check(BindMtlSubmaterialsByName(nmfIdentifiers, document, bindings),
          "missing and duplicate sections are represented as results");
    Check(bindings.size() == 3, "missing and duplicate results retain NMF table order");
    if (bindings.size() == 3) {
        Check(bindings[0].status == MtlSubmaterialBindingStatus::Ambiguous,
              "duplicate MTL names are reported as ambiguous");
        Check(bindings[1].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[1].sectionIndex == 0,
              "unique section remains resolvable when another name is duplicated");
        Check(bindings[2].status == MtlSubmaterialBindingStatus::Missing,
              "unrepresented NMF name is reported as missing");
    }
}

void TestDuplicateNmfIdentifiersAndExactCase() {
    const std::vector<std::string> nmfIdentifiers{"wall", "wall", "Wall"};
    MtlDocument document;
    document.submaterials = {MtlSubmaterial{"wall", {}, 1}};
    document.terminated = true;

    std::vector<MtlSubmaterialBinding> bindings;
    Check(BindMtlSubmaterialsByName(nmfIdentifiers, document, bindings),
          "repeated NMF identifiers and case variation are evaluated");
    Check(bindings.size() == 3, "repeated NMF identifiers retain table order");
    if (bindings.size() == 3) {
        Check(bindings[0].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[0].sectionIndex == 0 &&
              bindings[1].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[1].sectionIndex == 0,
              "repeated NMF identifiers may refer to the same unique MTL section");
        Check(bindings[2].status == MtlSubmaterialBindingStatus::Missing,
              "identifier matching is case-sensitive exact token equality");
    }
}

void TestMalformedAndOverLimitInputsAreTransactional() {
    MtlDocument document;
    document.submaterials = {MtlSubmaterial{"wall", {}, 1}};
    document.terminated = true;
    std::vector<MtlSubmaterialBinding> output{
        {MtlSubmaterialBindingStatus::Matched, 23},
    };
    std::string error;

    Check(!BindMtlSubmaterialsByName({""}, document, output, &error) && !error.empty(),
          "empty NMF identifier is rejected");
    Check(output.size() == 1 && output[0].sectionIndex == 23,
          "malformed identifier preserves caller output");

    MtlDocument unterminated;
    unterminated.submaterials = {MtlSubmaterial{"wall", {}, 1}};
    Check(!BindMtlSubmaterialsByName({"wall"}, unterminated, output, &error) && !error.empty(),
          "unterminated caller-built MTL document is rejected");
    Check(output.size() == 1 && output[0].sectionIndex == 23,
          "unterminated MTL input preserves caller output");

    std::vector<std::string> tooManyIdentifiers(MaxMtlSubmaterialBindingEntries + 1, "wall");
    Check(!BindMtlSubmaterialsByName(tooManyIdentifiers, document, output, &error) && !error.empty(),
          "over-limit caller-constructed identifier list is rejected");
    Check(output.size() == 1 && output[0].sectionIndex == 23,
          "over-limit input preserves caller output");

    MtlDocument tooManySections;
    tooManySections.submaterials.resize(MaxMtlSubmaterialBindingEntries + 1,
                                        MtlSubmaterial{"wall", {}, 1});
    tooManySections.terminated = true;
    Check(!BindMtlSubmaterialsByName({"wall"}, tooManySections, output, &error) && !error.empty(),
          "over-limit caller-constructed MTL section list is rejected");
    Check(output.size() == 1 && output[0].sectionIndex == 23,
          "over-limit MTL input preserves caller output");

    MtlDocument emptyName;
    emptyName.submaterials = {MtlSubmaterial{"", {}, 1}};
    emptyName.terminated = true;
    Check(!BindMtlSubmaterialsByName({"wall"}, emptyName, output, &error) && !error.empty(),
          "empty MTL section name is rejected");
    Check(output.size() == 1 && output[0].sectionIndex == 23,
          "malformed MTL section preserves caller output");
}

} // namespace

void TestLoaderSelectionRule() {
    MtlDocument single;
    single.submaterials = {MtlSubmaterial{"lambert1", {}, 1}};
    single.terminated = true;
    std::vector<MtlSubmaterialBinding> bindings;
    Check(SelectMtlSubmaterialsLikeLoader({"____Default1", "other_name"}, single, bindings),
          "single-section document selects");
    Check(bindings.size() == 2 && bindings[0].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[0].sectionIndex == 0 && bindings[1].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[1].sectionIndex == 0,
          "a single section is selected for every identifier regardless of name");

    MtlDocument multi;
    multi.submaterials = {
        MtlSubmaterial{"first", {}, 1},
        MtlSubmaterial{"wall", {}, 2},
        MtlSubmaterial{"wall", {}, 3},
    };
    multi.terminated = true;
    Check(SelectMtlSubmaterialsLikeLoader({"wall", "Wall", "nothing"}, multi, bindings),
          "multi-section document selects");
    Check(bindings.size() == 3 && bindings[0].status == MtlSubmaterialBindingStatus::Matched &&
              bindings[0].sectionIndex == 1,
          "the first exact-name section wins and duplicates are not ambiguous");
    Check(bindings[1].status == MtlSubmaterialBindingStatus::FallbackFirstSection &&
              bindings[1].sectionIndex == 0 &&
              bindings[2].status == MtlSubmaterialBindingStatus::FallbackFirstSection &&
              bindings[2].sectionIndex == 0,
          "case differences and unknown names keep the first section");

    MtlDocument unterminated = multi;
    unterminated.terminated = false;
    std::vector<MtlSubmaterialBinding> unchanged{{MtlSubmaterialBindingStatus::Missing, 9}};
    Check(!SelectMtlSubmaterialsLikeLoader({"wall"}, unterminated, unchanged) &&
              unchanged.size() == 1 && unchanged[0].sectionIndex == 9,
          "invalid selection input leaves output unchanged");
}

int main() {
    TestReorderedSectionsMatchByExactName();
    TestMissingAndAmbiguousSections();
    TestDuplicateNmfIdentifiersAndExactCase();
    TestMalformedAndOverLimitInputsAreTransactional();
    TestLoaderSelectionRule();
    if (failures != 0) {
        std::cerr << failures << " MTL submaterial binding test(s) failed\n";
        return 1;
    }
    std::cout << "All MTL submaterial binding tests passed\n";
    return 0;
}
