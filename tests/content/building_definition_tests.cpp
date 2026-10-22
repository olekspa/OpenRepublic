#include "openrepublic/content/building_definition.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

int RunCorpus(const std::filesystem::path& root) {
    std::size_t files = 0;
    std::size_t parsed = 0;
    std::size_t failed = 0;
    std::size_t validConnections = 0;
    std::size_t invalidConnections = 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".ini") continue;
            ++files;
            std::error_code sizeError;
            const auto size = std::filesystem::file_size(entry.path(), sizeError);
            if (sizeError || size > MaxBuildingDefinitionBytes) {
                ++failed;
                continue;
            }
            std::ifstream input(entry.path(), std::ios::binary);
            if (!input) {
                ++failed;
                continue;
            }
            std::string text;
            text.reserve(static_cast<std::size_t>(size));
            char buffer[8192];
            bool tooLarge = false;
            while (input) {
                input.read(buffer, sizeof(buffer));
                const auto count = static_cast<std::size_t>(input.gcount());
                if (count > MaxBuildingDefinitionBytes - text.size()) {
                    tooLarge = true;
                    break;
                }
                text.append(buffer, count);
            }
            if (tooLarge || input.bad() || text.size() != size) {
                ++failed;
                continue;
            }
            BuildingDefinition definition;
            if (!ParseBuildingDefinition(text, definition)) {
                ++failed;
                continue;
            }
            ++parsed;
            for (const auto& connection : definition.electricalConnections) {
                if (connection.valid)
                    ++validConnections;
                else
                    ++invalidConnections;
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        ++failed;
    }

    std::cout << "files=" << files << " parsed=" << parsed << " failed=" << failed
              << " valid_connections=" << validConnections
              << " invalid_connections=" << invalidConnections << '\n';
    return failed == 0 ? 0 : 1;
}

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

const std::string kSynthetic =
    "$NAME 4242\r\n"
    "\r\n"
    "$VEHICLE_STATION -1 0 -1  1 0 -1\r\n"
    "$TYPE_FACTORY\r\n"
    "$WORKERS_NEEDED 50\r\n"
    "$PRODUCTION livestock 0.2\r\n"
    "$PRODUCTION vehicles 1.0  //comment words $CONSUMPTION nothing 9\r\n"
    "$CONSUMPTION plants 0.4\r\n"
    "$CONSUMPTION_PER_SECOND eletric 2.5\r\n"
    "$STORAGE RESOURCE_TRANSPORT_COVERED 75\r\n"
    "$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 100.00 bauxite\r\n"
    "$STORAGE_PACKCONTAINERS_FROM_STORAGE 0\r\n"
    "$RESOURCE_VISUALIZATION 1\r\n"
    "position 0.0 0.0 -12.0\r\n"
    "-------\r\n"
    "$WORKING_VEHICLES_NEEDED 2\r\n";

void TestTypedExtraction() {
    BuildingDefinition definition;
    std::string error;
    Check(ParseBuildingDefinition(kSynthetic, definition, &error), "synthetic definition parses: " + error);
    Check(definition.nameId && *definition.nameId == "4242" && definition.typeFlags.size() == 1 &&
              definition.typeFlags[0] == "FACTORY",
          "name id and type flag are extracted");
    Check(definition.workersNeeded && *definition.workersNeeded == 50.0 &&
              definition.workingVehiclesNeeded && *definition.workingVehiclesNeeded == 2.0,
          "worker and vehicle counts are extracted");
    Check(definition.production.size() == 2 && definition.production[0].resource == "livestock" &&
              definition.production[0].value == 0.2 && definition.production[1].resource == "vehicles",
          "production entries keep order and drop trailing comments");
    Check(definition.production[0].workersDeclaredBefore && definition.consumption[0].workersDeclaredBefore,
          "rates after $WORKERS_NEEDED are flagged as scaled by workers");
    Check(definition.consumption.size() == 1 && definition.consumption[0].resource == "plants" &&
              definition.consumptionPerSecond.size() == 1 && definition.consumptionPerSecond[0].value == 2.5,
          "a directive inside a comment is not extracted");
    Check(definition.storage.size() == 2 && definition.storage[0].transportClass == "RESOURCE_TRANSPORT_COVERED" &&
              definition.storage[0].amount == 75.0 && definition.storage[0].resource.empty() &&
              definition.storage[1].directive == "$STORAGE_IMPORT_SPECIAL" &&
              definition.storage[1].resource == "bauxite",
          "storage directives keep their class, amount and optional resource");
    bool preserved = false;
    for (const auto& directive : definition.other) {
        preserved = preserved || directive.name == "$STORAGE_PACKCONTAINERS_FROM_STORAGE";
    }
    Check(preserved && definition.other.size() == 3 && definition.ignoredLines == 2,
          "other directives are preserved and data/separator rows are counted");
}

void TestRejectsMalformedTypedDirectives() {
    BuildingDefinition keep;
    keep.ignoredLines = 7;
    BuildingDefinition out = keep;
    const auto rejects = [&](const std::string& text, const std::string& label) {
        std::string error;
        Check(!ParseBuildingDefinition(text, out, &error) && !error.empty() && out.ignoredLines == 7, label);
    };
    rejects("$PRODUCTION coal\n", "a production entry without a rate is rejected");
    rejects("$PRODUCTION coal abc\n", "a non-numeric rate is rejected");
    rejects("$CONSUMPTION coal nan\n", "a non-finite rate is rejected");
    rejects("$WORKERS_NEEDED\n", "a worker count without a value is rejected");
    rejects("$NAME 1 2\n", "a name with two arguments is rejected");
    rejects(std::string("$NAME 1\0\n", 9), "embedded NUL is rejected");
    rejects(std::string(MaxBuildingDefinitionBytes + 1, 'x'), "oversized input is rejected");
    rejects("$NAME " + std::string(5000, '1') + "\n", "over-long lines are rejected");
}

void TestElectricalConnectionDeclarations() {
    const std::string text =
        "$CONNECTION_ELETRIC_HIGH_INPUT\r\n"
        "  // comment between points\r\n"
        "-1.25 0 2.5 // first point\r\n"
        "\r\n"
        "3 4.5 6\r\n"
        "$CONNECTION_ELETRIC_HIGH_OUTPUT\n"
        "0 1 2\n3 4 5\n"
        "$CONNECTION_ELETRIC_LOW_INPUT\n"
        "0 0 0\n1 1 1\n"
        "$CONNECTION_ELETRIC_LOW_OUTPUT\n"
        "0 0 0\n1 1 1\n"
        "$CONNECTION_ELETRIC_HIGH_INPUT_ONLY\n"
        "0 0 0\n1 1 1\n"
        "$CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY\n"
        "0 0 0\n1 1 1\n"
        "$CONNECTION_ELETRIC_LOW_INPUT_ONLY\n"
        "0 0 0\n1 1 1\n"
        "$CONNECTION_ELETRIC_LOW_OUTPUT_ONLY\n"
        "0 0 0\n1 1 1\n"
        "$RESOURCE_VISUALIZATION 1\n"
        "separator\n";

    BuildingDefinition definition;
    std::string error;
    Check(ParseBuildingDefinition(text, definition, &error), "all eight electrical spellings parse: " + error);
    const std::vector<std::string> expected{
        "$CONNECTION_ELETRIC_HIGH_INPUT", "$CONNECTION_ELETRIC_HIGH_OUTPUT",
        "$CONNECTION_ELETRIC_LOW_INPUT", "$CONNECTION_ELETRIC_LOW_OUTPUT",
        "$CONNECTION_ELETRIC_HIGH_INPUT_ONLY", "$CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY",
        "$CONNECTION_ELETRIC_LOW_INPUT_ONLY", "$CONNECTION_ELETRIC_LOW_OUTPUT_ONLY"};
    Check(definition.electricalConnections.size() == expected.size(),
          "all electrical declarations are retained in source order");
    if (definition.electricalConnections.size() == expected.size()) {
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto& connection = definition.electricalConnections[i];
            Check(connection.directive == expected[i] && connection.valid && connection.diagnostic.empty() &&
                      connection.points[0].has_value() && connection.points[1].has_value(),
                  "electrical directive spelling, coordinates, and validity are retained");
            Check(connection.line > 0, "electrical declaration preserves source line");
        }
        const auto& first = definition.electricalConnections.front();
        Check((*first.points[0])[0] == -1.25 && (*first.points[0])[1] == 0.0 &&
                  (*first.points[0])[2] == 2.5 && (*first.points[1])[0] == 3.0 &&
                  (*first.points[1])[1] == 4.5 && (*first.points[1])[2] == 6.0,
              "whitespace triples retain coordinate order and CRLF is accepted");
    }
    Check(definition.other.size() == 9 && definition.other[0].name == "$CONNECTION_ELETRIC_HIGH_INPUT" &&
              definition.other[0].arguments.empty() && definition.other[8].name == "$RESOURCE_VISUALIZATION" &&
              definition.ignoredLines == 17,
          "electrical and unknown headers remain unchanged and every data row remains counted");
}

void TestMalformedElectricalRowsAreOptionalDiagnostics() {
    const std::string text =
        "$NAME 73\n"
        "$PRODUCTION food 0.25\n"
        "$CONNECTION_ELETRIC_HIGH_INPUT\n"
        "nan 0 0\n"
        "1 2\n"
        "$CONNECTION_ELETRIC_LOW_OUTPUT foo\n"
        "0 0 0\n"
        "1 1 1\n"
        "$CONNECTION_ELETRIC_HIGH_OUTPUT\n"
        "0 0 0 9\n"
        "1 1 1\n"
        "$CONNECTION_ELETRIC_LOW_INPUT_ONLY\n"
        "0 0 0\n"
        "$CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY\n"
        "8 8 8\n9 9 9\n"
        "$UNKNOWN_KEEP these arguments\n"
        "tail row\n";
    BuildingDefinition definition;
    std::string error;
    Check(ParseBuildingDefinition(text, definition, &error),
          "malformed optional electrical rows do not reject valid economics: " + error);
    Check(definition.nameId && *definition.nameId == "73" && definition.production.size() == 1 &&
              definition.production[0].resource == "food" && definition.production[0].value == 0.25,
          "economic declarations survive malformed electrical data");
    Check(definition.electricalConnections.size() == 5, "interrupted blocks are retained and later blocks recover");
    if (definition.electricalConnections.size() == 5) {
        const auto& nonfinite = definition.electricalConnections[0];
        Check(!nonfinite.valid && !nonfinite.points[0] && !nonfinite.points[1] &&
                  nonfinite.diagnostic.find("malformed coordinate row") != std::string::npos,
              "non-finite and wrong-arity rows remain missing with diagnostics");
        const auto& inlineArgs = definition.electricalConnections[1];
        Check(!inlineArgs.valid && inlineArgs.points[0] && inlineArgs.points[1] &&
                  inlineArgs.diagnostic.find("inline arguments") != std::string::npos,
              "inline header arguments diagnose without suppressing captured points");
        const auto& extraCoordinate = definition.electricalConnections[2];
        Check(!extraCoordinate.valid && !extraCoordinate.points[0] && extraCoordinate.points[1],
              "coordinate rows require exactly three finite values");
        const auto& interrupted = definition.electricalConnections[3];
        Check(!interrupted.valid && interrupted.points[0] && !interrupted.points[1] &&
                  interrupted.diagnostic.find("before directive") != std::string::npos,
              "a following directive interrupts an incomplete block");
        Check(definition.electricalConnections[4].valid &&
                  definition.electricalConnections[4].directive == "$CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY",
              "a valid declaration after malformed blocks is recovered");
        Check(definition.electricalConnections[3].line < definition.electricalConnections[4].line,
              "recovered declarations retain their source lines and order");
    }
    Check(definition.other.size() == 6 && definition.other[0].name == "$CONNECTION_ELETRIC_HIGH_INPUT" &&
              definition.other[1].name == "$CONNECTION_ELETRIC_LOW_OUTPUT" &&
              definition.other[1].arguments.size() == 1 && definition.other[5].name == "$UNKNOWN_KEEP" &&
              definition.other[5].arguments.size() == 2 && definition.ignoredLines == 10,
          "unknown directives and ignored-row accounting remain preserved");
}

void TestMissingElectricalRowsAtEnd() {
    BuildingDefinition definition;
    std::string error;
    Check(ParseBuildingDefinition("$CONNECTION_ELETRIC_LOW_INPUT_ONLY\n0 1 2\n", definition, &error),
          "end-of-file inside optional electrical block is diagnostic, not fatal");
    Check(definition.electricalConnections.size() == 1 && !definition.electricalConnections[0].valid &&
              definition.electricalConnections[0].points[0] && !definition.electricalConnections[0].points[1] &&
              definition.electricalConnections[0].diagnostic.find("found 1") != std::string::npos,
          "missing final row is represented explicitly");
}

void TestElectricalFloatConversionBoundaries() {
    BuildingDefinition definition;
    std::string error;
    const std::string text =
        "$CONNECTION_ELETRIC_HIGH_INPUT\n"
        "-4.50.0 6.25\n"
        "1-2 3\n"
        "$CONNECTION_ELETRIC_LOW_OUTPUT\n"
        "1e2-2e-1 .3E+1\n"
        "0 0 0\n"
        "$CONNECTION_ELETRIC_LOW_INPUT\n"
        "0 1 2trailing\n"
        "3 4 5\n";
    Check(ParseBuildingDefinition(text, definition, &error),
          "adjacent float conversions and exponent forms are parsed: " + error);
    Check(definition.electricalConnections.size() == 3 && definition.electricalConnections[0].valid &&
              definition.electricalConnections[1].valid,
          "scanf-style adjacent conversions remain valid declarations");
    if (definition.electricalConnections.size() == 3) {
        const auto& adjacent = definition.electricalConnections[0];
        Check((*adjacent.points[0])[0] == -4.5 && (*adjacent.points[0])[1] == 0.0 &&
                  (*adjacent.points[0])[2] == 6.25 && (*adjacent.points[1])[0] == 1.0 &&
                  (*adjacent.points[1])[1] == -2.0 && (*adjacent.points[1])[2] == 3.0,
              "adjacent signed and decimal forms split at conversion boundaries");
        const auto& exponent = definition.electricalConnections[1];
        Check((*exponent.points[0])[0] == 100.0 && (*exponent.points[0])[1] == -0.2 &&
                  (*exponent.points[0])[2] == 3.0,
              "exponent forms parse across adjacent conversion boundaries");
        const auto& trailing = definition.electricalConnections[2];
        Check(!trailing.valid && !trailing.points[0] && trailing.points[1] &&
                  !trailing.diagnostic.empty(),
              "trailing nonnumeric text leaves its coordinate row missing with a diagnostic");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 1) {
        if (argc == 3 && std::string(argv[1]) == "--corpus-dir")
            return RunCorpus(std::filesystem::path(argv[2]));
        std::cerr << "usage: OpenRepublicBuildingDefinitionTests [--corpus-dir <directory>]\n";
        return 2;
    }
    TestTypedExtraction();
    TestRejectsMalformedTypedDirectives();
    TestElectricalConnectionDeclarations();
    TestMalformedElectricalRowsAreOptionalDiagnostics();
    TestMissingElectricalRowsAtEnd();
    TestElectricalFloatConversionBoundaries();
    if (failures != 0) {
        std::cerr << failures << " building-definition test(s) failed\n";
        return 1;
    }
    std::cout << "All building-definition tests passed\n";
    return 0;
}
