#pragma once

#include "EconomyOverlay.h"
#include "WickedEngine.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/terrain_preview.h"

#include <cstddef>
#include <string>
#include <vector>

namespace openrepublic::preview
{
	// OpenRepublic-authored presentation of the surroundings of an imported map region: the real
	// heightmap extended to a wide landscape, shaded terrain, a lake level, and procedurally
	// scattered trees. None of it is WRSR content or an interpretation of the map's masks; it
	// only uses the verified height data and OpenRepublic's own procedural assets.
	struct ShowcaseEnvironmentInfo
	{
		bool hasWater = false;
		float waterLevel = 0.0f;
		float terrainMinimum = 0.0f;
		float terrainMaximum = 0.0f;
		std::size_t nearTrees = 0;
		std::size_t farTrees = 0;
		std::string summary;
	};

	// Height data around the region, sampled once and reused by the near and far meshes so both
	// are shaded from the same elevation range and water level.
	struct ShowcaseTerrainData
	{
		openrepublic::content::TerrainPreviewGrid grid;
		double centerX = 0.0;
		double centerZ = 0.0;
		double nearHalfExtent = 0.0;
		float waterLevel = 0.0f;
		bool hasWater = false;
	};

	bool PrepareShowcaseTerrain(const openrepublic::content::MapPlacementRequest& request,
		double nearHalfExtent, ShowcaseTerrainData& data, std::string& error);

	// Presentation-only relief: exaggerates and extends land height with distance from the town so
	// the surroundings read as hills and mountain ranges. Identity near the town and over water.
	float ShowcaseLandHeight(const ShowcaseTerrainData& data, double worldX, double worldZ, float rawHeight);

	// Vertex colour of the showcase terrain at a world position (linear-ish 0..1 RGB).
	XMFLOAT3 ShadeShowcaseTerrain(const ShowcaseTerrainData& data, double worldX, double worldZ,
		float height, float slope);

	// Procedural detail textures (created once, shared).
	const wi::Resource& ShowcaseGrassTexture();
	const wi::Resource& ShowcaseAsphaltTexture();

	// Gives a terrain material the grass detail texture (UV 0 = world metres / 12).
	void ApplyShowcaseGrass(wi::scene::MaterialComponent& material);

	// Builds the far landscape mesh, the lake level and the trees. `instances` are the placed
	// buildings (centre-relative positions are derived from the region centre).
	bool BuildShowcaseWorld(wi::scene::Scene& scene,
		const openrepublic::content::MapPlacementRequest& request, const ShowcaseTerrainData& terrain,
		double regionRadius, const std::vector<RegionInstance>& instances,
		ShowcaseEnvironmentInfo& info, std::string& error);
}
