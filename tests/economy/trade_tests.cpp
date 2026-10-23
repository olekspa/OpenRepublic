#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic::economy;
namespace {
void Check(bool value, const char *message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
Catalog MakeCatalog() {
    Catalog result; ParseError error;
    Check(ParseCatalogText("openrepublic-economy 1\nresource grain\ntrade grain buy 5 sell 3\nbuilding border 1 1\n  store grain 10\n  recipe produce every 1 out grain:4\n  tradepost\nend\n", result, error), "trade catalog parses");
    return result;
}
}
int main() {
    auto catalog = MakeCatalog();
    Check(catalog.Resources()[0].tradeDeclared && catalog.Resources()[0].buyPrice == 5 &&
          catalog.Resources()[0].sellPrice == 3, "catalog retains declared prices");
    Check(catalog.BuildingTypes()[0].tradePost, "catalog retains trade-post flag");
    Catalog roundtrip; ParseError parseError;
    Check(ParseCatalogText(WriteCatalogText(catalog), roundtrip, parseError) &&
          roundtrip.Resources()[0].tradeDeclared && roundtrip.BuildingTypes()[0].tradePost,
          "canonical catalog preserves trade declaration");
    World world(catalog, 8, 8);
    auto place = world.PlaceBuilding(0, {2, 2});
    Check(place && world.Step().accepted, "trade post placed");
    const auto building = world.Snapshot().buildings[0].id;
    auto disable = world.SetBuildingEnabled(building, false);
    Check(disable && world.Step().accepted, "trade post production disabled");
    auto exportId = world.TradeExport(building, 0, 4, 2);
    Check(exportId && world.Step().accepted, "export queued and applied");
    auto snapshot = world.Snapshot();
    Check(world.GetCommandResult(exportId).state == CommandState::Accepted && snapshot.treasury == 6 &&
          snapshot.buildings[0].inventory[0].amount == 2, "export is rate capped and credits treasury");
    auto text = SaveWorld(world);
    Check(text.find("OPENREPUBLIC_ECONOMY_WORLD 5") == 0, "trade world uses save version 5");
    World restored(catalog, 1, 1);
    if (!LoadWorld(text, catalog, restored, parseError)) std::cerr << "load error: " << parseError.message << '\n';
    Check(parseError.code == ParseErrorCode::None, "trade save loads");
    Check(restored.Treasury() == 6 && restored.Snapshot().stateHash == snapshot.stateHash,
          "treasury and hash survive persistence");
    auto importId = restored.TradeImport(building, 0, 1, 1);
    auto pendingSave = SaveWorld(restored);
    World continued(catalog, 1, 1);
    Check(importId && LoadWorld(pendingSave, catalog, continued, parseError),
          "queued trade command survives persistence");
    Check(continued.GetCommandResult(importId).state == CommandState::Queued &&
          continued.Step().accepted && continued.Treasury() == 1,
          "restored import spends treasury at buy price");
    return 0;
}
