#include "openrepublic/economy_api/economy_api.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace openrepublic;

namespace {
void Check(bool ok, const char *label) {
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

economy::Catalog MakeCatalog() {
    economy::CatalogBuilder builder;
    economy::Catalog catalog;
    economy::CatalogErrorInfo error;
    economy::ResourceId electricity{}, product{};
    economy::BuildingTypeId generator{}, consumer{};
    Check(builder.AddUtilityResource("electricity", &electricity, error), "add electricity utility");
    Check(builder.AddResource("product", &product, error), "add ordinary product");
    Check(builder.AddBuildingType("generator", 1, 1, &generator, error), "add generator");
    Check(builder.AddBuildingType("consumer", 1, 1, &consumer, error), "add consumer");
    Check(builder.AddStorage(consumer, product, 8, error), "add consumer product storage");
    Check(builder.AddRecipe(generator, {"generate", 1, {}, {{electricity, 3}}}, error),
          "add generator recipe");
    Check(builder.AddRecipe(consumer, {"consume", 1, {{electricity, 2}}, {{product, 1}}}, error),
          "add utility consumer recipe");
    Check(builder.Build(catalog, error), "build diagnostic fixture catalog");
    return catalog;
}

economy::Catalog MakeWideAmountCatalog() {
    economy::CatalogBuilder builder;
    economy::Catalog catalog;
    economy::CatalogErrorInfo error;
    economy::ResourceId power{};
    economy::BuildingTypeId generator{};
    Check(builder.AddUtilityResource("power", &power, error), "add wide power utility");
    Check(builder.AddBuildingType("generator", 1, 1, &generator, error), "add wide generator");
    Check(builder.AddRecipe(generator,
              {"generate", 1, {}, {{power, std::numeric_limits<std::uint64_t>::max()}}}, error),
          "add maximum-width generator recipe");
    Check(builder.Build(catalog, error), "build wide amount fixture catalog");
    return catalog;
}

economy::Catalog MakeTopologyCatalog() {
    economy::CatalogBuilder builder;
    economy::Catalog catalog;
    economy::CatalogErrorInfo error;
    economy::ResourceId power{}, product{};
    economy::BuildingTypeId node{};
    Check(builder.AddUtilityResource("power", &power, error), "add topology power utility");
    Check(builder.AddResource("product", &product, error), "add topology product resource");
    Check(builder.AddBuildingType("node", 1, 1, &node, error), "add topology node");
    Check(builder.AddStorage(node, product, 1, error), "add topology product storage");
    Check(builder.AddRecipe(node, {"consume", 1, {{power, 1}}, {{product, 1}}}, error),
          "add topology utility consumer recipe");
    Check(builder.Build(catalog, error), "build topology fixture catalog");
    return catalog;
}
}

int main() {
    auto catalog = MakeCatalog();
    economy::World world(catalog, 12, 8);
    economy_api::EconomyApi api(world, {true, true});
    auto initial = api.QueryUtilityDiagnostics(world.Revision(), 0, 1);
    Check(initial.error == economy_api::ErrorCode::None && !initial.available && initial.tick == 0 &&
          initial.total == 0 && initial.networks.empty(),
          "diagnostics before the first completed tick are explicitly unavailable");

    Check(api.QueryUtilityDiagnostics(world.Revision(), 0, 0).error ==
              economy_api::ErrorCode::InvalidPageLimit,
          "zero diagnostic page limit is rejected");
    Check(api.QueryUtilityDiagnostics(world.Revision(), 0,
              economy_api::MaxUtilityDiagnosticsPageSize + 1).error ==
              economy_api::ErrorCode::InvalidPageLimit,
          "oversized diagnostic page limit is rejected");

    const auto deniedWorld = api.QueryWorld();
    economy_api::EconomyApi observer(world, {false, false});
    Check(observer.QueryUtilityDiagnostics(deniedWorld.world.revision, 0, 1).error ==
              economy_api::ErrorCode::PermissionDenied,
          "query-disabled API cannot inspect utility diagnostics");

    const auto placeGenerator = api.PlaceBuilding(world.Revision(), "generator", {1, 2});
    const auto placeFirst = api.PlaceBuilding(world.Revision(), "consumer", {4, 2});
    const auto placeSecond = api.PlaceBuilding(world.Revision(), "consumer", {8, 2});
    Check(placeGenerator.error == economy_api::ErrorCode::None &&
          placeFirst.error == economy_api::ErrorCode::None &&
          placeSecond.error == economy_api::ErrorCode::None &&
          api.Step() == economy_api::ErrorCode::None,
          "diagnostic fixture buildings place");
    auto view = api.QueryWorld();
    const auto generator = view.world.buildings[0].id;
    const auto first = view.world.buildings[1].id;
    const auto second = view.world.buildings[2].id;
    const auto link = api.ConnectUtility(view.world.revision, "electricity", generator, first);
    Check(link.error == economy_api::ErrorCode::None && api.Step() == economy_api::ErrorCode::None,
          "utility component connects");
    Check(api.Step() == economy_api::ErrorCode::None, "utility flow measurement tick completes");

    view = api.QueryWorld();
    const auto beforeQueries = economy::SaveWorld(world);
    const auto stale = api.QueryUtilityDiagnostics(view.world.revision + 1, 0, 1);
    Check(stale.error == economy_api::ErrorCode::StaleRevision,
          "diagnostic query rejects stale world revision");

    auto firstPage = api.QueryUtilityDiagnostics(view.world.revision, 0, 1);
    Check(firstPage.error == economy_api::ErrorCode::None && firstPage.available &&
          firstPage.tick == world.Tick() && firstPage.revision == view.world.revision &&
          firstPage.offset == 0 && firstPage.total == 2 && firstPage.hasMore &&
          firstPage.networks.size() == 1,
          "diagnostics return measured tick metadata and bounded first page");
    Check(firstPage.networks[0].resource == "electricity" &&
          firstPage.networks[0].component == generator &&
          firstPage.networks[0].members.size() == 2 &&
          firstPage.networks[0].members[0] == generator &&
          firstPage.networks[0].members[1] == first &&
          firstPage.networks[0].memberCount == 2 && firstPage.networks[0].membersComplete &&
          firstPage.networks[0].generated == "3" &&
          firstPage.networks[0].requested == "2" &&
          firstPage.networks[0].consumed == "2" &&
          firstPage.networks[0].unused == "1" &&
          firstPage.networks[0].starvedRecipes == 0,
          "connected network exposes exact named identities and conserved flow amounts");
    const auto secondPage = api.QueryUtilityDiagnostics(view.world.revision, 1, 1);
    Check(secondPage.error == economy_api::ErrorCode::None && secondPage.available &&
          secondPage.offset == 1 && secondPage.total == 2 && !secondPage.hasMore &&
          secondPage.networks.size() == 1 && secondPage.networks[0].component == second &&
          secondPage.networks[0].generated == "0" && secondPage.networks[0].requested == "2" &&
          secondPage.networks[0].consumed == "0" && secondPage.networks[0].unused == "0" &&
          secondPage.networks[0].memberCount == 1 && secondPage.networks[0].membersComplete &&
          secondPage.networks[0].starvedRecipes == 1,
          "disconnected consumer reports zero supply and one starved recipe");
    const auto selectedBuilding = api.QueryUtilityDiagnostics(view.world.revision, 0, 1, first);
    Check(selectedBuilding.error == economy_api::ErrorCode::None && selectedBuilding.available &&
          selectedBuilding.total == 1 && selectedBuilding.networks.size() == 1 &&
          selectedBuilding.networks[0].component == generator,
          "building filter returns only its exact connected component");
    Check(api.QueryUtilityDiagnostics(view.world.revision, 0, 1, 0).error ==
              economy_api::ErrorCode::InvalidArgument,
          "unknown building ID is rejected distinctly from an empty filter");
    Check(api.QueryUtilityDiagnostics(view.world.revision + 1, 0, 1, first).error ==
              economy_api::ErrorCode::StaleRevision,
          "building-filtered query still enforces revision checks");
    Check(api.QueryUtilityDiagnostics(view.world.revision, 0, 1, second + 100).error ==
              economy_api::ErrorCode::InvalidArgument,
          "building filter rejects unknown live IDs");
    Check(api.QueryUtilityDiagnostics(view.world.revision, 3, 1).error ==
              economy_api::ErrorCode::InvalidArgument,
          "diagnostic offset beyond collection is rejected");
    Check(economy::SaveWorld(world) == beforeQueries && api.QueryWorld().world.revision == view.world.revision,
          "diagnostic success and error paths do not mutate simulation state");

    const auto saved = economy::SaveWorld(world);
    economy::World restored(catalog, 1, 1);
    economy::ParseError parseError;
    Check(economy::LoadWorld(saved, catalog, restored, parseError), "restore measured utility world");
    economy_api::EconomyApi restoredApi(restored, {true, true});
    const auto afterRestore = restoredApi.QueryUtilityDiagnostics(restored.Revision(), 0, 1);
    Check(afterRestore.error == economy_api::ErrorCode::None && !afterRestore.available &&
          afterRestore.total == 0,
          "volatile utility observation history is unavailable after world restore");
    const auto restoredBuildings = restored.Snapshot().buildings;
    const auto unavailableFilter = restoredApi.QueryUtilityDiagnostics(restored.Revision(), 0, 1,
        restoredBuildings.back().id);
    Check(unavailableFilter.error == economy_api::ErrorCode::None && !unavailableFilter.available &&
          unavailableFilter.total == 0 && unavailableFilter.networks.empty(),
          "known building filter remains valid when volatile history is unavailable");

    auto replacement = MakeCatalog();
    std::string replacementError;
    Check(api.ReplaceCatalog(replacement, replacementError) == economy_api::ErrorCode::None,
          "same-structure catalog replacement succeeds");
    const auto afterReplacement = api.QueryUtilityDiagnostics(world.Revision(), 0, 1);
    Check(afterReplacement.error == economy_api::ErrorCode::None && !afterReplacement.available &&
          afterReplacement.total == 0,
          "catalog replacement invalidates prior utility observation history");

    auto wideCatalog = MakeWideAmountCatalog();
    economy::World wideWorld(wideCatalog, 8, 8);
    economy_api::EconomyApi wideApi(wideWorld, {true, true});
    Check(wideApi.PlaceBuilding(0, "generator", {1, 2}).error == economy_api::ErrorCode::None &&
          wideApi.PlaceBuilding(0, "generator", {5, 2}).error == economy_api::ErrorCode::None &&
          wideApi.Step() == economy_api::ErrorCode::None,
          "place two maximum-width generators");
    const auto wideBuildings = wideApi.QueryWorld().world.buildings;
    const auto wideLink = wideApi.ConnectUtility(wideWorld.Revision(), "power",
        wideBuildings[0].id, wideBuildings[1].id);
    Check(wideLink.error == economy_api::ErrorCode::None && wideApi.Step() == economy_api::ErrorCode::None &&
          wideApi.Step() == economy_api::ErrorCode::None,
          "connect wide generators and measure combined flow");
    const auto widePage = wideApi.QueryUtilityDiagnostics(wideWorld.Revision(), 0, 1);
    Check(widePage.error == economy_api::ErrorCode::None && widePage.total == 1 &&
          widePage.networks[0].generated == "36893488147419103230" &&
          widePage.networks[0].memberCount == 2 && widePage.networks[0].membersComplete &&
          widePage.networks[0].unused == "36893488147419103230" &&
          widePage.networks[0].consumed == "0",
          "API formats utility amounts above uint64 as exact decimal strings");

    auto topologyCatalog = MakeTopologyCatalog();
    economy::World topologyWorld(topologyCatalog, 20, 20);
    economy_api::EconomyApi topologyApi(topologyWorld, {true, true});
    constexpr std::size_t topologyMembers = economy_api::MaxUtilityDiagnosticMembers + 2;
    for (std::size_t index = 0; index < topologyMembers; ++index) {
        const auto placed = topologyApi.PlaceBuilding(topologyWorld.Revision(), "node",
            {static_cast<std::int32_t>(index % 20),
             static_cast<std::int32_t>(1 + (index / 20) * 2)});
        Check(placed.error == economy_api::ErrorCode::None, "queue large topology node");
    }
    Check(topologyApi.Step() == economy_api::ErrorCode::None,
          "place all nodes for bounded membership sample test");
    const auto nodes = topologyApi.QueryWorld().world.buildings;
    for (std::size_t index = 1; index < nodes.size(); ++index) {
        const auto linked = topologyApi.ConnectUtility(topologyWorld.Revision(), "power",
            nodes[index - 1].id, nodes[index].id);
        Check(linked.error == economy_api::ErrorCode::None, "queue large topology connection");
    }
    Check(topologyApi.Step() == economy_api::ErrorCode::None &&
          topologyApi.Step() == economy_api::ErrorCode::None,
          "apply large topology and collect current utility measurement");
    const auto topologyView = topologyApi.QueryWorld().world;
    const auto topologySaveBefore = economy::SaveWorld(topologyWorld);
    const auto topologyPage = topologyApi.QueryUtilityDiagnostics(topologyView.revision, 0, 1);
    Check(topologyPage.error == economy_api::ErrorCode::None && topologyPage.total == 1 &&
          topologyPage.networks.size() == 1 &&
          topologyPage.networks[0].memberCount == topologyMembers &&
          topologyPage.networks[0].members.size() == economy_api::MaxUtilityDiagnosticMembers &&
          !topologyPage.networks[0].membersComplete &&
          topologyPage.networks[0].members.front() == nodes.front().id &&
          topologyPage.networks[0].members.back() ==
              nodes[economy_api::MaxUtilityDiagnosticMembers - 1].id,
          "large diagnostic component returns a bounded prefix and explicit full member count");
    const auto outsideSampleId = nodes.back().id;
    Check(std::find(topologyPage.networks[0].members.begin(), topologyPage.networks[0].members.end(),
              outsideSampleId) == topologyPage.networks[0].members.end(),
          "selected test building falls outside the public member sample");
    const auto outsideSampleFilter = topologyApi.QueryUtilityDiagnostics(topologyView.revision, 0, 1,
        outsideSampleId);
    Check(outsideSampleFilter.error == economy_api::ErrorCode::None &&
          outsideSampleFilter.total == 1 && outsideSampleFilter.networks.size() == 1 &&
          outsideSampleFilter.networks[0].component == nodes.front().id &&
          outsideSampleFilter.networks[0].memberCount == topologyMembers,
          "building filter searches complete core membership beyond the bounded API sample");
    Check(economy::SaveWorld(topologyWorld) == topologySaveBefore &&
          topologyApi.QueryWorld().world.stateHash == topologyView.stateHash &&
          topologyApi.QueryWorld().world.revision == topologyView.revision,
          "large topology query and full-membership filter preserve world hash and revision");

    std::cout << "Economy utility diagnostics API contracts passed\n";
}
