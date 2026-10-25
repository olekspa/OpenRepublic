#include "openrepublic/procgeo/vehicles.h"

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <cctype>

namespace openrepublic::procgeo
{
namespace
{
	constexpr Color3 kRubber{ 0.075f, 0.080f, 0.078f };
	constexpr Color3 kGlass{ 0.075f, 0.115f, 0.145f };
	constexpr Color3 kChrome{ 0.56f, 0.57f, 0.54f };
	constexpr Color3 kLamp{ 0.92f, 0.72f, 0.40f };
	constexpr Color3 kTail{ 0.55f, 0.055f, 0.035f };
	constexpr Color3 kBed{ 0.24f, 0.25f, 0.23f };
	constexpr Color3 kWood{ 0.30f, 0.19f, 0.10f };
	constexpr std::array<Color3, 7> kPaint{{
		{ 0.12f, 0.25f, 0.39f }, { 0.43f, 0.12f, 0.095f }, { 0.62f, 0.48f, 0.16f },
		{ 0.68f, 0.67f, 0.61f }, { 0.19f, 0.34f, 0.22f }, { 0.31f, 0.32f, 0.31f },
		{ 0.58f, 0.29f, 0.12f }
	}};

	void AddWheelX(Mesh& mesh, float x, float y, float z, float radius, float width, int segments = 12)
	{
		const std::uint32_t base = static_cast<std::uint32_t>(mesh.positions.size());
		for (int i = 0; i < segments; ++i)
		{
			const float a = 6.28318530718f * static_cast<float>(i) / static_cast<float>(segments);
			const float yy = std::cos(a) * radius, zz = std::sin(a) * radius;
			mesh.positions.push_back({ x - width * 0.5f, y + yy, z + zz });
			mesh.positions.push_back({ x + width * 0.5f, y + yy, z + zz });
			mesh.colors.push_back(kRubber); mesh.colors.push_back(kRubber);
		}
		for (int i = 0; i < segments; ++i)
		{
			const std::uint32_t lo = base + 2u * static_cast<std::uint32_t>(i);
			const std::uint32_t next = base + 2u * static_cast<std::uint32_t>((i + 1) % segments);
			mesh.indices.insert(mesh.indices.end(), { lo, next, lo + 1, next, next + 1, lo + 1 });
		}
		const std::uint32_t left = static_cast<std::uint32_t>(mesh.positions.size());
		mesh.positions.push_back({ x - width * 0.5f, y, z }); mesh.colors.push_back(kRubber);
		const std::uint32_t right = static_cast<std::uint32_t>(mesh.positions.size());
		mesh.positions.push_back({ x + width * 0.5f, y, z }); mesh.colors.push_back(kRubber);
		for (int i = 0; i < segments; ++i)
		{
			const std::uint32_t a = base + 2u * static_cast<std::uint32_t>(i);
			const std::uint32_t b = base + 2u * static_cast<std::uint32_t>((i + 1) % segments);
			// On the left cap the outward normal is -X; on the right it is +X.
			mesh.indices.insert(mesh.indices.end(), { left, b, a, right, a + 1, b + 1 });
		}
		// A compact light-metal hub remains visible while the whole wheel rotates.
		constexpr float hubRadius = 0.22f;
		const std::uint32_t leftHub = static_cast<std::uint32_t>(mesh.positions.size());
		mesh.positions.push_back({ x - width * 0.5f, y, z }); mesh.colors.push_back(kChrome);
		const std::uint32_t rightHub = static_cast<std::uint32_t>(mesh.positions.size());
		mesh.positions.push_back({ x + width * 0.5f, y, z }); mesh.colors.push_back(kChrome);
		const std::uint32_t hubRing = static_cast<std::uint32_t>(mesh.positions.size());
		for (int i = 0; i < segments; ++i)
		{
			const float a = 6.28318530718f * static_cast<float>(i) / static_cast<float>(segments);
			const float yy = std::cos(a) * hubRadius, zz = std::sin(a) * hubRadius;
			mesh.positions.push_back({ x - width * 0.5f, y + yy, z + zz }); mesh.colors.push_back(kChrome);
			mesh.positions.push_back({ x + width * 0.5f, y + yy, z + zz }); mesh.colors.push_back(kChrome);
		}
		for (int i = 0; i < segments; ++i)
		{
			const std::uint32_t a = hubRing + 2u * static_cast<std::uint32_t>(i);
			const std::uint32_t b = hubRing + 2u * static_cast<std::uint32_t>((i + 1) % segments);
			mesh.indices.insert(mesh.indices.end(), { leftHub, b, a, rightHub, a + 1, b + 1 });
		}
	}

	void Box(Mesh& m, Vec3 c, Vec3 h, Color3 color) { AddBox(m, c, h, color); }
	void Wheels(Mesh& m, float halfTrack, float frontZ, float rearZ, float radius, float tireWidth)
	{
		for (float x : { -halfTrack, halfTrack })
			for (float z : { frontZ, rearZ }) AddWheelX(m, x, radius, z, radius, tireWidth);
	}

	Mesh Build(VehicleKind kind, Color3 paint, std::uint32_t seed, bool includeWheels = true,
		bool includeCargoLoad = true)
	{
		Mesh m;
		const Color3 cab = paint;
		if (kind == VehicleKind::Car)
		{
			Box(m, {0, 0.68f, -0.08f}, {0.83f, 0.29f, 1.87f}, paint);
			Box(m, {0, 1.15f, -0.28f}, {0.70f, 0.28f, 0.84f}, paint);
			Box(m, {0, 1.16f, 0.33f}, {0.64f, 0.20f, 0.53f}, kGlass);
			Box(m, {0, 1.16f, -0.82f}, {0.64f, 0.20f, 0.45f}, kGlass);
			if (includeWheels) Wheels(m, 0.79f, 1.22f, -1.25f, 0.34f, 0.22f);
			Box(m, {0, 0.48f, 1.83f}, {0.82f, 0.10f, 0.10f}, kChrome);
			Box(m, {0, 0.48f, -1.83f}, {0.82f, 0.10f, 0.10f}, kChrome);
			for (float x : {-0.57f, 0.57f}) { Box(m,{x,0.75f,1.89f},{0.14f,0.11f,0.035f},kLamp); Box(m,{x,0.75f,-1.89f},{0.13f,0.11f,0.035f},kTail); }
			return m;
		}
		if (kind == VehicleKind::Bus)
		{
			Box(m,{0,1.82f,-0.05f},{1.13f,1.32f,3.64f},paint);
			Box(m,{0,2.44f,2.46f},{1.00f,0.38f,0.67f},kGlass);
			Box(m,{0,2.44f,-0.11f},{1.00f,0.36f,1.45f},kGlass);
			Box(m,{0,2.44f,-2.63f},{1.00f,0.36f,0.63f},kGlass);
			Box(m,{0,0.52f,3.65f},{1.04f,0.11f,0.10f},kChrome);
			Box(m,{0,0.52f,-3.65f},{1.04f,0.11f,0.10f},kChrome);
			if (includeWheels) Wheels(m,1.06f,2.45f,-2.42f,0.46f,0.27f);
			for(float x:{-0.82f,0.82f}) { Box(m,{x,0.94f,3.67f},{0.13f,0.13f,0.035f},kLamp); Box(m,{x,0.94f,-3.67f},{0.13f,0.13f,0.035f},kTail); }
			return m;
		}
		if (kind == VehicleKind::Locomotive || kind == VehicleKind::TrainWagon)
		{
			if (kind == VehicleKind::Locomotive)
			{
				Box(m, {0, 1.08f, 0.10f}, {1.28f, 0.72f, 2.42f}, paint);
				Box(m, {0, 1.94f, 0.70f}, {0.90f, 0.45f, 0.88f}, paint);
				Box(m, {0, 1.96f, 1.61f}, {0.72f, 0.30f, 0.035f}, kGlass);
				Box(m, {0, 1.04f, 2.56f}, {0.96f, 0.12f, 0.12f}, kChrome);
				if (includeWheels) for (float z : {-1.35f, 0.05f, 1.25f})
					for (float x : {-0.96f, 0.96f}) AddWheelX(m, x, 0.52f, z, 0.52f, 0.25f);
				for (float x : {-0.82f, 0.82f}) Box(m, {x, 1.45f, 2.57f}, {0.15f, 0.13f, 0.04f}, kLamp);
			}
			else
			{
				Box(m, {0, 1.08f, 0}, {1.38f, 0.16f, 4.25f}, kBed);
				Box(m, {0, 1.58f, 0}, {1.30f, 0.30f, 3.92f}, paint);
				if (includeWheels) for (float x : {-1.10f, 1.10f})
					for (float z : {-1.46f, 1.46f}) AddWheelX(m, x, 0.50f, z, 0.50f, 0.24f);
			}
			return m;
		}
		if (kind == VehicleKind::Trailer)
		{
			Box(m,{0,0.91f,-0.05f},{1.20f,0.14f,3.48f},kBed);
			for(float x:{-1.05f,1.05f}) Box(m,{x,1.20f,-0.05f},{0.15f,0.42f,3.36f},paint);
			if (includeWheels) Wheels(m,0.98f,-1.52f,-0.73f,0.42f,0.27f);
			Box(m,{0,0.52f,-3.53f},{1.10f,0.10f,0.10f},kChrome);
			return m;
		}

		// Shared cab-over chassis: wheels and chassis are centered around ground-level origin.
		const float zFront = 3.05f, zRear = -2.62f;
		Box(m,{0,0.87f,0.0f},{1.16f,0.15f,3.72f},kBed);
		Box(m,{0,1.38f,2.32f},{1.12f,0.72f,1.22f},cab);
		Box(m,{0,1.72f,3.48f},{0.87f,0.30f,0.035f},kGlass);
		Box(m,{0,1.70f,1.15f},{0.91f,0.30f,0.035f},kGlass);
		if (includeWheels) Wheels(m,1.12f,zFront,zRear,0.48f,0.30f);
		Box(m,{0,0.74f,3.75f},{1.03f,0.12f,0.12f},kChrome);
		Box(m,{0,0.74f,-3.76f},{1.03f,0.12f,0.12f},kChrome);
		for(float x:{-0.82f,0.82f}) { Box(m,{x,0.96f,3.78f},{0.16f,0.13f,0.04f},kLamp); Box(m,{x,0.96f,-3.78f},{0.14f,0.12f,0.04f},kTail); }
		if (kind == VehicleKind::CargoTruck || kind == VehicleKind::Flatbed || kind == VehicleKind::DumpTruck)
		{
			Box(m,{0,1.17f,-1.60f},{1.12f,0.15f,1.80f},kBed);
			if (kind != VehicleKind::Flatbed)
				for(float x:{-1.08f,1.08f}) Box(m,{x,1.43f,-1.60f},{0.08f,0.18f,1.74f},kBed);
			if (kind == VehicleKind::DumpTruck)
			{
				Box(m, {0, 1.63f, -1.55f}, {1.12f, 0.48f, 1.72f}, {0.36f, 0.38f, 0.39f});
				Box(m, {0, 2.05f, -1.62f}, {0.92f, 0.10f, 1.40f}, {0.30f, 0.32f, 0.33f});
			}
			if (includeCargoLoad)
			{
				if ((seed & 1u) != 0u) Box(m,{0,1.68f,-1.55f},{0.78f,0.35f,1.18f},kWood);
				else for(int i=0;i<3;++i) Box(m,{-0.48f+0.48f*i,1.57f,-1.62f},{0.19f,0.26f,1.34f},kWood);
			}
		}
		else if (kind == VehicleKind::Tanker)
		{
			AddCylinder(m,0,-1.55f,1.05f,2.23f,1.00f,16,{0.58f,0.59f,0.56f});
			Box(m,{0,1.05f,-1.55f},{0.92f,0.12f,1.84f},kChrome);
		}
		else if (kind == VehicleKind::BoxVan)
		{
			Box(m,{0,1.73f,-1.43f},{1.12f,0.88f,2.03f},paint);
			Box(m,{0,1.73f,0.65f},{0.91f,0.34f,0.035f},kGlass);
		}
		if ((seed & 4u) != 0u && kind != VehicleKind::Car && kind != VehicleKind::Bus)
			Box(m, {0.0f, 2.22f, 2.40f}, {0.12f, 0.08f, 0.13f}, {0.78f, 0.42f, 0.12f});
		return m;
	}
}

VehicleKind VehicleKindForCatalogName(std::string_view name, bool railOnly)
{
	std::string folded;
	folded.reserve(name.size());
	for (unsigned char c : name) folded.push_back(static_cast<char>(std::tolower(c)));
	if (railOnly || folded.find("locomotive") != std::string::npos || folded.find("train") != std::string::npos)
		return folded.find("wagon") != std::string::npos ? VehicleKind::TrainWagon : VehicleKind::Locomotive;
	if (folded.find("bus") != std::string::npos || folded.find("coach") != std::string::npos) return VehicleKind::Bus;
	if (folded.find("car") != std::string::npos || folded.find("passenger") != std::string::npos) return VehicleKind::Car;
	if (folded.find("tanker") != std::string::npos || folded.find("cistern") != std::string::npos) return VehicleKind::Tanker;
	if (folded.find("dump") != std::string::npos || folded.find("tipper") != std::string::npos) return VehicleKind::DumpTruck;
	if (folded.find("flatbed") != std::string::npos || folded.find("trailer") != std::string::npos) return VehicleKind::Flatbed;
	if (folded.find("van") != std::string::npos || folded.find("box") != std::string::npos) return VehicleKind::BoxVan;
	return VehicleKind::CargoTruck;
}

VehicleKind TruckKindForResourceName(std::string_view name)
{
	std::string folded;
	folded.reserve(name.size());
	for (unsigned char c : name) folded.push_back(static_cast<char>(std::tolower(c)));
	if (folded.find("fuel") != std::string::npos || folded.find("oil") != std::string::npos ||
		folded.find("water") != std::string::npos || folded.find("milk") != std::string::npos ||
		folded.find("alcohol") != std::string::npos) return VehicleKind::Tanker;
	if (folded.find("coal") != std::string::npos || folded.find("ore") != std::string::npos ||
		folded.find("gravel") != std::string::npos || folded.find("sand") != std::string::npos ||
		folded.find("stone") != std::string::npos) return VehicleKind::DumpTruck;
	if (folded.find("steel") != std::string::npos || folded.find("plank") != std::string::npos ||
		folded.find("board") != std::string::npos || folded.find("vehicle") != std::string::npos)
		return VehicleKind::Flatbed;
	return VehicleKind::CargoTruck;
}

Mesh MakeTrainLocomotive(std::uint32_t colorSeed) { return Build(VehicleKind::Locomotive, kPaint[colorSeed % kPaint.size()], colorSeed); }
Mesh MakeTrainWagon(std::uint32_t colorSeed) { return Build(VehicleKind::TrainWagon, kPaint[colorSeed % kPaint.size()], colorSeed); }

Mesh MakeVehicle(VehicleKind kind, std::uint32_t colorSeed)
{
	return Build(kind, kPaint[colorSeed % kPaint.size()], colorSeed);
}

Mesh MakeVehicleBody(VehicleKind kind, std::uint32_t colorSeed)
{
	return Build(kind, kPaint[colorSeed % kPaint.size()], colorSeed, false, false);
}

Mesh MakeCargoTruckBody(std::uint32_t colorSeed)
{
	return Build(VehicleKind::CargoTruck, kPaint[colorSeed % kPaint.size()], colorSeed, false, false);
}

Mesh MakeVehicleWheel()
{
	Mesh wheel;
	AddWheelX(wheel, 0.0f, 0.0f, 0.0f, 0.48f, 0.30f);
	return wheel;
}

Mesh MakeVehicleHeadlightPair()
{
	Mesh mesh;
	const Color3 lamp{0.95f, 0.82f, 0.54f};
	for (float x : {-0.62f, 0.62f}) AddBox(mesh, {x, 0.0f, 0.0f}, {0.20f, 0.12f, 0.045f}, lamp);
	return mesh;
}

Mesh MakeVehicleBrakeLightPair()
{
	Mesh mesh;
	const Color3 lamp{0.45f, 0.025f, 0.018f};
	for (float x : {-0.62f, 0.62f}) AddBox(mesh, {x, 0.0f, 0.0f}, {0.15f, 0.11f, 0.04f}, lamp);
	return mesh;
}

Mesh MakeVehicleExhaustPuff()
{
	Mesh mesh;
	AddBox(mesh, {0.0f, 0.0f, 0.0f}, {0.12f, 0.10f, 0.12f}, {0.24f, 0.25f, 0.25f});
	AddBox(mesh, {0.04f, 0.12f, -0.02f}, {0.09f, 0.08f, 0.09f}, {0.31f, 0.32f, 0.32f});
	return mesh;
}

Mesh MakeCoalHeap()
{
	Mesh mesh;
	const Color3 coal{ 0.055f, 0.052f, 0.048f };
	// Faceted project-authored load, sized to sit inside the truck bed.
	AddBox(mesh, {0.0f, 0.04f, 0.0f}, {0.68f, 0.17f, 0.92f}, coal);
	AddBox(mesh, {-0.24f, 0.20f, -0.12f}, {0.31f, 0.16f, 0.40f}, {0.075f, 0.070f, 0.063f});
	AddBox(mesh, {0.28f, 0.18f, 0.22f}, {0.29f, 0.14f, 0.34f}, coal);
	AddBox(mesh, {0.02f, 0.29f, -0.35f}, {0.24f, 0.12f, 0.28f}, {0.09f, 0.082f, 0.073f});
	return mesh;
}

Mesh MakeVehicleIdLabel(std::uint64_t vehicleId)
{
	static constexpr bool segments[10][7] = {
		{true,true,true,true,true,true,false}, {false,true,true,false,false,false,false},
		{true,true,false,true,true,false,true}, {true,true,true,true,false,false,true},
		{false,true,true,false,false,true,true}, {true,false,true,true,false,true,true},
		{true,false,true,true,true,true,true}, {true,true,true,false,false,false,false},
		{true,true,true,true,true,true,true}, {true,true,true,true,false,true,true}
	};
	const std::string digits = std::to_string(vehicleId);
	Mesh mesh;
	const Color3 ink{ 0.96f, 0.84f, 0.48f };
	constexpr float halfWidth = 0.15f, halfBar = 0.035f, halfDepth = 0.025f;
	const float totalWidth = static_cast<float>(digits.size()) * 0.40f;
	for (std::size_t i = 0; i < digits.size(); ++i)
	{
		const int digit = digits[i] - '0';
		const float x = -totalWidth * 0.5f + static_cast<float>(i) * 0.40f;
		const auto bar = [&](int segment, float cx, float cy, float hx, float hy) {
			if (segments[digit][segment]) AddBox(mesh, {x + cx, cy, 0.0f}, {hx, hy, halfDepth}, ink);
		};
		bar(0, 0, 0.39f, halfWidth, halfBar); bar(6, 0, 0, halfWidth, halfBar);
		bar(3, 0, -0.39f, halfWidth, halfBar); bar(5, -0.185f, 0.20f, halfBar, 0.16f);
		bar(1, 0.185f, 0.20f, halfBar, 0.16f); bar(4, -0.185f, -0.20f, halfBar, 0.16f);
		bar(2, 0.185f, -0.20f, halfBar, 0.16f);
	}
	return mesh;
}

float VehicleLength(VehicleKind kind)
{
	switch (kind)
	{
	case VehicleKind::Car: return 3.9f;
	case VehicleKind::Bus: return 7.5f;
	case VehicleKind::Trailer: return 7.26f;
	case VehicleKind::Locomotive: return 4.88f;
	case VehicleKind::TrainWagon: return 8.5f;
	default: return 7.8f;
	}
}
}
