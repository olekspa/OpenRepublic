#include "openrepublic/content/mtl_preview_color.h"

#include <cmath>
#include <iostream>
#include <string>
#include <utility>
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

MtlDirective Color(std::vector<std::string> arguments) {
    return MtlDirective{"$DIFFUSECOLOR", std::move(arguments),
                        MtlDirectiveKind::DiffuseColor, 1};
}

void TestOrdinaryValues() {
    const MtlSubmaterial submaterial{
        "wall", {Color({"0.25", "0.5", "1", "0.75"})}, 1};
    const auto result = ExtractMtlDiffuseColorForPreview(submaterial);
    Check(result.status == MtlPreviewDiffuseColorStatus::Valid,
          "ordinary diffuse values convert successfully");
    Check(std::fabs(result.value[0] - 0.25f) < 1e-6f &&
          std::fabs(result.value[1] - 0.5f) < 1e-6f &&
          result.value[2] == 1.0f && std::fabs(result.value[3] - 0.75f) < 1e-6f,
          "all four converted channels are retained");
}

void TestLeadingPlus() {
    const MtlSubmaterial submaterial{
        "wall", {Color({"+0.25", "+1", "+2.5e-1", "+4."})}, 1};
    const auto result = ExtractMtlDiffuseColorForPreview(submaterial);
    Check(result.status == MtlPreviewDiffuseColorStatus::Valid,
          "leading plus signs accepted by MTL syntax convert successfully");
    Check(result.value[0] == 0.25f && result.value[1] == 1.0f &&
          result.value[2] == 0.25f && result.value[3] == 4.0f,
          "leading-plus numeric values are converted as expected");
}

void TestRepeatedDirectiveUsesLastValue() {
    const MtlSubmaterial submaterial{
        "wall", {Color({"1", "1", "1", "1"}),
                 Color({"0.1", "0.2", "0.3", "0.4"})}, 1};
    const auto result = ExtractMtlDiffuseColorForPreview(submaterial);
    Check(result.status == MtlPreviewDiffuseColorStatus::Valid,
          "repeated valid diffuse directives remain valid");
    Check(std::fabs(result.value[0] - 0.1f) < 1e-6f &&
          std::fabs(result.value[1] - 0.2f) < 1e-6f &&
          std::fabs(result.value[2] - 0.3f) < 1e-6f &&
          std::fabs(result.value[3] - 0.4f) < 1e-6f,
          "last diffuse directive wins");
}

void TestMissingAndNameKindMustBothMatch() {
    MtlSubmaterial submaterial{"wall", {
        MtlDirective{"$DIFFUSECOLOR", {"1", "1", "1", "1"},
                     MtlDirectiveKind::Unknown, 1},
        MtlDirective{"$OTHER", {"1", "1", "1", "1"},
                     MtlDirectiveKind::DiffuseColor, 2},
    }, 1};
    const auto result = ExtractMtlDiffuseColorForPreview(submaterial);
    Check(result.status == MtlPreviewDiffuseColorStatus::Absent,
          "a missing exact name-and-kind match returns Absent");
}

void TestFloatOverflowIsInvalid() {
    const MtlSubmaterial submaterial{
        "wall", {Color({"1e39", "0", "0", "1"})}, 1};
    const auto result = ExtractMtlDiffuseColorForPreview(submaterial);
    Check(result.status == MtlPreviewDiffuseColorStatus::Invalid,
          "finite double token outside float range returns Invalid");
}

void TestFloatUnderflowIsInvalid() {
    const MtlSubmaterial submaterial{
        "wall", {Color({"1e-50", "0", "0", "1"})}, 1};
    const auto result = ExtractMtlDiffuseColorForPreview(submaterial);
    Check(result.status == MtlPreviewDiffuseColorStatus::Invalid,
          "nonzero finite double token below float range returns Invalid");
}

void TestInvalidLastDirectiveDoesNotFallBack() {
    const MtlSubmaterial submaterial{
        "wall", {Color({"0.1", "0.2", "0.3", "1"}),
                 Color({"1e39", "0", "0", "1"})}, 1};
    const auto result = ExtractMtlDiffuseColorForPreview(submaterial);
    Check(result.status == MtlPreviewDiffuseColorStatus::Invalid,
          "an invalid last diffuse directive overrides an earlier valid value");
}

} // namespace

int main() {
    TestOrdinaryValues();
    TestLeadingPlus();
    TestRepeatedDirectiveUsesLastValue();
    TestMissingAndNameKindMustBothMatch();
    TestFloatOverflowIsInvalid();
    TestFloatUnderflowIsInvalid();
    TestInvalidLastDirectiveDoesNotFallBack();
    if (failures != 0) {
        std::cerr << failures << " MTL preview color test(s) failed\n";
        return 1;
    }
    std::cout << "All MTL preview color tests passed\n";
    return 0;
}
