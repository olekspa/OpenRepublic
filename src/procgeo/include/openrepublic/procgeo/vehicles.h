#pragma once

#include "openrepublic/procgeo/mesh.h"

#include <cstdint>
#include <string_view>

namespace openrepublic::procgeo
{
	enum class VehicleKind
	{
		CargoTruck,
		Tanker,
		BoxVan,
		Car,
		Bus,
		Trailer,
		Flatbed,
		DumpTruck,
		Locomotive,
		TrainWagon
	};

	// Name and resource matching is a presentation policy for OpenRepublic's authored
	// geometry. It does not identify original-game vehicle classes.
	VehicleKind VehicleKindForCatalogName(std::string_view name, bool railOnly = false);
	VehicleKind TruckKindForResourceName(std::string_view resourceName);

	// Builds an OpenRepublic-authored low-poly vehicle. Seed selects a deterministic body colour.
	Mesh MakeVehicle(VehicleKind kind, std::uint32_t colorSeed);
	Mesh MakeVehicleBody(VehicleKind kind, std::uint32_t colorSeed);
	Mesh MakeTrainLocomotive(std::uint32_t colorSeed);
	Mesh MakeTrainWagon(std::uint32_t colorSeed);
	// Native scene variants use a body without baked wheels/load so each wheel and
	// cargo load can be animated/toggled as a separate presentation node.
	Mesh MakeCargoTruckBody(std::uint32_t colorSeed);
	Mesh MakeVehicleWheel();
	Mesh MakeVehicleHeadlightPair();
	Mesh MakeVehicleBrakeLightPair();
	Mesh MakeVehicleExhaustPuff();
	Mesh MakeCoalHeap();
	Mesh MakeVehicleIdLabel(std::uint64_t vehicleId);
	float VehicleLength(VehicleKind kind);
}
