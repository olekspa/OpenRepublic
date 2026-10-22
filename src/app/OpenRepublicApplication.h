#pragma once

#include "WickedEngine.h"
#include "openrepublic/agent_api/agent_api.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/terrain_dds.h"
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
		OpenRepublicApplication();
		void SetSceneProbePath(std::filesystem::path path);
		void SetGlbProbePath(std::filesystem::path path);
		void SetNmfProbePath(std::filesystem::path path);
		void SetNmfTextureRoots(std::vector<std::filesystem::path> roots);
		void SetNmfMaterialPath(std::filesystem::path path);
		void SetMapPlacement(openrepublic::content::MapPlacementRequest request, double radiusMeters,
			bool regionPreview, std::filesystem::path contentRoot);
		void SetNmfUnlitPreview(bool enabled);
		void SetNmfMaterialUnlitPreview(bool enabled);
		void SetNmfCapturePaths(std::vector<std::filesystem::path> paths);
		void SetNmfCaptureZoom(float zoom);
		void SetNmfCaptureElevationDegrees(std::optional<float> elevationDegrees);
		void SetTerrainProbePath(std::filesystem::path path);
		void SetTerrainProbeTransform(std::optional<openrepublic::content::TerrainTransform> transform,
			std::string error = {});
		void Initialize() override;
		void Update(float dt) override;

	private:
		void SelectBuilding(wi::ecs::Entity selected);
		void PickAtPointer();
		void BeginFacilityPlacement(openrepublic::sim::FacilityType type);
		void CancelFacilityPlacement();
		void BeginStoragePlacement();
		void BeginRoadPlacement();
		void CancelRoadPlacement();
		std::string GetOpenRepublicId(wi::ecs::Entity entity) const;
		void UpdateSimulationPresentation();
		void UpdateStatusLabel();
		std::optional<openrepublic::sim::EntityId> GetSelectedSimulationId() const;
		void AssignTruckFarmEndpoint();
		void AssignTruckMillEndpoint();
		void AssignTruckStorageEndpoint();
		void SelectNextStorage();
		void QueueSelectedTruckPlan();
		void ApplyPendingRoadToggle();
		void ApplyPendingRules();
		void SyncPrototypeStorages(const openrepublic::sim::Snapshot& snapshot);
		void SyncFacilities(const openrepublic::sim::Snapshot& snapshot);
		void SyncRoads(const openrepublic::sim::Snapshot& snapshot);
		void SaveSimulation();
		void LoadSimulation();
		void ReloadLuaRules();
		openrepublic::sim::Snapshot QuerySimulationSnapshot() const;
		void SetEntityColor(wi::ecs::Entity entity, const XMFLOAT4& color);

		wi::scene::Scene previewWorld;
		std::unique_ptr<wi::RenderPath3D> renderPath;
		openrepublic::sim::Simulation simulation;
		openrepublic::agent_api::AgentApi agentApi;
		std::filesystem::path sceneProbePath;
		std::filesystem::path glbProbePath;
		std::filesystem::path nmfProbePath;
		std::vector<std::filesystem::path> nmfTextureRoots;
		std::filesystem::path nmfMaterialPath;
		std::optional<openrepublic::content::MapPlacementRequest> mapPlacement;
		double mapRadiusMeters = 0.0;
		bool mapRegionPreview = false;
		std::filesystem::path mapContentRoot;
		bool nmfUnlitPreview = false;
		bool nmfMaterialUnlitPreview = false;
		std::vector<std::filesystem::path> nmfCapturePaths;
		float nmfCaptureZoom = 1.0f;
		std::optional<float> nmfCaptureElevationDegrees;
		bool nmfCaptureMode = false;
		std::filesystem::path terrainProbePath;
		std::optional<openrepublic::content::TerrainTransform> terrainProbeTransform;
		std::string terrainProbeTransformError;
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
		wi::gui::Button setTruckFarmButton;
		wi::gui::Button setTruckMillButton;
		wi::gui::Button setTruckStorageButton;
		wi::gui::Button queueTruckPlanButton;
		wi::gui::Button selectNextStorageButton;
		wi::gui::Button saveButton;
		wi::gui::Button loadButton;
		wi::gui::Button reloadRulesButton;
		wi::gui::Label simulationStatus;
		std::optional<openrepublic::sim::SimulationRules> pendingRules;
		std::optional<openrepublic::sim::FacilityType> activeFacilityPlacement;
		bool activeStoragePlacement = false;
		bool activeRoadPlacement = false;
		openrepublic::sim::TruckPlan truckPlan;
		std::optional<std::pair<std::int32_t, std::int32_t>> pendingRoadStart;
		std::optional<bool> pendingMillApproachRoadState;
		std::uint64_t lastAgentEventSequence = 0;
		float simulationAccumulator = 0.0f;
		bool simulationPaused = false;
		bool millApproachRoadEnabled = true;
		std::string uiNotice;
	};
}
