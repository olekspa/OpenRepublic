#pragma once

#include "openrepublic/economy/economy.h"

#include <optional>
#include <string>
#include <vector>

namespace openrepublic::runtime
{
	struct EconomyOverviewResource
	{
		economy::ResourceId id{};
		std::string name;
		std::uint64_t stored{};
		std::optional<double> perTickDelta;
	};

	struct EconomyOverviewConstructionSite
	{
		economy::BuildingId id{};
		std::string type;
		std::uint8_t state{};
		double progress{};
	};

	struct EconomyOverviewHaul
	{
		economy::OrderId id{};
		economy::BuildingId source{}, destination{};
		economy::VehicleId vehicle{};
		std::string resource;
		std::uint64_t cargo{};
		economy::HaulState state{economy::HaulState::Idle};
	};

	struct EconomyOverview
	{
		std::uint64_t tick{};
		std::uint64_t treasury{};
		bool populationAvailable{};
		std::uint64_t population{}, housingCapacity{}, workersAssigned{}, workersRequired{};
		std::size_t truckCount{};
		std::uint64_t heatProduced{};
		std::vector<EconomyOverviewResource> resources;
		std::vector<EconomyOverviewConstructionSite> constructionSites;
		std::vector<EconomyOverviewHaul> activeHauls;
	};

	/** Builds a presentation-neutral view from authoritative snapshots. Rates use the
	 *  tick distance between snapshots; missing, reversed, or same-tick baselines
	 *  leave perTickDelta unset. Utility resources are excluded from stored totals. */
	EconomyOverview BuildEconomyOverview(const economy::WorldSnapshot& current,
		const economy::Catalog& catalog,
		const std::optional<economy::WorldSnapshot>& baseline = std::nullopt);
}
