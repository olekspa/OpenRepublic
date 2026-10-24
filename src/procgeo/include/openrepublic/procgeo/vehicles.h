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
	// Native scene variants use a body without baked wheels/load so each wheel and
	// cargo load can be animated/toggled as a separate presentation node.
	Mesh MakeCargoTruckBody(std::uint32_t colorSeed);
	Mesh MakeVehicleWheel();
	float VehicleLength(VehicleKind kind);
}
