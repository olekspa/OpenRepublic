#pragma once

#include "WickedEngine.h"
#include "EconomyOverlay.h"
#include "openrepublic/economy/economy.h"
#include "openrepublic/runtime/native_economy_layout.h"

#include <optional>
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
			const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error);
		bool Sync(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error);
		// Re-centers native geometry around the current snapshot. The caller can use
		// this for an explicit framing action while leaving ordinary Sync frozen.
		bool Rebase(wi::scene::Scene& scene, const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog, std::vector<RegionInstance>& outInstances, std::string& error);

		std::optional<economy::BuildingId> BuildingForEntity(wi::ecs::Entity entity) const;
		DirectX::XMFLOAT3 CellPosition(economy::Cell cell) const;
		bool CellFromGroundPoint(double worldX, double worldZ, std::uint32_t gridWidth,
			std::uint32_t gridDepth, economy::Cell& outCell) const noexcept;
		float CameraDistance() const noexcept;
		DirectX::XMFLOAT3 CameraTarget() const noexcept;
		wi::ecs::Entity GroundEntity() const noexcept { return groundEntity_; }

	private:
		struct BuildingVisual
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			economy::BuildingTypeId typeId{};
			double centerX{}, centerZ{}, width{}, depth{}, height{};
		};
		struct RoadVisual
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			double centerX{}, centerZ{}, width{}, depth{};
			bool enabled{};
		};
		struct UtilityVisual
		{
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
			economy::ResourceId resource{};
			economy::BuildingId a{}, b{};
			double centerX{}, centerZ{}, centerY{}, halfLength{}, yaw{};
		};

		bool initialized_ = false;
		wi::ecs::Entity groundEntity_ = wi::ecs::INVALID_ENTITY;
		std::optional<runtime::NativeEconomyTransform> transform_;
		std::unordered_map<economy::BuildingId, BuildingVisual> buildings_;
		std::unordered_map<economy::RoadId, RoadVisual> roads_;
		std::vector<UtilityVisual> utilityConnections_;
		std::unordered_map<wi::ecs::Entity, economy::BuildingId> buildingByEntity_;
		float cameraDistance_ = 20.0f;
		DirectX::XMFLOAT3 cameraTarget_{};
	};
}
