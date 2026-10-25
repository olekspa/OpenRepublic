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
    economy::Catalog utilityBase;
    economy::ParseError utilityParse;
    Check(economy::ParseCatalogText(
        "openrepublic-economy 1\nresource coal\nutility power\n"
        "building generator 1 1\nstore coal 8\nrecipe generate every 1 in coal:1 out power:3\nend\n",
        utilityBase, utilityParse), "utility tuning base catalog parses");
    economy::Catalog utilityTuned;
    Check(scripting::EconomyRules::ApplyText("return { schema_version = 1 }", utilityBase, utilityTuned, &error) &&
          economy::WriteCatalogText(utilityTuned) == economy::WriteCatalogText(utilityBase) &&
          utilityTuned.Resources().size() == 2 && utilityTuned.Resources()[1].utility,
          "Lua catalog rebuild preserves utility kind and recipe without inventing storage");
    const std::string utilityBefore = economy::WriteCatalogText(utilityTuned);
    Check(!scripting::EconomyRules::ApplyText(
        "return { schema_version = 1, buildings = { generator = { speed_percent = 50 } } }",
        utilityBase, utilityTuned, &error) && !error.empty() &&
        economy::WriteCatalogText(utilityTuned) == utilityBefore,
        "unsupported longer utility intervals reject transactionally during tuning");

    economy::Catalog optInBase;
    economy::ParseError optInParse;
    const char *optInSource =
        "openrepublic-economy 1\n"
        "resource food\n"
        "resource grain\n"
        "utility power\n"
        "trade food buy 8 sell 4\n"
        "food food 1 every 3\n"
        "building homes 1 1\n"
        "  housing 5\n"
        "  store food 20\n"
        "end\n"
        "building farm 1 1\n"
        "  workers 4\n"
        "  store food 100\n"
        "  tradepost\n"
        "  recipe harvest every 4 out food:10\n"
        "end\n"
        "building foodworks 1 1\n"
        "  store food 100\n"
        "  recipe provision every 1 out food:10\n"
        "end\n"
        "building generator 1 1\n"
        "  recipe generate every 1 out power:2\n"
        "end\n"
        "vehicle truck capacity 20 speed 2\n";
    Check(economy::ParseCatalogText(optInSource, optInBase, optInParse),
          "catalog with population, food, trade and utility declarations parses");

    economy::Catalog citizensBase;
    Check(economy::ParseCatalogText(
              "openrepublic-economy 1\nresource food\nfood food 1 every 3\ncitizens\n"
              "building home 1 1\n housing 2\n store food 8\nend\n",
              citizensBase, optInParse), "citizen catalog parses for Lua metadata preservation");
    economy::Catalog citizensUntuned;
    Check(scripting::EconomyRules::ApplyText("return { schema_version = 1 }", citizensBase,
                                             citizensUntuned, &error) &&
              economy::WriteCatalogText(citizensUntuned) == economy::WriteCatalogText(citizensBase),
          "empty Lua tuning preserves the opt-in citizens declaration");

    economy::Catalog optInUntuned;
    Check(scripting::EconomyRules::ApplyText("return { schema_version = 1 }", optInBase, optInUntuned, &error) &&
              economy::WriteCatalogText(optInUntuned) == economy::WriteCatalogText(optInBase),
          "empty Lua tuning preserves canonical citizens, population, food, trade-price, trade-post and utility metadata");

    economy::Catalog optInTuned;
    const char *optInTuning =
        "return { schema_version = 1, buildings = {"
        " homes = { storage_percent = 150 },"
        " farm = { speed_percent = 200, storage_percent = 200 } } }";
    Check(scripting::EconomyRules::ApplyText(optInTuning, optInBase, optInTuned, &error),
          "valid rate and storage tuning applies to an opted-in catalog: " + error);
    economy::BuildingTypeId homes{}, optInFarm{}, foodworks{}, generator{};
    Check(optInTuned.FindBuildingType("homes", homes) && optInTuned.FindBuildingType("farm", optInFarm) &&
              optInTuned.FindBuildingType("foodworks", foodworks) &&
              optInTuned.FindBuildingType("generator", generator), "opt-in building types survive tuning");
    const auto *homesDef = optInTuned.BuildingType(homes);
    const auto *farmDef = optInTuned.BuildingType(optInFarm);
    const auto *generatorDef = optInTuned.BuildingType(generator);
    Check(optInTuned.Food().resource == optInBase.Food().resource &&
              optInTuned.Food().amount == optInBase.Food().amount &&
              optInTuned.Food().intervalTicks == optInBase.Food().intervalTicks &&
              optInTuned.Resources()[0].tradeDeclared && optInTuned.Resources()[0].buyPrice == 8 &&
              optInTuned.Resources()[0].sellPrice == 4 && homesDef && homesDef->housing == 5 &&
              farmDef && farmDef->workers == 4 && farmDef->tradePost && farmDef->recipes[0].intervalTicks == 2 &&
              generatorDef && generatorDef->recipes[0].outputs[0].resource ==
                  optInTuned.Resources()[2].id && optInTuned.Resources()[2].utility,
          "rate and storage tuning preserves population, food, trade and utility opt-ins");

    economy::World optInWorld(optInBase, 8, 8);
    const auto homesPlacement = optInWorld.PlaceBuilding(homes, {0, 2});
    const auto farmPlacement = optInWorld.PlaceBuilding(optInFarm, {2, 2});
    const auto foodworksPlacement = optInWorld.PlaceBuilding(foodworks, {4, 2});
    const auto generatorPlacement = optInWorld.PlaceBuilding(generator, {6, 2});
    Check(homesPlacement && farmPlacement && foodworksPlacement && generatorPlacement,
          "opt-in world queues authored building placements");
    Check(optInWorld.Step().accepted &&
              optInWorld.GetCommandResult(homesPlacement).state == economy::CommandState::Accepted &&
              optInWorld.GetCommandResult(farmPlacement).state == economy::CommandState::Accepted &&
              optInWorld.GetCommandResult(foodworksPlacement).state == economy::CommandState::Accepted &&
              optInWorld.GetCommandResult(generatorPlacement).state == economy::CommandState::Accepted &&
              optInWorld.Snapshot().buildings.size() == 4,
          "valid placement commands apply and populate the opt-in world");
    Check(optInWorld.Snapshot().population == 0,
          "food-rule residents are assigned at the first authored food interval");
    Check(optInWorld.Step(12).accepted, "opt-in world advances across multiple food intervals");
    std::uint64_t stockedFood = 0;
    for (const auto &building : optInWorld.Snapshot().buildings) {
        for (const auto &inventory : building.inventory) {
            if (inventory.resource == optInBase.Food().resource) {
                stockedFood += inventory.amount;
            }
        }
    }
    Check(stockedFood > 0 && optInWorld.Snapshot().population == 5,
          "world retains produced food and resident state after food consumption intervals");
    Check(optInWorld.ReplaceCatalog(optInTuned, error),
          "Lua-tuned catalog with preserved opt-ins replaces the active catalog: " + error);
    const std::string optInSave = economy::SaveWorld(optInWorld);
    economy::World optInRestored(optInTuned, 1, 1);
    economy::ParseError optInLoadError;
    Check(!optInSave.empty() && economy::LoadWorld(optInSave, optInTuned, optInRestored, optInLoadError) &&
              optInLoadError.code == economy::ParseErrorCode::None &&
              optInRestored.Snapshot().stateHash == optInWorld.Snapshot().stateHash &&
              economy::SaveWorld(optInRestored) == optInSave,
          "world after Lua catalog replacement saves and reloads against the tuned catalog");
    Check(optInWorld.Step(5).accepted && optInRestored.Step(5).accepted &&
              optInRestored.Snapshot().stateHash == optInWorld.Snapshot().stateHash,
          "save-loaded population and food state continues deterministically");
    if (failures != 0) {
        std::cerr << failures << " economy-rules test(s) failed\n";
        return 1;
    }
    std::cout << "All economy-rules tests passed\n";
    return 0;
}
