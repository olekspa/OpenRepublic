#pragma once

#include "openrepublic/procgeo/mesh.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace openrepublic::procgeo
{
	struct PropInfo
	{
		std::string_view name;
		float nominalHeight;
	};

	Mesh MakeStreetLamp();
	Mesh MakeWoodenUtilityPole();
	Mesh MakeSteelLatticePylon();
	Mesh MakeCable(Vec3 a, Vec3 b, float sag, int segments, float thickness);
	Mesh MakeRailSegment(float length);
	Mesh MakeRailCrossbuck();
	Mesh MakeConcreteFence();
	Mesh MakeWoodenFence();
	Mesh MakeShrub(std::uint32_t seed);
	Mesh MakeGrassTuft(std::uint32_t seed);
	Mesh MakeBrickChimney();
	Mesh MakeCoolingTower();
	const std::vector<PropInfo>& AvailableProps();
}
