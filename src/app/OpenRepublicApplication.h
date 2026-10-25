#pragma once

#include "WickedEngine.h"
#include "openrepublic/agent_api/agent_api.h"
#include "openrepublic/compat/region_import.h"
#include "openrepublic/compat/catalog_import.h"
#include "EconomyOverlay.h"
#include "BuildingLodController.h"
#include "NativeEconomyScene.h"
#include "NativeEconomyTools.h"
#include "NativeEconomyHud.h"
#include "openrepublic/economy/checkpoint.h"
#include "ShowcaseEnvironment.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/economy_api/economy_api.h"
#include "openrepublic/runtime/graphics_settings.h"
#include "openrepublic/runtime/native_economy_clock.h"
#include "openrepublic/runtime/economy_overview.h"
#include "openrepublic/runtime/native_coal_demo.h"
#include "openrepublic/runtime/native_objectives.h"
#include "openrepublic/runtime/save_slots.h"
#include "openrepublic/runtime/native_construction.h"
#include "openrepublic/content/terrain_dds.h"
#include "openrepublic/sim/simulation.h"
#include "../economy_host/economy_protocol.h"
#include "LocalEconomyPipeEndpoint.h"
#include "openrepublic/scene_control.h"
#ifdef OPENREPUBLIC_WITH_LUA_RULES
#include "openrepublic/scripting/scene_script.h"
#endif

#include <filesystem>
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
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
		void SetGraphicsSettings(openrepublic::runtime::GraphicsSettings settings);
		void SetEconomyOverlay(std::string host, int port);
		void SetLocalEconomyPipeName(std::wstring name);
		void SetRegionCatalogImportOptions(openrepublic::compat::CatalogImportOptions options);
		void SetMapPlacement(openrepublic::content::MapPlacementRequest request, double radiusMeters,
			bool regionPreview, std::filesystem::path contentRoot, int regionModelLevel,
			bool dynamicLod = false, float lodNearDistance = 250.0f, float lodFarDistance = 700.0f);
		void SetSyntheticRegionFixture();
		void SetEconomyCheckpoint(openrepublic::economy::RestoredCheckpoint checkpoint, bool freshWorld = false);
		void SetAmbientTraffic(bool enabled) noexcept { nativeEconomyScene.SetAmbientTraffic(enabled); }
		void SetNativeTrafficPolish(bool enabled) noexcept { nativeEconomyScene.SetTrafficPolish(enabled); }
		void SetDarkSceneLighting(bool dark) noexcept { nativeEconomyScene.SetDarkLighting(dark); }
		void SetNativeTerrainEnabled(bool enabled);
		void SetNativeRailEnabled(bool enabled) noexcept { nativeRailEnabled = enabled; }
		void SetNativeObjectives(openrepublic::runtime::NativeObjectiveSet objectiveSet);
		void SetMapShowcaseSun(float elevationDegrees, float yawOffsetDegrees);
		void SetScenePipeName(std::wstring name);
		void SetSceneScriptPath(std::filesystem::path path);
		void SetMapShowcaseCapture(std::filesystem::path outputPath, bool enhancedLook,
			float cameraYawDegrees, float cameraElevationDegrees, float cameraDistanceScale,
			bool vegetationWind = true, bool grassLayer = true);
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
		bool ConfirmQuit();

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
		void UpdateRegionStatus();
		std::string SelectedUtilityStatus(bool expanded) const;
		void PumpLocalEconomyRequests();
		void StartLocalEconomyEndpoint();
		void RefreshRegionEconomy();
		void UpdateRegionEconomyPresentation();
		std::optional<openrepublic::preview::EconomyBuildingStatus> GetRegionBuildingForInstance(
			wi::ecs::Entity instance) const;
		bool QueueRegionStep(std::uint32_t ticks, bool automatic = false);
		void FrameNativeEconomyWorld();
		void QueueRegionBuildingToggle();
		void SetRegionHaulSource();
		void QueueRegionHaul();
		void BeginNativeBuildingPlacement();
		void BeginNativeRoadPlacement();
		void BeginNativeStationPlacement();
		void QueueNativeRailBuild();
		void QueueNativeTrainDispatch();
		void CancelNativeConstruction();
		void QueueNativeRemoval();
		void QueueNativeConstruction(economy::Cell cell);
		std::optional<economy::Cell> NativeCellAtPointer() const;
		void UpdateNativeConstructionPreview(bool pointerAvailable);
		void ClearNativeConstructionPreview();
		void RefreshNativeHaulChoices();
		openrepublic::economy::World* ActiveOwnedEconomyWorld() noexcept;
		const openrepublic::economy::World* ActiveOwnedEconomyWorld() const noexcept;
		void QueueNativeOrderChange(bool retarget);
		void QueueNativeDispatch();
		void QueueNativeCoalImport();
		void UpdateNativeCoalAutomation();
		void RestartNativeObjectives();
		void QueueNativeUtilityConnection(bool connect);
		void QueueNativeRoadToggle();
		void SaveNativeCheckpoint();
		void LoadNativeSaveSlot(std::size_t index);
		void StartNativeNewGame(const wchar_t* arguments);
		void RefreshNativeSaveSlots();
		bool LaunchNativeSession(const std::wstring& arguments);
		void UpdateNativeAutosave();
		void LayoutNativeControls();
		void ToggleNativeRunPause();
		bool TrackNativeSubmission(const economy_api::Submission& submitted, const std::string& action);
		bool CanQueueNativeCommand();

		wi::scene::Scene previewWorld;
		std::unique_ptr<wi::RenderPath3D> renderPath;
		openrepublic::sim::Simulation simulation;
		openrepublic::agent_api::AgentApi agentApi;
		// Keep the checkpoint's heap catalog/world alive until after API and endpoint teardown.
		openrepublic::economy::RestoredCheckpoint nativeEconomyCheckpoint;
		openrepublic::preview::NativeEconomyScene nativeEconomyScene;
		openrepublic::preview::NativeEconomyTools nativeEconomyTools;
		openrepublic::preview::NativeEconomyHud nativeEconomyHud;
		bool nativeEconomyFreshWorld = false;
		bool nativeCoalDemo = false;
		bool nativeCoalAutoRun = false;
		std::optional<openrepublic::runtime::NativeObjectiveSet> nativeObjectiveSet;
		std::string nativeObjectiveBaseline;
		openrepublic::economy::BuildingId coalDemoBorder{}, coalDemoDepot{}, coalDemoStorage{}, coalDemoPlant{};
		openrepublic::economy::ResourceId coalDemoResource{};
		openrepublic::economy::ResourceId coalDemoGrain{};
		enum class NativeConstructionMode { None, Building, Road, Remove };
		NativeConstructionMode nativeConstructionMode = NativeConstructionMode::None;
		economy::BuildingTypeId nativePlacementType = economy::InvalidBuildingTypeId;
		std::optional<economy::Cell> nativeRoadStart;
		std::optional<economy::Cell> nativeHoverCell;
		wi::ecs::Entity nativeConstructionPreview = wi::ecs::INVALID_ENTITY;
		float nativeStatusTop = 126.0f;
		bool nativeToolsReady = false;
		bool nativeDetailsExpanded = false;
		bool nativeOverviewExpanded = false;
		wi::gui::Button nativeDetailsButton;
		wi::gui::Button nativeOverviewButton;
		wi::gui::Button nativeSaveButton;
		wi::gui::Button nativeObjectiveRestartButton;
		wi::gui::Label nativeObjectiveLabel;
		wi::gui::Button nativeQuitButton;
		wi::gui::Button nativeGameMenuButton;
		std::array<wi::gui::Button, 3> nativeNewGameButtons;
		std::array<wi::gui::Button, 8> nativeSaveSlotButtons;
		std::array<wi::gui::Button, 2> nativeSavePageButtons;
		std::vector<runtime::SaveSlotInfo> nativeSaveSlots;
		std::size_t nativeSaveSlotOffset = 0;
		std::filesystem::path nativeSaveDirectory;
		bool nativeGameMenuExpanded = false;
		bool nativeAllowClose = false;
		std::uint64_t nativeSavedRevision = 0;
		std::uint64_t nextNativeAutosaveTick = 2400;
		std::size_t nextNativeAutosaveSlot = 0;
		// Lifetime order is intentional: EconomyApi references regionEconomyWorld,
		// which references regionEconomyCatalog. Destruction runs in reverse order.
		openrepublic::economy::Catalog regionEconomyCatalog;
		openrepublic::compat::CatalogImportOptions regionCatalogImportOptions;
		std::unique_ptr<openrepublic::economy::World> regionEconomyWorld;
		std::unique_ptr<openrepublic::economy_api::EconomyApi> regionEconomyApi;
		// Destruction order is intentional: stop/join the endpoint before destroying
		// its dispatcher, shared API, world, and catalog.
		std::unique_ptr<openrepublic::economy_host::Dispatcher> localEconomyDispatcher;
		std::unique_ptr<openrepublic::economy_host::LocalEconomyPipeEndpoint> localEconomyPipeEndpoint;
		std::wstring localEconomyPipeName;
		openrepublic::compat::RegionImportReport regionEconomyImport;
		std::optional<openrepublic::runtime::NativeEconomyTransform> regionVehicleTransform;
		std::optional<openrepublic::economy_api::WorldView> regionEconomySnapshot;
		std::deque<openrepublic::economy::WorldSnapshot> nativeOverviewHistory;
		std::optional<openrepublic::economy::WorldSnapshot> nativeHudSnapshot;
		std::vector<std::uint64_t> regionPendingCommandHandles;
		std::vector<wi::ecs::Entity> regionTruckEntities;
		std::string regionEconomyDescription;
		std::optional<std::uint64_t> lastPresentedRegionRevision;
		std::filesystem::path sceneProbePath;
		std::filesystem::path glbProbePath;
		std::filesystem::path nmfProbePath;
		std::vector<std::filesystem::path> nmfTextureRoots;
		std::filesystem::path nmfMaterialPath;
		std::optional<openrepublic::content::MapPlacementRequest> mapPlacement;
		double mapRadiusMeters = 0.0;
		bool mapRegionPreview = false;
		bool syntheticRegionFixtureMode = false;
		bool nativeEconomyCheckpointMode = false;
		bool nativeTerrainEnabled = false;
		bool nativeRailEnabled = false;
		bool nativeEconomyPresentationValid = false;
		double syntheticRegionCenterX = 0.0;
		double syntheticRegionCenterZ = 0.0;
		int mapRegionModelLevel = 0;
		bool mapRegionDynamicLod = false;
		float mapLodNearDistance = 250.0f;
		float mapLodFarDistance = 700.0f;
		BuildingLodController buildingLodController;
		bool mapShowcaseCaptureMode = false;
		bool mapShowcaseEnhancedLook = false;
		bool showcaseVegetationWind = true;
		bool showcaseGrassLayer = true;
		std::filesystem::path mapShowcaseCapturePath;
		float mapShowcaseCameraYawDegrees = 45.0f;
		float mapShowcaseCameraElevationDegrees = 24.5f;
		float mapShowcaseCameraDistanceScale = 1.5f;
		// Sun placement for the enhanced look: elevation above the horizon and the horizontal
		// angle from the camera's outward direction. NaN keeps the camera-aligned legacy sun.
		float mapShowcaseSunElevationDegrees = std::numeric_limits<float>::quiet_NaN();
		float mapShowcaseSunYawOffsetDegrees = 0.0f;
		preview::ShowcaseEnvironmentInfo showcaseEnvironment;
		// Runtime look/camera control (versioned JSON over a local named pipe; see docs/SCENE_CONTROL.md).
		std::wstring scenePipeName;
		std::unique_ptr<openrepublic::scenectl::SceneControlState> sceneControl;
		std::unique_ptr<openrepublic::economy_host::LocalEconomyPipeEndpoint> sceneEndpoint;
		wi::ecs::Entity showcaseSunEntity = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity showcaseWeatherEntity = wi::ecs::INVALID_ENTITY;
		XMFLOAT3 sceneCameraTarget{};
		float sceneCameraDistance = 1.0f;
		bool sceneCameraKnown = false;
		wi::ecs::Entity regionTruckMesh = wi::ecs::INVALID_ENTITY;
		std::vector<std::array<double, 3>> regionTruckPose;
		std::filesystem::path sceneScriptPath;
#ifdef OPENREPUBLIC_WITH_LUA_RULES
		std::unique_ptr<openrepublic::scripting::SceneScript> sceneScript;
#endif
		void UpdateSceneControl(float dt);
		void ApplySceneLook(const openrepublic::scenectl::SceneLook& look);
		float cameraSpeedScale = 1.0f;
		std::string economyHost;
		int economyPort = 0;
		std::unique_ptr<openrepublic::preview::EconomyOverlay> economyOverlay;
		std::vector<openrepublic::preview::RegionInstance> regionInstances;
		std::optional<openrepublic::runtime::GraphicsSettings> graphicsSettings;
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
		wi::gui::Button regionStepOneButton;
		wi::gui::Button regionStepButton;
		wi::gui::Button regionToggleButton;
		wi::gui::Button regionHaulSourceButton;
		wi::gui::Button regionHaulButton;
		wi::gui::Button nativeFrameButton;
		wi::gui::Button nativeCameraFollowButton;
		bool nativeCameraFollow = false;
		bool nativeHudOverview = false;
		wi::gui::Button nativeStepManyButton;
		wi::gui::Button nativeRunPauseButton;
		bool nativeEconomyRunning = false;
		openrepublic::runtime::NativeEconomyClock nativeEconomyRunClock;
		std::uint32_t nativeEconomySpeed = 1;
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
		wi::ecs::Entity selectedRegionBuildingEntity = wi::ecs::INVALID_ENTITY;
		std::optional<std::uint64_t> selectedRegionBuildingId;
		std::optional<std::uint64_t> regionHaulSourceId;
		std::string regionWorldStatus;
		std::string uiNotice;
	};
}
