#pragma once

#include "openrepublic/economy/economy.h"

#include <string>
#include <vector>

namespace openrepublic::economy_api {

struct SummaryStock {
    std::string resource;
    std::uint64_t amount{};
};
struct SummaryUnservedInput {
    std::string resource;
    std::string buildingType;
    economy::BuildingId building{};
    std::uint64_t held{};
    std::uint64_t needed{};
};
struct SummaryConstructionSite { economy::BuildingId building{}; std::string buildingType, state; double progress{}; };
struct EconomySummary {
    std::uint64_t tick{};
    std::uint64_t population{};
    std::uint64_t happyResidents{};
    std::uint32_t happiness{};
    bool citizensEnabled{};
    std::uint64_t workersAssigned{};
    std::uint64_t workersRequired{};
    bool populationEnabled{};
    bool workforceEnabled{};
    bool calendarEnabled{};
    std::uint32_t calendarYear{}, calendarDay{}, calendarMonth{};
    std::string calendarSeason;
    std::vector<SummaryStock> stock;
    std::vector<SummaryUnservedInput> unservedInputs;
    std::vector<SummaryConstructionSite> constructionSites;
    std::size_t vehicles{};
    std::size_t fleetVehicles{};
    std::size_t parkedFleetVehicles{};
    std::size_t dispatchedFleetVehicles{};
    std::string text;
};

/** Builds a deterministic presentation summary from one snapshot and its catalog.
 * Population and workforce values are active only for catalogs that declare them. */
EconomySummary MakeEconomySummary(const economy::Catalog& catalog,
    const economy::WorldSnapshot& snapshot);

} // namespace openrepublic::economy_api
