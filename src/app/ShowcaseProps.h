#pragma once

#include "EconomyOverlay.h"
#include "ShowcaseEnvironment.h"
#include "WickedEngine.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/procgeo/mesh.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace openrepublic::preview
{
	struct ShowcasePropsInfo
	{
		std::size_t vehicles = 0;
		std::size_t poles = 0;
		std::size_t lamps = 0;
		std::size_t pylons = 0;
		std::size_t railPieces = 0;
		std::size_t realVehicleTypes = 0;
		std::string summary;
	};

	// Creates a hidden mesh entity from an engine-independent procedural mesh; objects reference it.
	wi::ecs::Entity CreateProceduralMeshEntity(wi::scene::Scene& scene, const char* name,
		const openrepublic::procgeo::Mesh& mesh, float roughness, float metalness = 0.0f);

	// Places OpenRepublic procedural vehicles, utility poles with wires, street lamps, a pylon line, a
	// railway and an industrial skyline around the town. Everything is presentation-only (no WRSR content).
	bool BuildShowcaseProps(wi::scene::Scene& scene, const openrepublic::content::MapPlacementRequest& request,
		const ShowcaseTerrainData& terrain, double regionRadius, const std::filesystem::path& contentRoot,
		ShowcasePropsInfo& info, std::string& error);
}
