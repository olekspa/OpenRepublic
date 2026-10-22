#include "openrepublic/scripting/economy_rules.h"

#include <algorithm>
#include <iostream>
#include <string>

using namespace openrepublic;

namespace {
int failures = 0;
void Check(bool condition, const std::string &label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}
} // namespace

int main() {
    const economy::Catalog base = economy::MakeSyntheticCatalog();
    economy::Catalog tuned;
    std::string error;
    Check(scripting::EconomyRules::ApplyFile(OPENREPUBLIC_ECONOMY_RULES_PATH, base, tuned, &error),
          "the shipped example rules apply: " + error);
    economy::BuildingTypeId farm{}, mill{};
    economy::VehicleTypeId truck{};
    Check(base.FindBuildingType("farm", farm) && base.FindBuildingType("mill", mill) &&
              base.FindVehicleType("truck", truck),
          "synthetic catalog names exist");
    const auto *before = base.BuildingType(farm);
    const auto *after = tuned.BuildingType(farm);
    Check(before && after && after->recipes[0].intervalTicks <
                                  before->recipes[0].intervalTicks + (before->recipes[0].intervalTicks == 1 ? 1 : 0) &&
              after->recipes[0].intervalTicks == std::max<std::uint32_t>(1, before->recipes[0].intervalTicks * 100 / 150),
          "farm speed 150% shortens the recipe interval");
    Check(tuned.BuildingType(mill)->storage[0].capacity == base.BuildingType(mill)->storage[0].capacity * 2,
          "mill storage 200% doubles capacity");
    Check(tuned.VehicleType(truck)->capacity == base.VehicleType(truck)->capacity * 2, "truck capacity doubles");

    const economy::Catalog snapshot = tuned;
    const char *bad[] = {
        "return { schema_version = 1, buildings = { castle = { speed_percent = 100 } } }",
        "return { schema_version = 1, buildings = { farm = { speed_percent = 5 } } }",
        "return { schema_version = 1, buildings = { farm = { magic = 100 } } }",
        "return { schema_version = 2 }",
        "return { schema_version = 1, extra = {} }",
        "return { schema_version = 1, buildings = { farm = { speed_percent = 150.5 } } }",
        "os.exit() return {}",
        "while true do end",
        "return 5",
    };
    for (const char *text : bad) {
        economy::Catalog out = tuned;
        Check(!scripting::EconomyRules::ApplyText(text, base, out, &error) && !error.empty() &&
                  economy::WriteCatalogText(out) == economy::WriteCatalogText(snapshot),
              std::string("rejected without changing output: ") + text);
    }
    if (failures != 0) {
        std::cerr << failures << " economy-rules test(s) failed\n";
        return 1;
    }
    std::cout << "All economy-rules tests passed\n";
    return 0;
}
