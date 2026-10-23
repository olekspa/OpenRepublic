#pragma once

#include "EconomyOverlay.h"
#include "WickedEngine.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/terrain_preview.h"
#include "openrepublic/content/tree_placements.h"

#include <cstddef>
#include <map>
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
		// 1 for flat maps (relief is exaggerated), 0 for maps that already have real mountains.
		float reliefScale = 1.0f;
	};

	// Optional authentic tree meshes loaded from a local WRSR install (never committed or published).
	// Each entry is a hidden mesh entity; the scatter instances them with a height-normalising scale.
	struct RealTreeSet
	{
		std::vector<wi::ecs::Entity> conifers;
		std::vector<wi::ecs::Entity> broadleaf;
		std::vector<wi::ecs::Entity> poplars;
		std::vector<wi::ecs::Entity> shrubs;
		// Exact meshes by the species model name used in the package's trees.bin (for example "pine1").
		std::map<std::string, wi::ecs::Entity> byName;
		// Decoded real placements (positions are INFERRED to be map metres); null when unavailable.
		const openrepublic::content::TreePlacements* placements = nullptr;
		bool Any() const { return !conifers.empty() || !broadleaf.empty() || !poplars.empty() || !shrubs.empty(); }
	};

	bool PrepareShowcaseTerrain(const openrepublic::content::MapPlacementRequest& request,
		double nearHalfExtent, ShowcaseTerrainData& data, std::string& error);

	// Presentation-only relief: exaggerates and extends land height with distance from the town so
	// the surroundings read as hills and mountain ranges. Identity near the town and over water.
	float ShowcaseLandHeight(const ShowcaseTerrainData& data, double worldX, double worldZ, float rawHeight);

	// Vertex colour of the showcase terrain at a world position (linear-ish 0..1 RGB).
	XMFLOAT3 ShadeShowcaseTerrain(const ShowcaseTerrainData& data, double worldX, double worldZ,
		float height, float slope);

	// Builds a low-cost OpenRepublic foam ribbon along the sampled terrain/water contour.
	// This is presentation geometry and does not describe WRSR shoreline data.
	bool BuildShowcaseShoreFoam(wi::scene::Scene& scene, const ShowcaseTerrainData& terrain,
		const XMFLOAT3& worldOrigin, std::string& error);

	// Sun elevation the baked terrain colours are tuned for; a high sun needs a lower tint gain
	// (vertex colours cannot be re-shaded at runtime). NaN keeps the golden-hour tuning.
	void SetShowcaseBakedSunElevation(float elevationDegrees);
	// Terrain tint gain for a given sun elevation (1 at <= 17 degrees); the ratio between the current and the
	// baked gain is applied to the terrain materials at runtime so sun changes keep the tint balanced.
	float ShowcaseTerrainSunGain(float elevationDegrees);
	float ShowcaseBakedTerrainSunGain();

	// Procedural detail textures (created once, shared).
	const wi::Resource& ShowcaseGrassTexture();
	const wi::Resource& ShowcaseGrassBladeTexture();
	const wi::Resource& ShowcaseAsphaltTexture();
	// Soft round alpha sprite for smoke puffs.
	const wi::Resource& ShowcaseSoftCircleTexture();

	// Gives a terrain material the grass detail texture (UV 0 = world metres / 12). When `realGround`
	// is valid (a texture decoded from a local WRSR install) it replaces the procedural one.
	void ApplyShowcaseGrass(wi::scene::MaterialComponent& material, bool withNormalMap = true);
	void SetShowcaseRealGroundTexture(const wi::Resource& albedo, const wi::Resource& normal);

	// Builds the far landscape mesh, the lake level and the trees. `instances` are the placed
	// buildings (centre-relative positions are derived from the region centre).
	bool BuildShowcaseWorld(wi::scene::Scene& scene,
		const openrepublic::content::MapPlacementRequest& request, const ShowcaseTerrainData& terrain,
		double regionRadius, const std::vector<RegionInstance>& instances, const RealTreeSet* realTrees,
		ShowcaseEnvironmentInfo& info, std::string& error, bool vegetationWind = true);
}
