#include "openrepublic/content/building_construction.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::size_t Count(const BuildingConstructionProfile& profile,
                  BuildingConstructionDiagnosticCode code) {
    std::size_t result = 0;
    for (const auto& diagnostic : profile.diagnostics) result += diagnostic.code == code;
    return result;
}

void TestOrderedOperationsAndImmediateMultiplierPosition() {
    const std::string text =
        "$COST_ALL_MULTIPLY 2.00\r\n"
        "$COST_WORK foundation 1e2\r\n"
        "$COST_RESOURCE steel 3.500 // repeated declarations stay distinct\r\n"
        "$COST_RESOURCE steel 3.500\r\n"
        "$COST_ALL_MULTIPLY 0.5\r\n"
        "$COST_WORK shell -0\r\n"
        "$COST_RESOURCE_AUTO wall_concrete 1.25\r\n"
        "$COST_ALL_MULTIPLY 4\r\n";
    BuildingConstructionProfile profile;
    std::string error;
    Check(ParseBuildingConstructionProfile(text, profile, &error),
          "ordered declarations parse: " + error);
    Check(profile.operations.size() == 8, "every COST directive is retained in source order");
    if (profile.operations.size() == 8) {
        const auto& ops = profile.operations;
        Check(ops[0].kind == BuildingConstructionOperationKind::MultiplyExisting &&
                  ops[0].arguments[0] == "2.00" && ops[0].numericValue == 2.0,
              "leading multiplier keeps its raw spelling and lexical finite value");
        Check(ops[1].kind == BuildingConstructionOperationKind::Work &&
                  ops[1].arguments[0] == "foundation" && ops[1].arguments[1] == "1e2" &&
                  ops[1].numericValue == 100.0,
              "phase identifier and numeric spelling are preserved without lookup");
        Check(ops[2].workOperationIndex == 1 && ops[3].workOperationIndex == 1 &&
                  ops[2].arguments[0] == "steel" && ops[3].arguments[0] == "steel" &&
                  ops[2].arguments[1] == "3.500" && ops[3].arguments[1] == "3.500",
              "duplicate resource declarations remain distinct and bind to latest phase");
        Check(ops[4].kind == BuildingConstructionOperationKind::MultiplyExisting &&
                  ops[5].kind == BuildingConstructionOperationKind::Work &&
                  ops[6].workOperationIndex == 5 &&
                  ops[7].kind == BuildingConstructionOperationKind::MultiplyExisting,
              "multipliers remain at their source positions and do not persist as profile factors");
        Check(ops[6].kind == BuildingConstructionOperationKind::AutomaticResource &&
                  ops[6].arguments[0] == "wall_concrete" && ops[6].arguments[1] == "1.25" &&
                  Count(profile, BuildingConstructionDiagnosticCode::UnresolvedAutomaticRecipe) == 1,
              "automatic selector stays raw and explicitly unresolved");
        Check(ops[0].line == 1 && ops[1].line == 2 && ops[6].line == 7,
              "CRLF source line numbers are retained");
    }
}

void TestMalformedWorkResetsPhaseAndOrphansAreRetained() {
    const std::string text =
        "$COST_WORK phase_a 2\n"
        "$COST_RESOURCE ore 1\n"
        "$COST_WORK phase_b nope\n"
        "$COST_RESOURCE coal 2\n"
        "$COST_WORK too many 3\n"
        "$COST_RESOURCE_AUTO recipe 4\n";
    BuildingConstructionProfile profile;
    Check(ParseBuildingConstructionProfile(text, profile), "malformed optional COST declarations do not reject file");
    Check(profile.operations.size() == 6, "malformed declarations remain in the ordered profile");
    if (profile.operations.size() == 6) {
        Check(profile.operations[1].workOperationIndex == 0,
              "resource binds to the latest valid work operation");
        Check(!profile.operations[2].numericValue && !profile.operations[3].workOperationIndex &&
                  !profile.operations[4].numericValue && !profile.operations[5].workOperationIndex,
              "malformed WORK clears context and later declarations do not bind to an earlier phase");
    }
    Check(Count(profile, BuildingConstructionDiagnosticCode::MalformedArguments) == 2,
          "malformed WORK argument forms are diagnosed");
    Check(Count(profile, BuildingConstructionDiagnosticCode::OrphanPhase) == 2,
          "resource declarations without valid current phase receive orphan diagnostics");
    Check(Count(profile, BuildingConstructionDiagnosticCode::UnresolvedAutomaticRecipe) == 1,
          "automatic recipe remains unresolved even when orphaned");
}

void TestUnsupportedSelectorsKeepContinuationRows() {
    const std::string text =
        "$COST_WORK_BUILDING_VEHICLE_STATION\r\n"
        "  -1 0 -1 1 0 -1 // station data, not interpreted\r\n"
        "\r\n"
        "$COST_WORK_BUILDING_NODE entrance\r\n"
        "node continuation\r\n"
        "$COST_WORK foundation 7\r\n"
        "  trailing work row // retained too\r\n"
        "$NAME 42\r\n";
    BuildingConstructionProfile profile;
    Check(ParseBuildingConstructionProfile(text, profile), "opaque selectors parse as optional declarations");
    Check(profile.operations.size() == 3 &&
              Count(profile, BuildingConstructionDiagnosticCode::UnsupportedDirective) == 2,
          "unsupported COST selectors remain in source order with diagnostics");
    if (profile.operations.size() == 3) {
        Check(profile.operations[0].directive == "$COST_WORK_BUILDING_VEHICLE_STATION" &&
                  profile.operations[0].retainedContinuation.size() == 2 &&
                  profile.operations[0].retainedContinuation[0].line == 2 &&
                  profile.operations[0].retainedContinuation[0].text ==
                      "  -1 0 -1 1 0 -1 // station data, not interpreted" &&
                  profile.operations[0].retainedContinuation[1].text.empty(),
              "unknown station data and intervening blank row are preserved verbatim with provenance");
        Check(profile.operations[1].directive == "$COST_WORK_BUILDING_NODE" &&
                  profile.operations[1].retainedContinuation.size() == 1 &&
                  profile.operations[1].retainedContinuation[0].text == "node continuation" &&
                  !profile.operations[1].workOperationIndex,
              "node-selector continuation remains opaque raw text");
        Check(profile.operations[2].retainedContinuation.size() == 1 &&
                  profile.operations[2].retainedContinuation[0].line == 7 &&
                  profile.operations[2].retainedContinuation[0].text == "  trailing work row // retained too" &&
                  Count(profile, BuildingConstructionDiagnosticCode::UnexpectedContinuation) == 1,
              "known operation rows are retained and diagnosed as invalidating typed usability");
    }
}

void TestNumericValidationAndTransactionalHardFailures() {
    BuildingConstructionProfile profile;
    Check(ParseBuildingConstructionProfile(
              "$COST_WORK zero 0\n$COST_WORK negative -1.5\n"
              "$COST_WORK bad nan\n$COST_RESOURCE x inf\n"
              "$COST_ALL_MULTIPLY 1e9999\n$COST_ALL_MULTIPLY 1.0tail\n",
              profile), "finite zero and negative values are retained without gameplay interpretation");
    Check(profile.operations.size() == 6 && profile.operations[0].numericValue == 0.0 &&
              profile.operations[1].numericValue == -1.5 &&
              !profile.operations[2].numericValue && !profile.operations[3].numericValue &&
              !profile.operations[4].numericValue && !profile.operations[5].numericValue,
          "finite lexical conversion is separated from malformed and non-finite numeric tokens");
    Check(Count(profile, BuildingConstructionDiagnosticCode::MalformedArguments) == 4,
          "non-finite and malformed numeric spellings have diagnostics");

    BuildingConstructionProfile unchanged;
    unchanged.operations.push_back({BuildingConstructionOperationKind::Work, "$OLD", {"keep"}, 99});
    const auto rejects = [&](const std::string& input, const std::string& label) {
        const auto beforeSize = unchanged.operations.size();
        std::string error;
        Check(!ParseBuildingConstructionProfile(input, unchanged, &error) && !error.empty() &&
                  unchanged.operations.size() == beforeSize && unchanged.operations[0].directive == "$OLD",
              label);
    };
    rejects(std::string("$COST_WORK phase 1\n\0tail", 24), "embedded NUL rejects transactionally");
    rejects(std::string(MaxBuildingDefinitionBytes + 1, 'x'), "oversized input rejects transactionally");
    rejects("$NAME " + std::string(5000, '1') + "\n", "overlong line rejects transactionally");
    std::string tooManyTokens = "$COST_WORK";
    for (int i = 0; i < 130; ++i) tooManyTokens += " x";
    tooManyTokens += '\n';
    rejects(tooManyTokens, "the existing parser's token limits reject transactionally");
    rejects("$PRODUCTION coal invalid\n", "existing definition validation rejects invalid required economics");
    std::string tooManyContinuationRows = "$COST_WORK phase 1\n";
    tooManyContinuationRows.append(MaxBuildingConstructionContinuationRows + 1, '\n');
    rejects(tooManyContinuationRows, "continuation row cap rejects transactionally");
}

void TestSourceContextAndActualStationSpelling() {
    BuildingConstructionProfile profile;
    Check(ParseBuildingConstructionProfile(
        "$COST_WORK foundation 2\r\n"
        "$COST_RESOURCE steel invalid\r\n"
        "$COST_RESOURCE_AUTO wall_steel invalid\r\n"
        "$COST_WORK_VEHICLE_STATION\r\n"
        " // opaque station rows\r\n\r\n"
        " -1 0 -1\r\n 1 0 1 // second row\r\n"
        "$COST_RESOURCE steel 3\r\n"
        "$COST_WORK phase_with_rows 4\r\n"
        "opaque trailing data\r\n"
        "$COST_RESOURCE steel 5\r\n", profile),
        "source context survives malformed resources and opaque station rows");
    Check(profile.operations.size() == 7, "station does not consume a following directive");
    if (profile.operations.size() == 7) {
        Check(profile.operations[1].workOperationIndex == 0 &&
              profile.operations[2].workOperationIndex == 0 &&
              !profile.operations[1].numericValue && !profile.operations[2].numericValue,
              "malformed resource numbers keep source phase association without typed values");
        const auto& station = profile.operations[3];
        Check(station.kind == BuildingConstructionOperationKind::Unsupported &&
              station.directive == "$COST_WORK_VEHICLE_STATION" && station.workOperationIndex == 0 &&
              station.retainedContinuation.size() == 4 &&
              station.retainedContinuation[2].line == 7 && station.retainedContinuation[2].text == " -1 0 -1" &&
              station.retainedContinuation[3].line == 8 && station.retainedContinuation[3].text == " 1 0 1 // second row",
              "phase-scoped station preserves both opaque rows, comments and blank line");
        Check(profile.operations[4].workOperationIndex == 0 &&
              !profile.operations[6].workOperationIndex,
              "opaque station retains phase context but unexpected WORK row clears it");
    }
    Check(Count(profile, BuildingConstructionDiagnosticCode::OrphanPhase) == 1 &&
          Count(profile, BuildingConstructionDiagnosticCode::UnsupportedDirective) == 1,
          "context and unsupported-selector diagnostics stay explicit");
}

} // namespace

int main() {
    TestOrderedOperationsAndImmediateMultiplierPosition();
    TestMalformedWorkResetsPhaseAndOrphansAreRetained();
    TestUnsupportedSelectorsKeepContinuationRows();
    TestNumericValidationAndTransactionalHardFailures();
    TestSourceContextAndActualStationSpelling();
    if (failures != 0) {
        std::cerr << failures << " construction-profile test(s) failed\n";
        return 1;
    }
    std::cout << "All building-construction-profile tests passed\n";
    return 0;
}
