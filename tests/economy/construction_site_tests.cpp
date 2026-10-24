#include "openrepublic/economy/economy.h"
#include <cstdlib>
#include <iostream>
using namespace openrepublic::economy;
int main() {
    const std::string text="openrepublic-economy 1\nresource planks\nresource bricks\nbuilding mill 2 2\n  build planks:2 bricks:3 work 4\nend\n";
    Catalog catalog; ParseError error;
    if (!ParseCatalogText(text,catalog,error) || WriteCatalogText(catalog).find("build planks:2 bricks:3 work 4")==std::string::npos) return 1;
    World world(catalog,12,12); const auto ordinary=world.PlaceBuilding(0,{2,2}); world.Step();
    if (world.Snapshot().buildings.size()!=1 || world.Snapshot().buildings[0].constructionSite) return 2;
    const auto siteCommand=world.PlaceConstructionSite(0,{6,2}); if (!siteCommand) return 3; world.Step();
    auto snapshot=world.Snapshot(); if (snapshot.buildings.size()!=2 || !snapshot.buildings[1].constructionSite || snapshot.buildings[1].constructionProgress!=0.0) return 4;
    const auto saved=SaveWorld(world); if (saved.find("WORLD 7")==std::string::npos) return 5;
    World restored(catalog,1,1); if (!LoadWorld(saved,catalog,restored,error)) { std::cerr<<error.message<<"\n"; return 6; }
    if (restored.Snapshot().stateHash!=snapshot.stateHash || !restored.Snapshot().buildings[1].constructionSite) return 7;
    const auto before=world.Snapshot().stateHash; World noSite(catalog,12,12); noSite.PlaceBuilding(0,{2,2}); noSite.Step();
    if (noSite.Snapshot().buildings[0].constructionSite || !ordinary || before==0) return 8;
    std::cout<<"Construction-site catalog, placement, and v7 persistence checks passed\n";
}
