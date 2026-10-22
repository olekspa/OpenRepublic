#include "openrepublic/content/building_definition.h"

#include <iostream>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

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

} // namespace

int main() {
    TestTypedExtraction();
    TestRejectsMalformedTypedDirectives();
    if (failures != 0) {
        std::cerr << failures << " building-definition test(s) failed\n";
        return 1;
    }
    std::cout << "All building-definition tests passed\n";
    return 0;
}
