#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace openrepublic::economy;

namespace {
int failures = 0;

void Check(bool condition, const char *message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Catalog MakeCatalog(ResourceId processorInput, ResourceId processorOutput, std::uint32_t interval,
                    std::uint64_t inputAmount, std::uint64_t outputAmount,
                    std::uint64_t vehicleCapacity = 4, std::uint32_t farmInterval = 5) {
    CatalogBuilder builder;
    CatalogErrorInfo error;
    ResourceId grain{}, flour{}, bread{};
    BuildingTypeId farm{}, mill{};
    bool ok = builder.AddResource("grain", &grain, error) &&
              builder.AddResource("flour", &flour, error) &&
              builder.AddResource("bread", &bread, error) &&
              builder.AddBuildingType("farm", 1, 1, &farm, error) &&
              builder.AddBuildingType("mill", 1, 1, &mill, error);
    for (ResourceId resource = grain; ok && resource <= bread; ++resource)
        ok = builder.AddStorage(farm, resource, 100, error) && builder.AddStorage(mill, resource, 100, error);
    ok = ok && builder.AddRecipe(farm, {"harvest", farmInterval, {}, {{grain, 2}}}, error) &&
         builder.AddRecipe(mill, {"mill", interval, {{processorInput, inputAmount}},
                                  {{processorOutput, outputAmount}}}, error) &&
         builder.AddVehicleType("truck", vehicleCapacity, 1, nullptr, error);
    Catalog catalog;
    ok = ok && builder.Build(catalog, error);
    Check(ok, "test catalog builds");
    return catalog;
}

void PlaceAndAdvanceToActiveRecipe(World &world) {
    world.PlaceBuilding(0, {1, 1});
    world.PlaceBuilding(1, {4, 1});
    world.Step();
    for (int i = 0; i < 10; ++i) {
        const auto snapshot = world.Snapshot();
        if (!snapshot.buildings.empty() && snapshot.buildings[0].recipes[0].remainingTicks > 0)
            return;
        world.Step();
    }
}
} // namespace

int main() {
    const auto original = MakeCatalog(0, 1, 5, 1, 1);
    const auto rewiredInput = MakeCatalog(1, 1, 5, 1, 1);
    const auto rewiredOutput = MakeCatalog(0, 2, 5, 1, 1);
    const auto tuned = MakeCatalog(0, 1, 7, 2, 3, 4, 7);
    const auto reducedTruck = MakeCatalog(0, 1, 5, 1, 1, 1);

    World world(original, 12, 8);
    PlaceAndAdvanceToActiveRecipe(world);
    const auto before = world.Snapshot();
    Check(before.buildings.size() == 2 && before.buildings[0].recipes[0].remainingTicks > 0,
          "farm has active recipe progress before reload checks");

    std::string error;
    Check(!world.ReplaceCatalog(rewiredInput, error) && !error.empty(),
          "same-cardinality input resource rewiring is rejected");
    Check(&world.GetCatalog() == &original && world.Snapshot().stateHash == before.stateHash,
          "rejected input rewiring preserves catalog and state hash");

    error.clear();
    Check(!world.ReplaceCatalog(rewiredOutput, error) && !error.empty(),
          "same-cardinality output resource rewiring is rejected");
    Check(&world.GetCatalog() == &original && world.Snapshot().stateHash == before.stateHash,
          "rejected output rewiring preserves catalog and state hash");

    error.clear();
    Check(world.ReplaceCatalog(tuned, error), "same-structure amount and interval tuning is accepted");
    Check(&world.GetCatalog() == &tuned, "world references the tuned catalog after accepted reload");
    const auto after = world.Snapshot();
    Check(after.buildings.size() == before.buildings.size() &&
              after.buildings[0].recipes[0].remainingTicks <= 7,
          "accepted tuning preserves active recipe progress within the new interval");

    World cargoWorld(original, 12, 8);
    for (std::int32_t x = 1; x < 6; ++x)
        cargoWorld.BuildRoad({x, 1}, {x + 1, 1});
    cargoWorld.PlaceBuilding(0, {1, 2});
    cargoWorld.PlaceBuilding(1, {4, 2});
    cargoWorld.Step();
    cargoWorld.CreateHaulOrder(1, 2, 0, 0);
    cargoWorld.Step();
    bool carrying = false;
    for (int i = 0; i < 30 && !carrying; ++i) {
        cargoWorld.Step();
        const auto snapshot = cargoWorld.Snapshot();
        carrying = !snapshot.vehicles.empty() && snapshot.vehicles[0].cargo > 1;
    }
    Check(carrying, "test truck has more cargo than the reduced capacity");
    const auto cargoBefore = cargoWorld.Snapshot();
    error.clear();
    Check(!cargoWorld.ReplaceCatalog(reducedTruck, error) && !error.empty(),
          "replacement truck capacity below onboard cargo is rejected");
    Check(&cargoWorld.GetCatalog() == &original && cargoWorld.Snapshot().stateHash == cargoBefore.stateHash,
          "rejected vehicle-capacity reduction preserves catalog and state hash");

    if (failures) {
        std::cerr << failures << " catalog reload checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All catalog reload structural checks passed.\n";
    return EXIT_SUCCESS;
}
