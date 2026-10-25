#pragma once

#include "openrepublic/procgeo/mesh.h"

#include <cstdint>
#include <string_view>

namespace openrepublic::procgeo
{
	// OpenRepublic presentation templates selected from catalog names. Dimensions are
	// footprint metres; vertical proportions and palettes are authored display policy.
	Mesh MakeBuilding(std::string_view catalogName, float width, float depth,
		std::uint32_t seed, bool constructionSite = false, float progress = 1.0f,
		bool dark = false);
}
