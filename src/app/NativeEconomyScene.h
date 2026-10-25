#pragma once

#include "WickedEngine.h"
#include "EconomyOverlay.h"
#include "openrepublic/economy/economy.h"
#include "openrepublic/runtime/native_economy_layout.h"
#include "openrepublic/runtime/ambient_traffic.h"
#include "openrepublic/procgeo/vehicles.h"
#include "openrepublic/runtime/synthetic_terrain.h"
#include "openrepublic/runtime/native_traffic_polish.h"
#include "openrepublic/runtime/native_rail_geometry.h"

#include <optional>
#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openrepublic::preview
{
	// Renders an authoritative native economy snapshot as OpenRepublic-authored
	// primitive proxies. These shapes communicate grid occupancy and connectivity;
	// they do not claim physical dimensions or depict original-game assets.
	class NativeEconomyScene final
	{
	public:
		bool Initialize(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error,
			bool nativeTerrain = false);
		bool Sync(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error);
		bool SyncVehiclesOnly(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog, const runtime::NativeEconomyTransform& transform, std::string& error);
		void UpdateVehicleMotion(wi::scene::Scene& scene, double alpha,
			const std::function<double(double, double)>& groundHeight = {});
		void SetAmbientTraffic(bool enabled) noexcept { ambientTrafficEnabled_ = enabled; }
		void SetTrafficPolish(bool enabled) noexcept { trafficPolishEnabled_ = enabled; }
		void SetDarkLighting(bool dark) noexcept { darkLighting_ = dark; }
		// Re-centers native geometry around the current snapshot. The caller can use
		// this for an explicit framing action while leaving ordinary Sync frozen.
		bool Rebase(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error);

		std::optional<economy::BuildingId> BuildingForEntity(wi::ecs::Entity entity) const;
		DirectX::XMFLOAT3 CellPosition(economy::Cell cell) const;
	std::optional<DirectX::XMFLOAT3> VehiclePosition(economy::VehicleId id, double alpha) const;
		bool CellFromGroundPoint(double worldX, double worldZ, std::uint32_t gridWidth,
			std::uint32_t gridDepth, economy::Cell& outCell) const noexcept;
		double GroundHeight(double worldX, double worldZ) const noexcept;
		bool CanPlaceFootprint(economy::Cell cell, std::uint32_t width, std::uint32_t depth) const noexcept;
		float CameraDistance() const noexcept;
		DirectX::XMFLOAT3 CameraTarget() const noexcept;
		wi::ecs::Entity GroundEntity() const noexcept { return groundEntity_; }

	private:
		struct BuildingVisual
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity pad = wi::ecs::INVALID_ENTITY;
			economy::BuildingTypeId typeId{};
			double centerX{}, centerZ{}, width{}, depth{}, height{};
			double progress{1.0};
			bool constructionSite{};
		};
		struct RoadVisual
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			double centerX{}, centerZ{}, width{}, depth{};
			bool enabled{};
		};
		struct RailVisual { wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY; economy::Cell a{}, b{}; };
		struct UtilityVisual
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			economy::ResourceId resource{};
			economy::BuildingId a{}, b{};
			double centerX{}, centerZ{}, centerY{}, halfLength{}, yaw{};
		};
		struct VehicleVisual
		{
			wi::ecs::Entity body = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity bodyMesh = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity load = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity headlights = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity brakeLights = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity exhaust = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity label = wi::ecs::INVALID_ENTITY;
			wi::ecs::Entity labelMesh = wi::ecs::INVALID_ENTITY;
			std::array<wi::ecs::Entity, 4> wheels{};
			std::array<wi::ecs::Entity, 2> wagons{};
			std::array<wi::ecs::Entity, 2> wagonMeshes{};
			economy::Cell previous{};
			economy::Cell current{};
			float yaw{};
			float wheelRotation{};
			float wheelRotationBase{};
			bool hasPrevious{};
			bool loaded{};
			bool idle{};
			openrepublic::procgeo::VehicleKind kind = openrepublic::procgeo::VehicleKind::CargoTruck;
		};
		struct BuildingEffect
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			bool active{};
		};
		struct AmbientVehicleVisual { wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY; };

		bool initialized_ = false;
		bool terrainEnabled_ = false;
		wi::ecs::Entity waterEntity_ = wi::ecs::INVALID_ENTITY;
		openrepublic::runtime::SyntheticTerrain terrain_;
		wi::ecs::Entity groundEntity_ = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity truckMesh_ = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity wheelMesh_ = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity cargoMesh_ = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity headlightsMesh_ = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity brakeLightsMesh_ = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity exhaustMesh_ = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity ambientCarMesh_ = wi::ecs::INVALID_ENTITY;
		std::optional<runtime::NativeEconomyTransform> transform_;
		std::unordered_map<economy::BuildingId, BuildingVisual> buildings_;
		std::unordered_map<economy::RoadId, RoadVisual> roads_;
		std::unordered_map<economy::RailId, RailVisual> rails_;
		std::vector<UtilityVisual> utilityConnections_;
		std::unordered_map<economy::VehicleId, VehicleVisual> vehicles_;
		std::vector<AmbientVehicleVisual> ambientVehicles_;
		bool ambientTrafficEnabled_ = false;
		bool trafficPolishEnabled_ = false;
		bool darkLighting_ = false;
		bool SyncAmbientTraffic(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot, std::string& error);
		std::unordered_map<economy::BuildingId, BuildingEffect> buildingEffects_;
		std::unordered_map<wi::ecs::Entity, economy::BuildingId> buildingByEntity_;
		float cameraDistance_ = 20.0f;
		DirectX::XMFLOAT3 cameraTarget_{};
	};
}
