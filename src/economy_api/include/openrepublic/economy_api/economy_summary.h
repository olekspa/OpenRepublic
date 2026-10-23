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
struct EconomySummary {
    std::uint64_t tick{};
    std::vector<SummaryStock> stock;
    std::vector<SummaryUnservedInput> unservedInputs;
    std::size_t vehicles{};
    std::string text;
};

/** Builds a deterministic presentation summary from one snapshot and its catalog.
 * Population and workers are unavailable in the current economy data model. */
EconomySummary MakeEconomySummary(const economy::Catalog& catalog,
    const economy::WorldSnapshot& snapshot);

} // namespace openrepublic::economy_api
