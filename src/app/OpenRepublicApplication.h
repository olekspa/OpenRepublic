#pragma once

#include "WickedEngine.h"
#include "openrepublic/sim/simulation.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openrepublic
{
	// Owns the presentation path and coordinates it with the headless simulation.
	class OpenRepublicApplication final : public wi::Application
	{
	public:
		void SetSceneProbePath(std::filesystem::path path);
		void SetGlbProbePath(std::filesystem::path path);
		void Initialize() override;
		void Update(float dt) override;

	private:
		void SelectBuilding(wi::ecs::Entity selected);
		void PickAtPointer();
		std::string GetOpenRepublicId(wi::ecs::Entity entity) const;
		void UpdateSimulationPresentation();
		void UpdateStatusLabel();
		void ApplyPendingRules();
		void SyncPrototypeStorages(const openrepublic::sim::Snapshot& snapshot);
		void SaveSimulation();
		void LoadSimulation();
		void ReloadLuaRules();
		void SetEntityColor(wi::ecs::Entity entity, const XMFLOAT4& color);

		wi::scene::Scene previewWorld;
		std::unique_ptr<wi::RenderPath3D> renderPath;
		openrepublic::sim::Simulation simulation;
		std::filesystem::path sceneProbePath;
		std::filesystem::path glbProbePath;
		wi::ecs::Entity farmBuilding = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity millBuilding = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity warehouseBuilding = wi::ecs::INVALID_ENTITY;
		wi::ecs::Entity truckEntity = wi::ecs::INVALID_ENTITY;
		std::array<wi::ecs::Entity, 4> roadSegments{};
		std::vector<wi::ecs::Entity> prototypeStorageEntities;
		std::vector<std::string> prototypeStorageIds;
		wi::ecs::Entity selectedBuilding = wi::ecs::INVALID_ENTITY;
		wi::gui::Button pauseButton;
		wi::gui::Button stepButton;
		wi::gui::Button roadButton;
		wi::gui::Button placeStorageButton;
		wi::gui::Button saveButton;
		wi::gui::Button loadButton;
		wi::gui::Button reloadRulesButton;
		wi::gui::Label simulationStatus;
		std::optional<openrepublic::sim::SimulationRules> pendingRules;
		float simulationAccumulator = 0.0f;
		bool simulationPaused = false;
		bool millApproachRoadEnabled = true;
		std::string uiNotice;
	};
}
