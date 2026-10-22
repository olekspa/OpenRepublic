#pragma once

#include "openrepublic/procgeo/mesh.h"

#include <cstdint>

namespace openrepublic::procgeo
{
	enum class VehicleKind
	{
		CargoTruck,
		Tanker,
		BoxVan,
		Car,
		Bus,
		Trailer
	};

	// Builds an OpenRepublic-authored low-poly vehicle. Seed selects a deterministic body colour.
	Mesh MakeVehicle(VehicleKind kind, std::uint32_t colorSeed);
	float VehicleLength(VehicleKind kind);
}
