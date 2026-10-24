#include "openrepublic/runtime/native_coal_demo.h"

namespace openrepublic::runtime {

std::optional<CoalDemoAction> NextCoalDemoAction(const economy::WorldSnapshot& world,
    economy::BuildingId border, economy::BuildingId depot, economy::BuildingId storage,
    economy::BuildingId plant, economy::ResourceId coal, std::uint64_t batch) {
    if (!batch || !border || !depot || !storage || !plant || !coal) return std::nullopt;
    const auto building = [&world, coal](economy::BuildingId id) -> const economy::BuildingSnapshot* {
        for (const auto& b : world.buildings) if (b.id == id) return &b;
        return nullptr;
    };
    const auto stock = [coal](const economy::BuildingSnapshot* b) {
        if (!b) return std::uint64_t{};
        for (const auto& slot : b->inventory) if (slot.resource == coal) return slot.amount;
        return std::uint64_t{};
    };
    const auto* borderBuilding = building(border);
    const auto* storageBuilding = building(storage);
    if (!borderBuilding || !storageBuilding || !building(depot) || !building(plant)) return std::nullopt;
    const auto vehicle = [&world, depot](economy::VehicleId id) -> const economy::VehicleSnapshot* {
        for (const auto& v : world.vehicles) if (v.id == id && v.ownerDepot == depot) return &v;
        return nullptr;
    };
    const auto* first = vehicle(1);
    const auto* second = vehicle(2);
    if (!first || !second) return std::nullopt;
    // Import when the border is running low, but avoid piling up inventory.
    if (stock(borderBuilding) < batch && world.treasury >= batch)
        return CoalDemoAction{CoalDemoActionKind::ImportCoal, 0, border, border, batch};
    if (first->order == 0 && first->leg == economy::VehicleLeg::None && stock(borderBuilding) >= batch)
        return CoalDemoAction{CoalDemoActionKind::DispatchToStorage, first->id, border, storage, batch};
    if (second->order == 0 && second->leg == economy::VehicleLeg::None && stock(storageBuilding) >= batch)
        return CoalDemoAction{CoalDemoActionKind::DispatchToPlant, second->id, storage, plant, batch};
    return std::nullopt;
}

} // namespace openrepublic::runtime
