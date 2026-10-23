#include "openrepublic/economy_api/economy_summary.h"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <tuple>

namespace openrepublic::economy_api {
namespace {
std::uint64_t SaturatingAdd(std::uint64_t a, std::uint64_t b) {
    return b > std::numeric_limits<std::uint64_t>::max() - a
        ? std::numeric_limits<std::uint64_t>::max() : a + b;
}
}

EconomySummary MakeEconomySummary(const economy::Catalog& catalog,
    const economy::WorldSnapshot& snapshot) {
    EconomySummary result;
    result.tick = snapshot.tick;
    result.vehicles = snapshot.vehicles.size();
    std::map<economy::ResourceId, std::uint64_t> totals;
    for (const auto& building : snapshot.buildings)
        for (const auto& item : building.inventory)
            totals[item.resource] = SaturatingAdd(totals[item.resource], item.amount);
    for (const auto& resource : catalog.Resources()) {
        if (!resource.utility)
            result.stock.push_back({resource.name, totals[resource.id]});
    }

    // Only buildings explicitly stalled for missing material are counted. For each,
    // report recipe inputs whose currently held stock is below one recipe cycle.
    for (const auto& building : snapshot.buildings) {
        if (building.stall.reason != economy::StallReason::MissingInput)
            continue;
        const auto* type = catalog.BuildingType(building.type);
        if (!type)
            continue;
        std::map<economy::ResourceId, std::uint64_t> held;
        for (const auto& item : building.inventory)
            held[item.resource] = SaturatingAdd(held[item.resource], item.amount);
        if (building.stall.recipeIndex >= type->recipes.size())
            continue;
        const auto& recipe = type->recipes[building.stall.recipeIndex];
        for (const auto& input : recipe.inputs) {
            if (input.resource != building.stall.resource)
                continue;
            const auto amount = held[input.resource];
            if (amount >= input.amount)
                continue;
            const auto* resource = catalog.Resource(input.resource);
            if (resource)
                result.unservedInputs.push_back({resource->name, type->name,
                    building.id, amount, input.amount});
        }
    }
    std::sort(result.unservedInputs.begin(), result.unservedInputs.end(),
        [](const auto& a, const auto& b) {
            const auto aMissing = a.needed - a.held;
            const auto bMissing = b.needed - b.held;
            if (aMissing != bMissing) return aMissing > bMissing;
            return std::tie(a.resource, a.building, a.buildingType) <
                   std::tie(b.resource, b.building, b.buildingType);
        });
    if (result.unservedInputs.size() > 5)
        result.unservedInputs.resize(5);

    std::ostringstream text;
    text << "Tick: " << result.tick << '\n'
         << "Population: unavailable\n"
         << "Workers: unavailable\n"
         << "Stock:";
    for (const auto& item : result.stock)
        text << '\n' << "  " << item.resource << ": " << item.amount;
    text << '\n' << "Unserved inputs (top 5):";
    if (result.unservedInputs.empty())
        text << " none";
    for (const auto& item : result.unservedInputs)
        text << '\n' << "  " << item.resource << " at " << item.buildingType
             << "#" << item.building << ": " << item.held << "/" << item.needed;
    text << '\n' << "Vehicles: " << result.vehicles;
    result.text = text.str();
    return result;
}
} // namespace openrepublic::economy_api
