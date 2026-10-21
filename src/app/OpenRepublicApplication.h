#pragma once

#include "WickedEngine.h"
#include "openrepublic/sim/simulation.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace openrepublic
{
	// Owns the presentation path and coordinates it with the headless simulation.
	class OpenRepublicApplication final : public wi::Application
	{
	public:
		void SetSceneProbePath(std::filesystem::path path);
		void SetGlbProbePath(std::filesystem::path path);
		void SetTerrainProbePath(std::filesystem::path path);
		void Initialize() override;
		void Update(float dt) override;

	private:
		void SelectBuilding(wi::ecs::Entity selected);
		void PickAtPointer();
		void BeginFacilityPlacement(openrepublic::sim::FacilityType type);
		void CancelFacilityPlacement();
		void BeginRoadPlacement();
		void CancelRoadPlacement();
		std::string GetOpenRepublicId(wi::ecs::Entity entity) const;
		void UpdateSimulationPresentation();
		void UpdateStatusLabel();
		void ApplyPendingRules();
		void SyncPrototypeStorages(const openrepublic::sim::Snapshot& snapshot);
		void SyncFacilities(const openrepublic::sim::Snapshot& snapshot);
		void SyncRoads(const openrepublic::sim::Snapshot& snapshot);
		void SaveSimulation();
		void LoadSimulation();
		void ReloadLuaRules();
		void SetEntityColor(wi::ecs::Entity entity, const XMFLOAT4& color);

		wi::scene::Scene previewWorld;
		std::unique_ptr<wi::RenderPath3D> renderPath;
		openrepublic::sim::Simulation simulation;
		std::filesystem::path sceneProbePath;
		std::filesystem::path glbProbePath;
		std::filesystem::path terrainProbePath;
		wi::ecs::Entity farmBuilding = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity millBuilding = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity warehouseBuilding = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity truckEntity = wi::ecs::INVALID_ENTITY;
		struct RoadVisual
		{
			openrepublic::sim::EntityId id{};
			wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
		};
		std::vector<RoadVisual> roadVisuals;
		std::vector<wi::ecs::Entity> prototypeStorageEntities;
		std::vector<std::string> prototypeStorageIds;
		std::vector<wi::ecs::Entity> facilityEntities;
		std::vector<std::string> facilityIds;
		std::vector<openrepublic::sim::FacilityType> facilityTypes;
		wi::ecs::Entity selectedBuilding = wi::ecs::INVALID_ENTITY;
		wi::gui::Button pauseButton;
		wi::gui::Button stepButton;
		wi::gui::Button roadButton;
		wi::gui::Button buildRoadButton;
		wi::gui::Button placeStorageButton;
		wi::gui::Button buildFarmButton;
		wi::gui::Button buildMillButton;
		wi::gui::Button cancelPlacementButton;
		wi::gui::Button saveButton;
		wi::gui::Button loadButton;
		wi::gui::Button reloadRulesButton;
		wi::gui::Label simulationStatus;
		std::optional<openrepublic::sim::SimulationRules> pendingRules;
		std::optional<openrepublic::sim::FacilityType> activeFacilityPlacement;
		bool activeRoadPlacement = false;
		std::optional<std::pair<std::int32_t, std::int32_t>> pendingRoadStart;
		std::uint64_t lastConstructionResultCommandId = 0;
		float simulationAccumulator = 0.0f;
		bool simulationPaused = false;
		bool millApproachRoadEnabled = true;
		std::string uiNotice;
	};
}
