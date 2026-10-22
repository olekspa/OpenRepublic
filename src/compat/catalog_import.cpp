#include "openrepublic/compat/catalog_import.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace openrepublic::compat {
namespace {

constexpr long double kMicroUnitsPerUnit = 1000000.0L;
constexpr long double kUint64ExclusiveLimit = 18446744073709551616.0L;
constexpr std::uint64_t kTruckCapacity = 20000000u;
constexpr std::uint32_t kTruckSpeed = 2;

using RatesByResource = std::map<std::string, long double>;

struct BuildingRates {
    RatesByResource productionPerDay;
    RatesByResource consumptionPerDay;
    RatesByResource productionPerTick;
    RatesByResource consumptionPerTick;
};

bool ValidName(const std::string &name) {
    if (name.empty() || name.size() > 96)
        return false;
    for (const char value : name)
        if (value == '#' || value == ' ' || value == '\t' || value == '\r' || value == '\n')
            return false;
    return true;
}

bool ToRoundedAmount(long double value, std::uint64_t &amount, bool &roundedUp) {
    if (!std::isfinite(value) || value < 0.0L || value >= kUint64ExclusiveLimit)
        return false;
    const long double rounded = std::floor(value + 0.5L);
    if (rounded >= kUint64ExclusiveLimit)
        return false;
    roundedUp = rounded > value;
    amount = static_cast<std::uint64_t>(rounded);
    if (value > 0.0L && amount == 0) {
        amount = 1;
        roundedUp = true;
    }
    return true;
}

bool ToCapacity(long double value, std::uint64_t &capacity) {
    if (!std::isfinite(value) || value < 0.0L || value >= kUint64ExclusiveLimit)
        return false;
    const long double rounded = std::ceil(value);
    if (rounded >= kUint64ExclusiveLimit)
        return false;
    capacity = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(rounded));
    return true;
}

void AddSkipped(ImportReport &report, const std::string &name, const std::string &reason) {
    report.skippedBuildings.push_back({name, reason});
    ++report.buildingsSkipped;
}

bool ImportRates(const content::BuildingDefinition &definition, std::uint32_t ticksPerDay,
                 const std::string &buildingName, BuildingRates &rates,
                 std::string &reason, std::vector<std::string> &warnings) {
    auto addRate = [&](const content::BuildingResourceRate &entry, bool production,
                       bool perSecond) {
        if (!ValidName(entry.resource)) {
            reason = "invalid resource name '" + entry.resource + "' at line " +
                     std::to_string(entry.line);
            return false;
        }
        if (!std::isfinite(entry.value) || entry.value < 0.0) {
            reason = "invalid negative or non-finite rate for resource '" + entry.resource +
                     "' at line " + std::to_string(entry.line);
            return false;
        }
        const long double workers = entry.workersDeclaredBefore
                                        ? static_cast<long double>(
                                              definition.workersNeeded.value_or(0.0))
                                        : 1.0L;
        if (workers <= 0.0L || !std::isfinite(workers)) {
            reason = "workersDeclaredBefore has no positive finite workersNeeded value";
            return false;
        }
        const long double secondsFactor = perSecond ? 60.0L : 1.0L;
        const long double perDay = static_cast<long double>(entry.value) * secondsFactor * workers *
                                   kMicroUnitsPerUnit;
        if (!std::isfinite(perDay)) {
            reason = "rate overflow for resource '" + entry.resource + "'";
            return false;
        }
        auto &daily = production ? rates.productionPerDay : rates.consumptionPerDay;
        auto &tickly = production ? rates.productionPerTick : rates.consumptionPerTick;
        daily[entry.resource] += perDay;
        tickly[entry.resource] += perDay / static_cast<long double>(ticksPerDay);
        if (!std::isfinite(daily[entry.resource]) || !std::isfinite(tickly[entry.resource])) {
            reason = "combined rate overflow for resource '" + entry.resource + "'";
            return false;
        }
        return true;
    };

    for (const auto &entry : definition.production)
        if (!addRate(entry, true, false))
            return false;
    for (const auto &entry : definition.consumption)
        if (!addRate(entry, false, false))
            return false;
    for (const auto &entry : definition.consumptionPerSecond)
        if (!addRate(entry, false, true))
            return false;

    auto recordTiny = [&](const RatesByResource &tickly, const char *direction) {
        for (const auto &entry : tickly) {
            if (entry.second > 0.0L && entry.second < 0.5L) {
                warnings.push_back("building '" + buildingName + "' " + direction + " resource '" +
                                   entry.first + "' rounded up to 1 micro-unit per tick");
            }
        }
    };
    recordTiny(rates.productionPerTick, "production");
    recordTiny(rates.consumptionPerTick, "consumption");
    return true;
}

bool BuildRecipeAmounts(const RatesByResource &rates,
                        const std::string &buildingName, const char *direction,
                        std::vector<economy::RecipeAmount> &amounts,
                        std::map<std::string, std::uint64_t> &amountsByName,
                        const std::map<std::string, economy::ResourceId> &resourceIds,
                        std::vector<std::string> &warnings, std::string &reason) {
    for (const auto &entry : rates) {
        std::uint64_t amount = 0;
        bool roundedUp = false;
        if (!ToRoundedAmount(entry.second, amount, roundedUp)) {
            reason = std::string(direction) + " rate overflows micro-unit storage for resource '" +
                     entry.first + "'";
            return false;
        }
        if (amount == 0)
            continue;
        amountsByName[entry.first] = amount;
        amounts.push_back({resourceIds.at(entry.first), amount});
        if (roundedUp && entry.second >= 0.5L) {
            warnings.push_back("building '" + buildingName + "' " + direction + " resource '" +
                               entry.first + "' rounded up to an integer micro-unit amount");
        }
    }
    return true;
}

} // namespace

bool ImportCatalogFromDefinitions(
    const std::map<std::string, content::BuildingDefinition> &definitions,
    const CatalogImportOptions &options, economy::Catalog &out, ImportReport &report) {
    ImportReport next;
    next.options = options;
    next.definitionsSeen = definitions.size();
    report = next;
    auto failImport = [&](const std::string &message) {
        next.success = false;
        next.error = message;
        std::sort(next.warnings.begin(), next.warnings.end());
        report = std::move(next);
        return false;
    };
    if (!options.ticksPerDay)
        return failImport("ticksPerDay must be positive");
    if (!options.defaultFootprint)
        return failImport("defaultFootprint must be positive");
    if (!std::isfinite(options.storageDaysFallback) || options.storageDaysFallback <= 0.0)
        return failImport("storageDaysFallback must be finite and positive");

    std::set<std::string> resourceNames;
    for (const auto &building : definitions) {
        const auto &definition = building.second;
        next.resourceSpecificStorageEntriesSeen += static_cast<std::size_t>(std::count_if(
            definition.storage.begin(), definition.storage.end(),
            [](const content::BuildingStorageEntry &entry) { return !entry.resource.empty(); }));
        auto collect = [&](const std::vector<content::BuildingResourceRate> &entries) {
            for (const auto &entry : entries)
                if (ValidName(entry.resource))
                    resourceNames.insert(entry.resource);
        };
        collect(definition.production);
        collect(definition.consumption);
        collect(definition.consumptionPerSecond);
        for (const auto &entry : definition.storage)
            if (!entry.resource.empty() && ValidName(entry.resource))
                resourceNames.insert(entry.resource);
    }

    economy::CatalogBuilder builder;
    economy::CatalogErrorInfo catalogError;
    std::map<std::string, economy::ResourceId> resourceIds;
    for (const auto &name : resourceNames) {
        economy::ResourceId id{};
        if (!builder.AddResource(name, &id, catalogError))
            return failImport("could not add resource '" + name + "': " + catalogError.message);
        resourceIds.emplace(name, id);
        ++next.resourcesImported;
    }

    for (const auto &building : definitions) {
        const auto &name = building.first;
        const auto &definition = building.second;
        const bool hasEconomics = !definition.production.empty() || !definition.consumption.empty() ||
                                  !definition.consumptionPerSecond.empty();
        if (!hasEconomics) {
            AddSkipped(next, name, "no production or consumption entries");
            continue;
        }
        if (!ValidName(name)) {
            AddSkipped(next, name, "invalid building name for economy catalog");
            continue;
        }
        const auto invalidStorageResource = std::find_if(
            definition.storage.begin(), definition.storage.end(),
            [](const content::BuildingStorageEntry &entry) {
                return !entry.resource.empty() && !ValidName(entry.resource);
            });
        if (invalidStorageResource != definition.storage.end()) {
            AddSkipped(next, name, "invalid resource name '" + invalidStorageResource->resource +
                                       "' in resource-bearing storage entry at line " +
                                       std::to_string(invalidStorageResource->line));
            continue;
        }
        BuildingRates rates;
        std::string reason;
        if (!ImportRates(definition, options.ticksPerDay, name, rates, reason, next.warnings)) {
            AddSkipped(next, name, reason);
            continue;
        }

        std::vector<economy::RecipeAmount> inputs;
        std::vector<economy::RecipeAmount> outputs;
        std::map<std::string, std::uint64_t> inputAmounts;
        std::map<std::string, std::uint64_t> outputAmounts;
        // Recipes use per-tick rates. The rate helper preserves exact fractions until this point.
        if (!BuildRecipeAmounts(rates.consumptionPerTick, name, "consumption", inputs,
                                inputAmounts, resourceIds, next.warnings, reason) ||
            !BuildRecipeAmounts(rates.productionPerTick, name, "production", outputs,
                                outputAmounts, resourceIds, next.warnings, reason)) {
            AddSkipped(next, name, reason);
            continue;
        }
        if (inputs.empty() && outputs.empty()) {
            AddSkipped(next, name, "all economic rates are zero at micro-unit precision");
            continue;
        }

        economy::BuildingTypeId buildingId{};
        if (!builder.AddBuildingType(name, options.defaultFootprint, options.defaultFootprint,
                                     &buildingId, catalogError))
            return failImport("could not add building '" + name + "': " + catalogError.message);

        std::set<std::string> storedNames;
        for (const auto &entry : inputAmounts)
            storedNames.insert(entry.first);
        for (const auto &entry : outputAmounts)
            storedNames.insert(entry.first);
        for (const auto &resource : storedNames) {
            const long double productionDay = rates.productionPerDay.count(resource)
                                                  ? rates.productionPerDay.at(resource)
                                                  : 0.0L;
            const long double consumptionDay = rates.consumptionPerDay.count(resource)
                                                   ? rates.consumptionPerDay.at(resource)
                                                   : 0.0L;
            const long double largerDay = std::max(productionDay, consumptionDay);
            const std::uint64_t productionTick = outputAmounts.count(resource)
                                                     ? outputAmounts.at(resource)
                                                     : 0;
            const std::uint64_t consumptionTick = inputAmounts.count(resource)
                                                      ? inputAmounts.at(resource)
                                                      : 0;
            const long double fallback = static_cast<long double>(options.storageDaysFallback) * largerDay;
            const long double tickMinimum = static_cast<long double>(
                                                std::max(productionTick, consumptionTick)) *
                                            4.0L;
            std::uint64_t capacity = 0;
            if (!ToCapacity(std::max(fallback, tickMinimum), capacity))
                return failImport("storage capacity overflow for building '" + name +
                                  "', resource '" + resource + "'");
            if (!builder.AddStorage(buildingId, resourceIds.at(resource), capacity, catalogError))
                return failImport("could not add storage for building '" + name + "': " +
                                  catalogError.message);
        }
        if (!builder.AddRecipe(buildingId, {"operate", 1, inputs, outputs}, catalogError))
            return failImport("could not add recipe for building '" + name + "': " +
                              catalogError.message);
        ++next.buildingsImported;
    }

    if (!builder.AddVehicleType("truck", kTruckCapacity, kTruckSpeed, nullptr, catalogError))
        return failImport("could not add OpenRepublic default truck: " + catalogError.message);
    economy::Catalog candidate;
    if (!builder.Build(candidate, catalogError))
        return failImport("could not build catalog: " + catalogError.message);
    out = std::move(candidate);
    next.success = true;
    next.error.clear();
    std::sort(next.warnings.begin(), next.warnings.end());
    report = std::move(next);
    return true;
}

std::string DescribeImport(const ImportReport &report) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "Compatibility catalog import: " << (report.success ? "success" : "failed") << "\n";
    output << "Definitions: " << report.definitionsSeen << ", resources: " << report.resourcesImported
           << ", buildings imported: " << report.buildingsImported << ", skipped: "
           << report.buildingsSkipped << "\n";
    output << "Resource-specific storage entries observed: "
           << report.resourceSpecificStorageEntriesSeen << "\n";
    output << "Options: ticksPerDay=" << report.options.ticksPerDay
           << ", defaultFootprint=" << report.options.defaultFootprint
           << ", storageDaysFallback="
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << report.options.storageDaysFallback << "\n";
    output << "OBSERVED in one reviewed build: worker scaling depends on declaration order and "
              "per-second input is multiplied by 60. Rate time base and fallback capacity are "
              "INFERRED; explicit storage resource mapping is UNKNOWN. Truck capacity/speed and "
              "footprint are OpenRepublic defaults.\n";
    if (!report.error.empty())
        output << "Error: " << report.error << "\n";
    for (const auto &skipped : report.skippedBuildings)
        output << "Skipped " << skipped.name << ": " << skipped.reason << "\n";
    for (const auto &warning : report.warnings)
        output << "Warning: " << warning << "\n";
    return output.str();
}

} // namespace openrepublic::compat
