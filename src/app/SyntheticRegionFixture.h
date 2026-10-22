#pragma once

#include "EconomyOverlay.h"
#include "openrepublic/compat/region_roads.h"

#include <string>
#include <vector>

namespace openrepublic::preview
{
	// OpenRepublic-authored scene and plain position data for exercising the
	// app-owned region economy without loading any WRSR files or assets.
	struct SyntheticRegionFixtureData
	{
		std::vector<RegionInstance> instances;
		std::vector<openrepublic::compat::PlacedBuildingRef> placements;
		std::vector<openrepublic::compat::RoadLinkRef> roads;
		std::size_t centerPlacementIndex = 1;
		double radiusMeters = 24.0;
		double centerX = 0.0;
		double centerZ = 0.0;
		DirectX::XMFLOAT3 cameraTarget = DirectX::XMFLOAT3(0, 0, 0);
		float cameraDistance = 40.0f;
		std::string description;
	};

	// Creates a deterministic three-building Farm -> Mill -> Bakery scene and a
	// visible road on OpenRepublic's synthetic ground. Source placement keys match
	// the corresponding entries in `placements` and `instances`.
	bool CreateSyntheticRegionFixture(wi::scene::Scene& scene,
		SyntheticRegionFixtureData& output, std::string& error);
}
