#include "openrepublic/procgeo/vehicles.h"

#include <array>
#include <cmath>
#include <iostream>

using namespace openrepublic::procgeo;

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* label) {
		if (!ok) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
	};
	struct Spec { VehicleKind kind; float widthLo, widthHi, lengthLo, lengthHi, heightLo, heightHi; };
	const std::array<Spec, 10> specs{{
		{VehicleKind::CargoTruck, 2.2f, 2.9f, 7.0f, 9.0f, 1.8f, 3.0f},
		{VehicleKind::Tanker,    2.2f, 2.9f, 7.0f, 9.0f, 2.0f, 3.0f},
		{VehicleKind::BoxVan,    2.2f, 2.9f, 7.0f, 9.0f, 2.0f, 3.0f},
		{VehicleKind::Car,       1.5f, 2.0f, 3.7f, 4.4f, 1.0f, 2.0f},
		{VehicleKind::Bus,       2.0f, 2.6f, 7.0f, 8.0f, 3.0f, 4.0f},
		{VehicleKind::Trailer,   2.2f, 2.9f, 7.0f, 9.0f, 0.9f, 2.0f},
		{VehicleKind::Flatbed,   2.2f, 2.9f, 7.0f, 9.0f, 1.8f, 3.0f},
		{VehicleKind::DumpTruck, 2.2f, 2.9f, 7.0f, 9.0f, 2.0f, 3.3f},
		{VehicleKind::Locomotive,2.4f, 2.8f, 4.7f, 5.1f, 2.0f, 2.6f},
		{VehicleKind::TrainWagon,2.5f, 2.8f, 8.2f, 8.8f, 1.0f, 2.0f},
	}};
	for (const Spec& spec : specs)
	{
		const Mesh mesh = MakeVehicle(spec.kind, 42u);
		check(mesh.Valid(), "vehicle mesh indices and vertex colours are valid");
		check(mesh.indices.size() / 3 >= 150 && mesh.indices.size() / 3 <= 600,
			"vehicle has showcase-scale triangle count");
		Vec3 lo{}, hi{};
		check(mesh.Bounds(lo, hi), "vehicle has bounds");
		const float width = hi.x - lo.x, length = hi.z - lo.z, height = hi.y - lo.y;
		check(width >= spec.widthLo && width <= spec.widthHi, "vehicle width is plausible in metres");
		check(length >= spec.lengthLo && length <= spec.lengthHi, "vehicle length is plausible in metres");
		check(std::abs(length - VehicleLength(spec.kind)) <= 0.25f, "reported length matches mesh bounds");
		check(height >= spec.heightLo && height <= spec.heightHi, "vehicle height is plausible in metres");
		check(std::abs(lo.y) <= 0.05f, "wheel bottoms meet ground level");

		const Mesh again = MakeVehicle(spec.kind, 42u);
		check(mesh.positions.size() == again.positions.size() && mesh.colors.size() == again.colors.size() &&
			mesh.indices == again.indices, "same vehicle input has deterministic mesh structure");
		bool sameValues = mesh.positions.size() == again.positions.size();
		for (std::size_t i = 0; sameValues && i < mesh.positions.size(); ++i)
		{
			const auto& a = mesh.positions[i]; const auto& b = again.positions[i];
			const auto& ca = mesh.colors[i]; const auto& cb = again.colors[i];
			sameValues = a.x == b.x && a.y == b.y && a.z == b.z && ca.r == cb.r && ca.g == cb.g && ca.b == cb.b;
		}
		check(sameValues, "same vehicle input has deterministic positions and colours");
		const Mesh variant = MakeVehicle(spec.kind, 43u);
		bool changedColor = mesh.colors.size() != variant.colors.size();
		for (std::size_t i = 0; !changedColor && i < mesh.colors.size(); ++i)
			changedColor = mesh.colors[i].r != variant.colors[i].r || mesh.colors[i].g != variant.colors[i].g ||
				mesh.colors[i].b != variant.colors[i].b;
		check(changedColor, "different seeds select a different muted paint variant");
	}
	const Mesh subtleVariant = MakeVehicleBody(VehicleKind::CargoTruck, 44u);
	const Mesh baseBody = MakeVehicleBody(VehicleKind::CargoTruck, 42u);
	check(subtleVariant.Valid() && baseBody.Valid() && subtleVariant.positions.size() != baseBody.positions.size(),
		"seed adds a subtle deterministic body detail on selected vehicle variants");
	const Mesh truckBody = MakeCargoTruckBody(5u);
	const Mesh wheel = MakeVehicleWheel();
	const Mesh coalHeap = MakeCoalHeap();
	const Mesh idLabel = MakeVehicleIdLabel(23);
	const Mesh locomotive = MakeTrainLocomotive(4u);
	const Mesh wagon = MakeTrainWagon(4u);
	const Mesh headlights = MakeVehicleHeadlightPair();
	const Mesh brakeLights = MakeVehicleBrakeLightPair();
	const Mesh exhaust = MakeVehicleExhaustPuff();
	Vec3 bodyLo{}, bodyHi{}, wheelLo{}, wheelHi{};
	check(truckBody.Valid() && truckBody.Bounds(bodyLo, bodyHi), "separate cargo-truck body mesh is valid");
	check(wheel.Valid() && wheel.Bounds(wheelLo, wheelHi), "separate wheel mesh is valid");
	check(coalHeap.Valid(), "separate project-authored coal heap mesh is valid");
	check(idLabel.Valid(), "vehicle ID label mesh is valid");
	check(locomotive.Valid() && wagon.Valid(), "train locomotive and wagon meshes are valid");
	check(headlights.Valid() && brakeLights.Valid(), "headlight and brake-light meshes are valid");
	check(exhaust.Valid(), "vehicle exhaust particle mesh is valid");
	check(locomotive.indices != wagon.indices || locomotive.positions.size() != wagon.positions.size(),
		"train locomotive and wagon use distinct mesh geometry");
	check(VehicleKindForCatalogName("city_bus") == VehicleKind::Bus &&
		VehicleKindForCatalogName("family_car") == VehicleKind::Car &&
		VehicleKindForCatalogName("electric_locomotive", true) == VehicleKind::Locomotive &&
		VehicleKindForCatalogName("freight_wagon", true) == VehicleKind::TrainWagon,
		"catalog names map to documented presentation vehicle kinds");
	check(TruckKindForResourceName("coal") == VehicleKind::DumpTruck &&
		TruckKindForResourceName("fuel") == VehicleKind::Tanker &&
		TruckKindForResourceName("steel") == VehicleKind::Flatbed &&
		TruckKindForResourceName("grain") == VehicleKind::CargoTruck,
		"resource categories map to dump, tanker, flatbed, and box truck presentations");
	check(bodyLo.y > 0.05f, "separate truck body omits baked ground-contact wheels");
	check(std::abs((wheelHi.x - wheelLo.x) - 0.30f) < 1e-5f &&
		std::abs((wheelHi.y - wheelLo.y) - 0.96f) < 1e-5f &&
		std::abs((wheelHi.z - wheelLo.z) - 0.96f) < 1e-5f,
		"animated wheel mesh has an X axle and expected radius/width");
	check(VehicleLength(VehicleKind::Car) > 3.7f && VehicleLength(VehicleKind::Car) < 4.4f,
		"reported car length matches its scale");
	check(VehicleLength(VehicleKind::Bus) > 7.0f && VehicleLength(VehicleKind::Bus) < 8.0f,
		"reported bus length matches its scale");
	if (failures) return 1;
	std::cout << "All procgeo vehicle tests passed\n";
	return 0;
}
