#include "OpenRepublicApplication.h"
#include "ModelImporter.h"
#include "openrepublic/content/terrain_dds.h"

#ifdef OPENREPUBLIC_WITH_LUA_RULES
#include "openrepublic/scripting/lua_rules_loader.h"
#endif

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

namespace openrepublic
{
	namespace
	{
		constexpr float kSimulationTickSeconds = 0.25f;
		constexpr float kCameraMoveSpeed = 5.0f;
		constexpr float kMaxCameraMoveDeltaSeconds = 0.1f;
		constexpr float kWheelZoomSpeed = 0.9f;
		// Construction cells share integer coordinates with road service nodes so
		// player-placed facilities/storage can occupy an explicit truck endpoint.
		constexpr float kPlacementGridSpacing = openrepublic::sim::Simulation::RoadCellWorldSize;
		constexpr float kPlacementGridHalfExtent =
			(static_cast<float>(openrepublic::sim::Simulation::PrototypeGridMax) + 0.5f) * kPlacementGridSpacing;
		constexpr float kLegacyOverlapDisplayOffset = 0.75f * kPlacementGridSpacing;
		constexpr float kRoadGridHalfExtent =
			(static_cast<float>(openrepublic::sim::Simulation::PrototypeGridMax) + 0.5f) *
			openrepublic::sim::Simulation::RoadCellWorldSize;
		constexpr XMFLOAT4 kFarmColor(0.24f, 0.52f, 0.28f, 1.0f);
		constexpr XMFLOAT4 kMillColor(0.72f, 0.43f, 0.20f, 1.0f);
		constexpr XMFLOAT4 kWarehouseColor(0.43f, 0.50f, 0.58f, 1.0f);
		constexpr XMFLOAT4 kPrototypeStorageColor(0.32f, 0.55f, 0.70f, 1.0f);
		constexpr XMFLOAT4 kPlacedFarmColor(0.36f, 0.68f, 0.34f, 1.0f);
		constexpr XMFLOAT4 kPlacedMillColor(0.82f, 0.54f, 0.27f, 1.0f);
		constexpr XMFLOAT4 kSelectedBuildingColor(1.0f, 0.78f, 0.20f, 1.0f);
		constexpr XMFLOAT4 kRoadColor(0.22f, 0.24f, 0.27f, 1.0f);
		constexpr XMFLOAT4 kClosedRoadColor(0.68f, 0.20f, 0.16f, 1.0f);
		constexpr wi::Color kUiButtonIdle(36, 60, 80, 255);
		constexpr wi::Color kUiButtonFocus(52, 88, 116, 255);
		constexpr wi::Color kUiButtonActive(28, 46, 62, 255);
		constexpr wi::Color kUiTextColor(242, 247, 255, 255);
		constexpr char kSimulationSaveFile[] = "OpenRepublic-demo.save";
		constexpr char kDemoRulesFile[] = "data/rules/demo_rules.lua";
		constexpr std::uint32_t kTerrainPreviewMaxSide = 257;
		constexpr double kTerrainDisplayUnitsPerMeter = 0.001;
		constexpr double kTerrainVerticalPreviewScale = 0.005; // 5x vertical exaggeration at the display scale.
		constexpr double kRawPreviewVerticalScale = 5.0;
		constexpr double kMaxTerrainPreviewHeight = 1000.0;
		// Current synthetic storage capacity also shown by the demo stock readout.
		constexpr std::uint32_t kStorageFlourCapacity = 5;
		constexpr std::uintmax_t kMaxTerrainMetadataBytes = 64u * 1024u;

		bool IsBuiltInDemoCell(std::int32_t gridX, std::int32_t gridZ)
		{
			return gridZ == 0 && (gridX == -2 || gridX == 0 || gridX == 2);
		}

		float GetSyntheticEntityDisplayZ(std::int32_t gridX, std::int32_t gridZ)
		{
			const float z = static_cast<float>(gridZ) * kPlacementGridSpacing;
			// v1-v6 permitted player entities to share the demo Farm/Mill/Warehouse
			// cells. Keep their logical grid positions for routing while separating
			// the cubes spatially for picking; actual pointer selection remains a
			// separate runtime acceptance check.
			return z + (IsBuiltInDemoCell(gridX, gridZ) ? kLegacyOverlapDisplayOffset : 0.0f);
		}

		bool CreateTerrainPreview(wi::scene::Scene& scene, const std::filesystem::path& heightmapPath,
			wi::ecs::Entity& terrainEntity, std::string& description, std::string& error)
		{
			openrepublic::content::Heightmap heightmap;
			if (!openrepublic::content::LoadLegacyR32F(heightmapPath, heightmap, &error))
			{
				return false;
			}
			if (heightmap.width < 2 || heightmap.height < 2)
			{
				error = "terrain preview requires at least a 2 x 2 heightmap";
				return false;
			}

			const std::filesystem::path metadataPath = heightmapPath.parent_path() / L"script.ini";
			openrepublic::content::TerrainMetadata metadata;
			bool hasMetadata = false;
			std::error_code filesystemError;
			const bool metadataExists = std::filesystem::exists(metadataPath, filesystemError);
			if (filesystemError)
			{
				error = "could not check for adjacent terrain script.ini metadata";
				return false;
			}
			if (metadataExists)
			{
				const auto metadataBytes = std::filesystem::file_size(metadataPath, filesystemError);
				if (filesystemError || metadataBytes > kMaxTerrainMetadataBytes)
				{
					error = "adjacent terrain script.ini is unreadable or exceeds the 64 KiB limit";
					return false;
				}
				std::ifstream metadataStream(metadataPath, std::ios::binary);
				if (!metadataStream)
				{
					error = "could not open adjacent terrain script.ini";
					return false;
				}
				std::string metadataText(static_cast<std::size_t>(metadataBytes), '\0');
				metadataStream.read(metadataText.data(), static_cast<std::streamsize>(metadataText.size()));
				if (!metadataStream || metadataStream.peek() != std::char_traits<char>::eof())
				{
					error = "terrain script.ini changed during read or could not be read completely";
					return false;
				}
				if (!openrepublic::content::ParseTerrainMetadata(metadataText, heightmap, metadata, &error))
				{
					return false;
				}
				hasMetadata = true;
			}

			double minimumHeight = std::numeric_limits<double>::infinity();
			double maximumHeight = -std::numeric_limits<double>::infinity();
			for (const float rawHeight : heightmap.samples)
			{
				const double height = hasMetadata
					? static_cast<double>(rawHeight) * metadata.heightScale + metadata.worldPositionY
					: static_cast<double>(rawHeight);
				minimumHeight = std::min(minimumHeight, height);
				maximumHeight = std::max(maximumHeight, height);
			}

			const double verticalScale = hasMetadata ? kTerrainVerticalPreviewScale : kRawPreviewVerticalScale;
			const double verticalSpan = (maximumHeight - minimumHeight) * verticalScale;
			if (!std::isfinite(verticalSpan) || verticalSpan > kMaxTerrainPreviewHeight)
			{
				error = "height range is outside the safe preview bounds";
				return false;
			}

			const std::uint32_t previewWidth = std::min(heightmap.width, kTerrainPreviewMaxSide);
			const std::uint32_t previewHeight = std::min(heightmap.height, kTerrainPreviewMaxSide);
			const double defaultWidth = std::max(1.0, static_cast<double>(heightmap.width) * 0.01);
			const double defaultDepth = std::max(1.0, static_cast<double>(heightmap.height) * 0.01);
			const double displayWidth = std::clamp(hasMetadata ? metadata.worldSizeX * kTerrainDisplayUnitsPerMeter : defaultWidth, 1.0, 100.0);
			const double displayDepth = std::clamp(hasMetadata ? metadata.worldSizeY * kTerrainDisplayUnitsPerMeter : defaultDepth, 1.0, 100.0);

			std::vector<XMFLOAT3> positions;
			positions.reserve(static_cast<std::size_t>(previewWidth) * previewHeight);
			for (std::uint32_t z = 0; z < previewHeight; ++z)
			{
				const std::uint64_t sourceZNumerator = static_cast<std::uint64_t>(z) * (heightmap.height - 1u);
				const std::uint32_t sourceZ = static_cast<std::uint32_t>((sourceZNumerator + (previewHeight - 1u) / 2u) / (previewHeight - 1u));
				for (std::uint32_t x = 0; x < previewWidth; ++x)
				{
					const std::uint64_t sourceXNumerator = static_cast<std::uint64_t>(x) * (heightmap.width - 1u);
					const std::uint32_t sourceX = static_cast<std::uint32_t>((sourceXNumerator + (previewWidth - 1u) / 2u) / (previewWidth - 1u));
					const std::size_t sourceIndex = static_cast<std::size_t>(sourceZ) * heightmap.width + sourceX;
					const double sourceHeight = hasMetadata
						? static_cast<double>(heightmap.samples[sourceIndex]) * metadata.heightScale + metadata.worldPositionY
						: static_cast<double>(heightmap.samples[sourceIndex]);
					const double displayY = (sourceHeight - minimumHeight) * verticalScale;
					if (!std::isfinite(displayY) || displayY > kMaxTerrainPreviewHeight)
					{
						error = "a terrain vertex is outside the safe preview bounds";
						return false;
					}
					const float u = static_cast<float>(x) / static_cast<float>(previewWidth - 1u);
					const float v = static_cast<float>(z) / static_cast<float>(previewHeight - 1u);
					positions.emplace_back(
						static_cast<float>((static_cast<double>(u) - 0.5) * displayWidth),
						static_cast<float>(displayY),
						static_cast<float>((static_cast<double>(v) - 0.5) * displayDepth));
				}
			}

			std::vector<std::uint32_t> indices;
			indices.reserve(static_cast<std::size_t>(previewWidth - 1u) * (previewHeight - 1u) * 6u);
			for (std::uint32_t z = 0; z + 1u < previewHeight; ++z)
			{
				for (std::uint32_t x = 0; x + 1u < previewWidth; ++x)
				{
					const std::uint32_t a = z * previewWidth + x;
					const std::uint32_t b = a + 1u;
					const std::uint32_t c = a + previewWidth;
					const std::uint32_t d = c + 1u;
					// Match Wicked Engine's Entity_CreatePlane winding so the top surface is front-facing.
					indices.insert(indices.end(), { a, b, c, b, d, c });
				}
			}

			terrainEntity = scene.Entity_CreateMeshFromData("OpenRepublic bounded terrain preview",
				indices.size(), indices.data(), positions.size(), positions.data());
			if (wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(terrainEntity))
			{
				mesh->CreateRenderData();
			}
			else
			{
				error = "Wicked Engine did not create the terrain mesh component";
				return false;
			}
			if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(terrainEntity))
			{
				material->SetBaseColor(XMFLOAT4(0.34f, 0.48f, 0.27f, 1.0f));
				material->CreateRenderData(true);
			}
			else
			{
				error = "Wicked Engine did not create a terrain material component";
				return false;
			}

			std::ostringstream summary;
			summary << "Terrain preview " << heightmap.width << 'x' << heightmap.height << " R32F -> "
				<< previewWidth << 'x' << previewHeight << " mesh; centered, row order preserved";
			if (hasMetadata)
			{
				summary << "; candidate height raw*" << metadata.heightScale << " + offset " << metadata.worldPositionY
					<< " (5x vertical preview exaggeration; coordinates/orientation unverified)";
			}
			else
			{
				summary << "; raw sample-height preview (no adjacent script.ini metadata)";
			}
			description = summary.str();
			return true;
		}

		bool HasExtensionInsensitive(const std::filesystem::path& path, const wchar_t* extension)
		{
			std::wstring actual = path.extension().wstring();
			std::transform(actual.begin(), actual.end(), actual.begin(), [](wchar_t value) {
				return static_cast<wchar_t>(std::towlower(value));
			});
			std::wstring expected(extension);
			std::transform(expected.begin(), expected.end(), expected.begin(), [](wchar_t value) {
				return static_cast<wchar_t>(std::towlower(value));
			});
			return actual == expected;
		}

		void StyleButton(wi::gui::Button& button)
		{
			button.SetColor(kUiButtonIdle, wi::gui::IDLE);
			button.SetColor(kUiButtonFocus, wi::gui::FOCUS);
			button.SetColor(kUiButtonActive, wi::gui::ACTIVE);
			button.SetColor(kUiButtonIdle, wi::gui::DEACTIVATING);
			button.font.params.color = kUiTextColor;
		}

		const char* ResourceName(openrepublic::sim::Resource resource)
		{
			switch (resource)
			{
			case openrepublic::sim::Resource::Grain: return "grain";
			case openrepublic::sim::Resource::Flour: return "flour";
			default: return "empty";
			}
		}

		std::filesystem::path LocateDemoRulesFile()
		{
			const std::filesystem::path workingPath = std::filesystem::current_path() / kDemoRulesFile;
			std::error_code error;
			if (std::filesystem::is_regular_file(workingPath, error)) return workingPath;

			std::wstring executablePath(32768, L'\0');
			const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
			if (length > 0 && length < executablePath.size())
			{
				executablePath.resize(length);
				const auto executableDirectory = std::filesystem::path(executablePath).parent_path();
				const auto bundledPath = executableDirectory / kDemoRulesFile;
				error.clear();
				if (std::filesystem::is_regular_file(bundledPath, error)) return bundledPath;
			}
			return workingPath;
		}
	}

	void OpenRepublicApplication::SetSceneProbePath(std::filesystem::path path)
	{
		sceneProbePath = std::move(path);
	}

	void OpenRepublicApplication::SetGlbProbePath(std::filesystem::path path)
	{
		glbProbePath = std::move(path);
	}

	void OpenRepublicApplication::SetTerrainProbePath(std::filesystem::path path)
	{
		terrainProbePath = std::move(path);
	}

	void OpenRepublicApplication::Initialize()
	{
		wi::Application::Initialize();
		bool terrainPreviewLoaded = false;

		#ifdef OPENREPUBLIC_WITH_LUA_RULES
		{
			openrepublic::sim::SimulationRules initialRules;
			std::string error;
			const auto rulesPath = LocateDemoRulesFile();
			if (openrepublic::scripting::LuaRulesLoader::LoadFile(rulesPath, initialRules, &error) &&
				simulation.SetRules(initialRules, &error))
			{
				uiNotice = "Loaded project rules from " + rulesPath.string() + ".";
			}
			else
			{
				uiNotice = "Lua rules unavailable; using safe defaults: " + error;
			}
		}
		#endif

		// Synthetic OpenRepublic-authored content only. The view follows snapshots;
		// no Wicked Engine scene object owns or changes simulation state.
		const wi::ecs::Entity ground = previewWorld.Entity_CreatePlane("OpenRepublic synthetic construction ground");
		if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(ground))
		{
			// The buildable area and visible synthetic ground use the same finite grid bounds.
			const float groundHalfExtent = std::max(kPlacementGridHalfExtent, kRoadGridHalfExtent);
			transform->Scale(XMFLOAT3(groundHalfExtent, 1.0f, groundHalfExtent));
			transform->UpdateTransform();
		}
		SetEntityColor(ground, XMFLOAT4(0.20f, 0.27f, 0.20f, 1.0f));

		auto createBlock = [this](const char* name, float x, float y, const XMFLOAT3& scale, const XMFLOAT4& color)
		{
			const wi::ecs::Entity entity = previewWorld.Entity_CreateCube(name);
			if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(entity))
			{
				transform->Scale(scale);
				transform->Translate(XMFLOAT3(x, y, 0));
				transform->UpdateTransform();
			}
			SetEntityColor(entity, color);
			return entity;
		};

		farmBuilding = createBlock("OpenRepublic synthetic farm", -4.0f, 0.72f, XMFLOAT3(0.62f, 0.72f, 0.62f), kFarmColor);
		millBuilding = createBlock("OpenRepublic synthetic mill", 0.0f, 0.82f, XMFLOAT3(0.82f, 0.82f, 0.82f), kMillColor);
		warehouseBuilding = createBlock("OpenRepublic synthetic warehouse", 4.0f, 0.72f, XMFLOAT3(0.72f, 0.72f, 0.72f), kWarehouseColor);

		truckEntity = createBlock("OpenRepublic synthetic truck", -4.0f, 0.28f, XMFLOAT3(0.25f, 0.25f, 0.25f), XMFLOAT4(0.85f, 0.72f, 0.18f, 1.0f));

		const wi::ecs::Entity sun = previewWorld.Entity_CreateLight(
			"OpenRepublic preview light", XMFLOAT3(0, 0, 0), XMFLOAT3(1.0f, 0.88f, 0.72f),
			4.0f, 100.0f, wi::scene::LightComponent::DIRECTIONAL);
		if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(sun))
		{
			transform->RotateRollPitchYaw(XMFLOAT3(-0.7f, 0.6f, 0));
			transform->UpdateTransform();
		}

		if (!sceneProbePath.empty())
		{
			if (!HasExtensionInsensitive(sceneProbePath, L".wiscene"))
			{
				uiNotice = "Scene probe accepts only Wicked Engine .wiscene files.";
			}
			else if (std::error_code fileError; !std::filesystem::is_regular_file(sceneProbePath, fileError))
			{
				uiNotice = "Scene probe file not found: " + sceneProbePath.string();
			}
			else
			{
				const std::size_t transformsBefore = previewWorld.transforms.GetCount();
				wi::scene::LoadModel2(previewWorld, sceneProbePath.u8string(), XMMatrixTranslation(0, 0, 3));
				const std::size_t transformsAfter = previewWorld.transforms.GetCount();
				if (transformsAfter > transformsBefore)
				{
					uiNotice = "Loaded local WISCENE probe; added " + std::to_string(transformsAfter - transformsBefore) + " transforms.";
				}
				else
				{
					uiNotice = "WISCENE probe added no scene transforms.";
				}
			}
			wilog("[OpenRepublic] Scene probe: %s", uiNotice.c_str());
		}

		if (!glbProbePath.empty())
		{
			if (!HasExtensionInsensitive(glbProbePath, L".glb"))
			{
				uiNotice = "GLB probe accepts only .glb files.";
			}
			else if (std::error_code fileError; !std::filesystem::is_regular_file(glbProbePath, fileError))
			{
				uiNotice = "GLB probe file not found: " + glbProbePath.string();
			}
			else
			{
				const std::size_t transformsBefore = previewWorld.transforms.GetCount();
				const std::size_t meshesBefore = previewWorld.meshes.GetCount();
				const std::size_t materialsBefore = previewWorld.materials.GetCount();
				ImportModel_GLTF(glbProbePath.u8string(), previewWorld);
				const std::size_t transformCount = previewWorld.transforms.GetCount() - transformsBefore;
				const std::size_t meshCount = previewWorld.meshes.GetCount() - meshesBefore;
				const std::size_t materialCount = previewWorld.materials.GetCount() - materialsBefore;
				if (meshCount > 0)
				{
					uiNotice = "Imported local GLB probe: " + std::to_string(meshCount) + " meshes, " +
						std::to_string(materialCount) + " materials, " + std::to_string(transformCount) + " transforms.";
				}
				else
				{
					uiNotice = "GLB probe imported no meshes; see the Wicked Engine log for importer diagnostics.";
				}
			}
			wilog("[OpenRepublic] GLB probe: %s", uiNotice.c_str());
		}

		if (!terrainProbePath.empty())
		{
			wi::ecs::Entity terrainEntity = wi::ecs::INVALID_ENTITY;
			std::string description;
			std::string error;
			if (CreateTerrainPreview(previewWorld, terrainProbePath, terrainEntity, description, error))
			{
				terrainPreviewLoaded = true;
				uiNotice = description;
			}
			else
			{
				uiNotice = "Terrain probe failed: " + error;
			}
			wilog("[OpenRepublic] Terrain probe: %s", uiNotice.c_str());
		}

		renderPath = std::make_unique<wi::RenderPath3D>();
		renderPath->scene = &previewWorld;
		renderPath->setBloomEnabled(false);
		renderPath->setDepthOfFieldEnabled(false);

		wi::gui::GUI& gui = renderPath->GetGUI();
		pauseButton.Create("SimulationPauseButton");
		pauseButton.SetText("Pause simulation");
		StyleButton(pauseButton);
		pauseButton.SetSize(XMFLOAT2(130, 28));
		pauseButton.SetPos(XMFLOAT2(18, 18));
		pauseButton.OnClick([this](const wi::gui::EventArgs&) {
			simulationPaused = !simulationPaused;
			simulationAccumulator = 0.0f;
			pauseButton.SetText(simulationPaused ? "Resume simulation" : "Pause simulation");
			uiNotice = simulationPaused ? "Simulation paused." : "Simulation resumed.";
			UpdateStatusLabel();
		});
		gui.AddWidget(&pauseButton);

		stepButton.Create("SimulationStepButton");
		stepButton.SetText("Step one tick");
		StyleButton(stepButton);
		stepButton.SetSize(XMFLOAT2(112, 28));
		stepButton.SetPos(XMFLOAT2(152, 18));
		stepButton.OnClick([this](const wi::gui::EventArgs&) {
			ApplyPendingRules();
			ApplyPendingRoadToggle();
			simulation.Step();
			simulationAccumulator = 0.0f;
			uiNotice = "Advanced one fixed simulation tick.";
			UpdateSimulationPresentation();
		});
		gui.AddWidget(&stepButton);

		roadButton.Create("MillRoadToggleButton");
		roadButton.SetText("Close mill road");
		StyleButton(roadButton);
		roadButton.SetSize(XMFLOAT2(132, 28));
		roadButton.SetPos(XMFLOAT2(268, 18));
		roadButton.OnClick([this](const wi::gui::EventArgs&) {
			const bool currentOrPending = pendingMillApproachRoadState.value_or(millApproachRoadEnabled);
			pendingMillApproachRoadState = !currentOrPending;
			uiNotice = std::string("Queued mill approach road ") + (*pendingMillApproachRoadState ? "open" : "closed") +
				" request for the next fixed simulation tick.";
			UpdateStatusLabel();
		});
		gui.AddWidget(&roadButton);

		buildRoadButton.Create("BuildRoadSegmentButton");
		buildRoadButton.SetText("Build road");
		StyleButton(buildRoadButton);
		buildRoadButton.SetSize(XMFLOAT2(100, 28));
		buildRoadButton.SetPos(XMFLOAT2(340, 54));
		buildRoadButton.OnClick([this](const wi::gui::EventArgs&) {
			if (activeRoadPlacement) CancelRoadPlacement();
			else BeginRoadPlacement();
		});
		gui.AddWidget(&buildRoadButton);

		placeStorageButton.Create("PlacePrototypeStorageButton");
		placeStorageButton.SetText("Place storage");
		StyleButton(placeStorageButton);
		placeStorageButton.SetSize(XMFLOAT2(122, 28));
		placeStorageButton.SetPos(XMFLOAT2(444, 54));
		placeStorageButton.OnClick([this](const wi::gui::EventArgs&) {
			if (activeStoragePlacement) CancelFacilityPlacement();
			else BeginStoragePlacement();
		});
		gui.AddWidget(&placeStorageButton);

		buildFarmButton.Create("BuildSyntheticFarmButton");
		buildFarmButton.SetText("Build farm");
		StyleButton(buildFarmButton);
		buildFarmButton.SetSize(XMFLOAT2(100, 28));
		buildFarmButton.SetPos(XMFLOAT2(18, 54));
		buildFarmButton.OnClick([this](const wi::gui::EventArgs&) {
			BeginFacilityPlacement(openrepublic::sim::FacilityType::Farm);
		});
		gui.AddWidget(&buildFarmButton);

		buildMillButton.Create("BuildSyntheticMillButton");
		buildMillButton.SetText("Build mill");
		StyleButton(buildMillButton);
		buildMillButton.SetSize(XMFLOAT2(100, 28));
		buildMillButton.SetPos(XMFLOAT2(122, 54));
		buildMillButton.OnClick([this](const wi::gui::EventArgs&) {
			BeginFacilityPlacement(openrepublic::sim::FacilityType::Mill);
		});
		gui.AddWidget(&buildMillButton);

		cancelPlacementButton.Create("CancelFacilityPlacementButton");
		cancelPlacementButton.SetText("Cancel build");
		StyleButton(cancelPlacementButton);
		cancelPlacementButton.SetSize(XMFLOAT2(110, 28));
		cancelPlacementButton.SetPos(XMFLOAT2(226, 54));
		cancelPlacementButton.OnClick([this](const wi::gui::EventArgs&) {
			if (activeRoadPlacement) CancelRoadPlacement();
			else CancelFacilityPlacement();
		});
		gui.AddWidget(&cancelPlacementButton);

		setTruckFarmButton.Create("TruckPlanFarmEndpointButton");
		setTruckFarmButton.SetText("Use selected farm");
		StyleButton(setTruckFarmButton);
		setTruckFarmButton.SetSize(XMFLOAT2(148, 28));
		setTruckFarmButton.SetPos(XMFLOAT2(18, 90));
		setTruckFarmButton.OnClick([this](const wi::gui::EventArgs&) { AssignTruckFarmEndpoint(); });
		gui.AddWidget(&setTruckFarmButton);

		setTruckMillButton.Create("TruckPlanMillEndpointButton");
		setTruckMillButton.SetText("Use selected mill");
		StyleButton(setTruckMillButton);
		setTruckMillButton.SetSize(XMFLOAT2(148, 28));
		setTruckMillButton.SetPos(XMFLOAT2(172, 90));
		setTruckMillButton.OnClick([this](const wi::gui::EventArgs&) { AssignTruckMillEndpoint(); });
		gui.AddWidget(&setTruckMillButton);

		setTruckStorageButton.Create("TruckPlanStorageEndpointButton");
		setTruckStorageButton.SetText("Use selected storage");
		StyleButton(setTruckStorageButton);
		setTruckStorageButton.SetSize(XMFLOAT2(166, 28));
		setTruckStorageButton.SetPos(XMFLOAT2(326, 90));
		setTruckStorageButton.OnClick([this](const wi::gui::EventArgs&) { AssignTruckStorageEndpoint(); });
		gui.AddWidget(&setTruckStorageButton);

		queueTruckPlanButton.Create("QueueTruckPlanButton");
		queueTruckPlanButton.SetText("Queue truck plan");
		StyleButton(queueTruckPlanButton);
		queueTruckPlanButton.SetSize(XMFLOAT2(148, 28));
		queueTruckPlanButton.SetPos(XMFLOAT2(498, 90));
		queueTruckPlanButton.OnClick([this](const wi::gui::EventArgs&) { QueueSelectedTruckPlan(); });
		gui.AddWidget(&queueTruckPlanButton);
		setTruckFarmButton.SetText("Farm: OR-" + std::to_string(truckPlan.farmId));
		setTruckMillButton.SetText("Mill: OR-" + std::to_string(truckPlan.millId));
		setTruckStorageButton.SetText("Storage: OR-" + std::to_string(truckPlan.storageId));

		saveButton.Create("SimulationSaveButton");
		saveButton.SetText("Save demo");
		StyleButton(saveButton);
		saveButton.SetSize(XMFLOAT2(90, 28));
		saveButton.SetPos(XMFLOAT2(530, 18));
		saveButton.OnClick([this](const wi::gui::EventArgs&) { SaveSimulation(); });
		gui.AddWidget(&saveButton);

		loadButton.Create("SimulationLoadButton");
		loadButton.SetText("Load demo");
		StyleButton(loadButton);
		loadButton.SetSize(XMFLOAT2(90, 28));
		loadButton.SetPos(XMFLOAT2(624, 18));
		loadButton.OnClick([this](const wi::gui::EventArgs&) { LoadSimulation(); });
		gui.AddWidget(&loadButton);

		#ifdef OPENREPUBLIC_WITH_LUA_RULES
		reloadRulesButton.Create("ReloadLuaRulesButton");
		reloadRulesButton.SetText("Reload Lua rules");
		StyleButton(reloadRulesButton);
		reloadRulesButton.SetSize(XMFLOAT2(120, 28));
		reloadRulesButton.SetPos(XMFLOAT2(718, 18));
		reloadRulesButton.OnClick([this](const wi::gui::EventArgs&) { ReloadLuaRules(); });
		gui.AddWidget(&reloadRulesButton);
		#endif

		simulationStatus.Create("SimulationStatus");
		simulationStatus.SetColor(wi::Color(24, 31, 39, 255));
		simulationStatus.font.params.color = kUiTextColor;
		simulationStatus.margin_left = 10;
		simulationStatus.margin_top = 7;
		simulationStatus.SetSize(XMFLOAT2(980, 130));
		simulationStatus.SetPos(XMFLOAT2(18, 126));
		simulationStatus.SetWrapEnabled(true);
		gui.AddWidget(&simulationStatus);

		wi::scene::TransformComponent cameraTransform;
		cameraTransform.Translate(terrainPreviewLoaded ? XMFLOAT3(0, 18.0f, -27.0f) : XMFLOAT3(0, 6.5f, -11.5f));
		cameraTransform.RotateRollPitchYaw(XMFLOAT3(terrainPreviewLoaded ? 0.52f : 0.43f, 0, 0));
		cameraTransform.UpdateTransform();
		renderPath->camera->TransformCamera(cameraTransform);
		renderPath->camera->UpdateCamera();

		ActivatePath(renderPath.get());
		UpdateSimulationPresentation();
	}

	void OpenRepublicApplication::Update(float dt)
	{
		wi::Application::Update(dt);

		if (simulationPaused)
		{
			simulationAccumulator = 0.0f;
		}
		else
		{
			simulationAccumulator += dt;
			while (simulationAccumulator >= kSimulationTickSeconds)
			{
				ApplyPendingRules();
				ApplyPendingRoadToggle();
				simulation.Step();
				simulationAccumulator -= kSimulationTickSeconds;
				UpdateSimulationPresentation();
			}
		}

		if (!is_window_active || renderPath == nullptr || renderPath->camera == nullptr)
		{
			return;
		}

		const bool guiHasFocus = renderPath->GetGUI().HasFocus();
		if (!guiHasFocus && wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
		{
			if (activeRoadPlacement) CancelRoadPlacement();
			else if (activeFacilityPlacement.has_value() || activeStoragePlacement) CancelFacilityPlacement();
			else SelectBuilding(wi::ecs::INVALID_ENTITY);
		}
		if (!guiHasFocus && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
		{
			PickAtPointer();
		}
		if (guiHasFocus)
		{
			return;
		}

		wi::scene::CameraComponent& camera = *renderPath->camera;
		const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&camera.At));
		const XMVECTOR up = XMLoadFloat3(&camera.Up);
		const XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
		XMVECTOR movement = XMVectorZero();
		const auto isDownOrPressed = [](wi::input::BUTTON button)
		{
			const bool down = wi::input::Down(button);
			const bool pressed = wi::input::Press(button);
			return down || pressed;
		};

		if (isDownOrPressed(static_cast<wi::input::BUTTON>('W')) || isDownOrPressed(wi::input::KEYBOARD_BUTTON_UP)) movement = XMVectorAdd(movement, forward);
		if (isDownOrPressed(static_cast<wi::input::BUTTON>('S')) || isDownOrPressed(wi::input::KEYBOARD_BUTTON_DOWN)) movement = XMVectorSubtract(movement, forward);
		if (isDownOrPressed(static_cast<wi::input::BUTTON>('D')) || isDownOrPressed(wi::input::KEYBOARD_BUTTON_RIGHT)) movement = XMVectorAdd(movement, right);
		if (isDownOrPressed(static_cast<wi::input::BUTTON>('A')) || isDownOrPressed(wi::input::KEYBOARD_BUTTON_LEFT)) movement = XMVectorSubtract(movement, right);
		if (isDownOrPressed(wi::input::KEYBOARD_BUTTON_SPACE)) movement = XMVectorAdd(movement, XMVectorSet(0, 1, 0, 0));
		if (isDownOrPressed(wi::input::KEYBOARD_BUTTON_LCONTROL)) movement = XMVectorSubtract(movement, XMVectorSet(0, 1, 0, 0));

		const XMFLOAT4 pointer = wi::input::GetPointer();
		bool cameraMoved = false;
		if (XMVectorGetX(XMVector3LengthSq(movement)) > 0)
		{
			movement = XMVector3Normalize(movement);
			const XMVECTOR eye = XMLoadFloat3(&camera.Eye);
			// Keep movement in world-units per second and bound catch-up after a stalled frame.
			const float movementDelta = std::clamp(dt, 0.0f, kMaxCameraMoveDeltaSeconds);
			XMStoreFloat3(&camera.Eye, XMVectorAdd(eye, XMVectorScale(movement, kCameraMoveSpeed * movementDelta)));
			cameraMoved = true;
		}
		if (pointer.z != 0)
		{
			const XMVECTOR eye = XMLoadFloat3(&camera.Eye);
			XMStoreFloat3(&camera.Eye, XMVectorAdd(eye, XMVectorScale(forward, pointer.z * kWheelZoomSpeed)));
			cameraMoved = true;
		}
		if (cameraMoved)
		{
			camera.UpdateCamera();
		}
	}

	std::string OpenRepublicApplication::GetOpenRepublicId(wi::ecs::Entity entity) const
	{
		if (entity == farmBuilding) return "OR-1001";
		if (entity == millBuilding) return "OR-1002";
		if (entity == warehouseBuilding) return "OR-1003";
		for (std::size_t i = 0; i < prototypeStorageEntities.size() && i < prototypeStorageIds.size(); ++i)
		{
			if (entity == prototypeStorageEntities[i]) return prototypeStorageIds[i];
		}
		for (std::size_t i = 0; i < facilityEntities.size() && i < facilityIds.size(); ++i)
		{
			if (entity == facilityEntities[i]) return facilityIds[i];
		}
		return {};
	}

	std::optional<openrepublic::sim::EntityId> OpenRepublicApplication::GetSelectedSimulationId() const
	{
		const std::string openRepublicId = GetOpenRepublicId(selectedBuilding);
		constexpr char prefix[] = "OR-";
		if (openRepublicId.rfind(prefix, 0) != 0 || openRepublicId.size() <= sizeof(prefix) - 1) return std::nullopt;
		try
		{
			const unsigned long long parsed = std::stoull(openRepublicId.substr(sizeof(prefix) - 1));
			if (parsed > std::numeric_limits<openrepublic::sim::EntityId>::max()) return std::nullopt;
			return static_cast<openrepublic::sim::EntityId>(parsed);
		}
		catch (const std::exception&)
		{
			return std::nullopt;
		}
	}

	void OpenRepublicApplication::AssignTruckFarmEndpoint()
	{
		const auto id = GetSelectedSimulationId();
		const auto snapshot = simulation.GetSnapshot();
		const auto facility = id ? std::find_if(snapshot.facilities.begin(), snapshot.facilities.end(), [&](const auto& item) {
			return item.id == *id && item.type == openrepublic::sim::FacilityType::Farm;
		}) : snapshot.facilities.end();
		if (!id || facility == snapshot.facilities.end())
		{
			uiNotice = "Select a built-in or player-built farm first.";
		}
		else if (std::none_of(snapshot.roadNodes.begin(), snapshot.roadNodes.end(), [&](const auto& node) {
			return node.gridX == facility->gridX && node.gridZ == facility->gridZ;
		}))
		{
			uiNotice = "Farm OR-" + std::to_string(*id) + " has no road node at grid (" +
				std::to_string(facility->gridX) + ", " + std::to_string(facility->gridZ) +
				"); connect a road endpoint to this cell before assigning it.";
		}
		else
		{
			truckPlan.farmId = *id;
			setTruckFarmButton.SetText("Farm: OR-" + std::to_string(*id));
			uiNotice = "Truck farm endpoint set to OR-" + std::to_string(*id) + ". Queue the plan when all endpoints are ready.";
		}
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::AssignTruckMillEndpoint()
	{
		const auto id = GetSelectedSimulationId();
		const auto snapshot = simulation.GetSnapshot();
		const auto facility = id ? std::find_if(snapshot.facilities.begin(), snapshot.facilities.end(), [&](const auto& item) {
			return item.id == *id && item.type == openrepublic::sim::FacilityType::Mill;
		}) : snapshot.facilities.end();
		if (!id || facility == snapshot.facilities.end())
		{
			uiNotice = "Select a built-in or player-built mill first.";
		}
		else if (std::none_of(snapshot.roadNodes.begin(), snapshot.roadNodes.end(), [&](const auto& node) {
			return node.gridX == facility->gridX && node.gridZ == facility->gridZ;
		}))
		{
			uiNotice = "Mill OR-" + std::to_string(*id) + " has no road node at grid (" +
				std::to_string(facility->gridX) + ", " + std::to_string(facility->gridZ) +
				"); connect a road endpoint to this cell before assigning it.";
		}
		else
		{
			truckPlan.millId = *id;
			setTruckMillButton.SetText("Mill: OR-" + std::to_string(*id));
			uiNotice = "Truck mill endpoint set to OR-" + std::to_string(*id) + ". Queue the plan when all endpoints are ready.";
		}
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::AssignTruckStorageEndpoint()
	{
		const auto id = GetSelectedSimulationId();
		const auto snapshot = simulation.GetSnapshot();
		const auto storage = id ? std::find_if(snapshot.storages.begin(), snapshot.storages.end(), [&](const auto& item) {
			return item.id == *id;
		}) : snapshot.storages.end();
		if (!id || storage == snapshot.storages.end())
		{
			uiNotice = "Select the built-in warehouse or a placed storage first.";
		}
		else if (std::none_of(snapshot.roadNodes.begin(), snapshot.roadNodes.end(), [&](const auto& node) {
			return node.gridX == storage->gridX && node.gridZ == storage->gridZ;
		}))
		{
			uiNotice = "Storage OR-" + std::to_string(*id) + " has no road node at grid (" +
				std::to_string(storage->gridX) + ", " + std::to_string(storage->gridZ) +
				"); connect a road endpoint to this cell before assigning it.";
		}
		else
		{
			truckPlan.storageId = *id;
			setTruckStorageButton.SetText("Storage: OR-" + std::to_string(*id));
			uiNotice = "Truck storage endpoint set to OR-" + std::to_string(*id) + ". Queue the plan when all endpoints are ready.";
		}
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::QueueSelectedTruckPlan()
	{
		const auto snapshot = simulation.GetSnapshot();
		if (!snapshot.pendingTruckPlans.empty())
		{
			uiNotice = "Truck plan command " + std::to_string(snapshot.pendingTruckPlans.front().commandId) +
				" is still pending; wait for its fixed-tick result before queueing another plan.";
			UpdateStatusLabel();
			return;
		}
		const auto activeFarm = std::find_if(snapshot.facilities.begin(), snapshot.facilities.end(), [&](const auto& item) {
			return item.id == snapshot.truckPlan.farmId && item.type == openrepublic::sim::FacilityType::Farm;
		});
		const auto activeFarmNode = activeFarm == snapshot.facilities.end()
			? snapshot.roadNodes.end()
			: std::find_if(snapshot.roadNodes.begin(), snapshot.roadNodes.end(), [&](const auto& node) {
				return node.gridX == activeFarm->gridX && node.gridZ == activeFarm->gridZ;
			});
		const bool idleAtAssignedFarm = snapshot.truckCargo == openrepublic::sim::Resource::None &&
			snapshot.truckCargoAmount == 0 && snapshot.activeRoute.empty() &&
			snapshot.truckStage == openrepublic::sim::TruckStage::LoadGrainAtFarm &&
			activeFarmNode != snapshot.roadNodes.end() && snapshot.truckNode == activeFarmNode->id;
		if (!idleAtAssignedFarm)
		{
			const auto activeStorage = std::find_if(snapshot.storages.begin(), snapshot.storages.end(), [&](const auto& item) {
				return item.id == snapshot.truckPlan.storageId;
			});
			const bool storageOnlyRetarget = truckPlan.farmId == snapshot.truckPlan.farmId &&
				truckPlan.millId == snapshot.truckPlan.millId && truckPlan.storageId != snapshot.truckPlan.storageId &&
				snapshot.truckStage == openrepublic::sim::TruckStage::DeliverFlourToWarehouse &&
				snapshot.truckCargo == openrepublic::sim::Resource::Flour && snapshot.truckCargoAmount > 0 &&
				activeStorage != snapshot.storages.end() && activeStorage->inventory.flour >= kStorageFlourCapacity;
			if (!storageOnlyRetarget)
			{
				uiNotice = "Truck plan changes wait until the truck is empty, its route is clear, and it is idle at its assigned farm. During delivery recovery, only change Storage while keeping Farm and Mill unchanged and the active storage is full of flour.";
				UpdateStatusLabel();
				return;
			}
		}
		const std::uint64_t commandId = simulation.QueueTruckPlan(truckPlan);
		if (commandId == 0)
		{
			uiNotice = "Truck plan could not be queued; the bounded command queue may be full or the simulation may be at its terminal tick.";
		}
		else
		{
			uiNotice = "Queued truck plan command " + std::to_string(commandId) + " (Farm OR-" +
				std::to_string(truckPlan.farmId) + " -> Mill OR-" + std::to_string(truckPlan.millId) +
				" -> Storage OR-" + std::to_string(truckPlan.storageId) + "); graph connectivity and truck state will be validated at the next fixed tick.";
		}
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::ApplyPendingRoadToggle()
	{
		if (!pendingMillApproachRoadState.has_value()) return;
		const bool requestedState = *pendingMillApproachRoadState;
		pendingMillApproachRoadState.reset();
		if (simulation.SetRoadEnabled(202, requestedState))
		{
			millApproachRoadEnabled = requestedState;
			uiNotice = std::string("Mill approach road ") + (millApproachRoadEnabled ? "opened" : "closed") +
				" at a fixed simulation tick boundary.";
		}
		else
		{
			uiNotice = "Road remains open while the truck's active route requires it.";
		}
	}

	void OpenRepublicApplication::BeginFacilityPlacement(openrepublic::sim::FacilityType type)
	{
		CancelRoadPlacement();
		activeStoragePlacement = false;
		placeStorageButton.SetText("Place storage");
		activeFacilityPlacement = type;
		SelectBuilding(wi::ecs::INVALID_ENTITY);
		uiNotice = std::string("Choose a ground cell for the synthetic ") +
			(type == openrepublic::sim::FacilityType::Farm ? "farm" : "mill") +
			"; press Esc or Cancel build to leave placement mode.";
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::CancelFacilityPlacement()
	{
		activeFacilityPlacement.reset();
		activeStoragePlacement = false;
		placeStorageButton.SetText("Place storage");
		uiNotice = "Placement mode cancelled.";
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::BeginStoragePlacement()
	{
		CancelRoadPlacement();
		activeFacilityPlacement.reset();
		activeStoragePlacement = true;
		SelectBuilding(wi::ecs::INVALID_ENTITY);
		placeStorageButton.SetText("Cancel storage");
		uiNotice = "Place synthetic storage: click a valid construction grid cell. The storage is queued for validation at the next fixed simulation tick; Esc or Cancel build exits placement.";
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::BeginRoadPlacement()
	{
		activeFacilityPlacement.reset();
		activeStoragePlacement = false;
		placeStorageButton.SetText("Place storage");
		activeRoadPlacement = true;
		pendingRoadStart.reset();
		SelectBuilding(wi::ecs::INVALID_ENTITY);
		uiNotice = "Build synthetic road: click a lattice cell for the first endpoint, then an orthogonally adjacent cell. Esc cancels.";
		buildRoadButton.SetText("Cancel road");
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::CancelRoadPlacement()
	{
		activeRoadPlacement = false;
		pendingRoadStart.reset();
		buildRoadButton.SetText("Build road");
		uiNotice = "Road construction mode cancelled.";
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::SelectBuilding(wi::ecs::Entity selected)
	{
		selectedBuilding = !GetOpenRepublicId(selected).empty() ? selected : wi::ecs::INVALID_ENTITY;
		SetEntityColor(farmBuilding, selectedBuilding == farmBuilding ? kSelectedBuildingColor : kFarmColor);
		SetEntityColor(millBuilding, selectedBuilding == millBuilding ? kSelectedBuildingColor : kMillColor);
		SetEntityColor(warehouseBuilding, selectedBuilding == warehouseBuilding ? kSelectedBuildingColor : kWarehouseColor);
		for (const wi::ecs::Entity entity : prototypeStorageEntities)
		{
			SetEntityColor(entity, selectedBuilding == entity ? kSelectedBuildingColor : kPrototypeStorageColor);
		}
		for (std::size_t i = 0; i < facilityEntities.size() && i < facilityTypes.size(); ++i)
		{
			const XMFLOAT4 baseColor = facilityTypes[i] == openrepublic::sim::FacilityType::Farm
				? kPlacedFarmColor : kPlacedMillColor;
			SetEntityColor(facilityEntities[i], selectedBuilding == facilityEntities[i] ? kSelectedBuildingColor : baseColor);
		}
		UpdateStatusLabel();

		if (window != nullptr)
		{
			const std::string id = GetOpenRepublicId(selectedBuilding);
			SetWindowTextA(window, !id.empty()
				? (std::string("OpenRepublic | Selected ") + id + " | WASD/Arrows move, Space/Ctrl rise/lower, wheel zoom, click inspect, Esc clear").c_str()
				: "OpenRepublic | Synthetic economy | WASD/Arrows move, Space/Ctrl rise/lower, wheel zoom, click select, Esc clear");
		}
	}

	void OpenRepublicApplication::PickAtPointer()
	{
		if (renderPath == nullptr || renderPath->camera == nullptr)
		{
			return;
		}

		const XMFLOAT4 pointer = wi::input::GetPointer();
		if (pointer.x < 0 || pointer.y < 0 || pointer.x >= renderPath->GetLogicalWidth() || pointer.y >= renderPath->GetLogicalHeight())
		{
			return;
		}

		const wi::primitive::Ray ray = wi::renderer::GetPickRay(
			static_cast<long>(pointer.x), static_cast<long>(pointer.y), *renderPath, *renderPath->camera);
		if (activeRoadPlacement)
		{
			if (!std::isfinite(ray.origin.y) || !std::isfinite(ray.direction.y) || std::abs(ray.direction.y) < 1e-6f)
			{
				uiNotice = "Road endpoint ray is parallel to the ground; adjust the camera.";
				UpdateStatusLabel();
				return;
			}
			const float distance = -ray.origin.y / ray.direction.y;
			if (!std::isfinite(distance) || distance < ray.TMin || distance > ray.TMax)
			{
				uiNotice = "No road lattice cell at this pointer location.";
				UpdateStatusLabel();
				return;
			}
			const float worldX = ray.origin.x + ray.direction.x * distance;
			const float worldZ = ray.origin.z + ray.direction.z * distance;
			constexpr float roadCellSize = openrepublic::sim::Simulation::RoadCellWorldSize;
			const float gridXFloat = std::round(worldX / roadCellSize);
			const float gridZFloat = std::round(worldZ / roadCellSize);
			if (!std::isfinite(gridXFloat) || !std::isfinite(gridZFloat) ||
				gridXFloat < openrepublic::sim::Simulation::PrototypeGridMin || gridXFloat > openrepublic::sim::Simulation::PrototypeGridMax ||
				gridZFloat < openrepublic::sim::Simulation::PrototypeGridMin || gridZFloat > openrepublic::sim::Simulation::PrototypeGridMax)
			{
				uiNotice = "Road endpoint is outside the bounded synthetic road lattice.";
				UpdateStatusLabel();
				return;
			}
			const auto gridX = static_cast<std::int32_t>(gridXFloat);
			const auto gridZ = static_cast<std::int32_t>(gridZFloat);
			if (!pendingRoadStart.has_value())
			{
				pendingRoadStart = std::make_pair(gridX, gridZ);
				uiNotice = "Road start set at (" + std::to_string(gridX) + ", " + std::to_string(gridZ) + "); choose an orthogonally adjacent endpoint.";
				UpdateStatusLabel();
				return;
			}
			const auto [startX, startZ] = *pendingRoadStart;
			if (std::abs(gridX - startX) + std::abs(gridZ - startZ) != 1)
			{
				uiNotice = "Road endpoint rejected: choose one orthogonally adjacent lattice cell.";
				UpdateStatusLabel();
				return;
			}
			const std::uint64_t commandId = simulation.QueueRoadSegment(startX, startZ, gridX, gridZ);
			if (commandId == 0)
			{
				uiNotice = "Road segment could not be queued; the bounded construction queue may be full.";
			}
			else
			{
				uiNotice = "Queued synthetic road segment command " + std::to_string(commandId) + " from (" +
					std::to_string(startX) + ", " + std::to_string(startZ) + ") to (" + std::to_string(gridX) + ", " +
					std::to_string(gridZ) + "); validation occurs at the next simulation tick.";
			}
			pendingRoadStart.reset();
			UpdateStatusLabel();
			return;
		}
		if (activeFacilityPlacement.has_value())
		{
			if (!std::isfinite(ray.origin.y) || !std::isfinite(ray.direction.y) || std::abs(ray.direction.y) < 1e-6f)
			{
				uiNotice = "Placement ray is parallel to the ground; adjust the camera.";
				UpdateStatusLabel();
				return;
			}
			const float distance = -ray.origin.y / ray.direction.y;
			if (!std::isfinite(distance) || distance < ray.TMin || distance > ray.TMax)
			{
				uiNotice = "No ground cell at this pointer location.";
				UpdateStatusLabel();
				return;
			}
			const float worldX = ray.origin.x + ray.direction.x * distance;
			const float worldZ = ray.origin.z + ray.direction.z * distance;
			const float gridXFloat = std::round(worldX / kPlacementGridSpacing);
			const float gridZFloat = std::round(worldZ / kPlacementGridSpacing);
			if (!std::isfinite(gridXFloat) || !std::isfinite(gridZFloat) ||
				gridXFloat < openrepublic::sim::Simulation::PrototypeGridMin || gridXFloat > openrepublic::sim::Simulation::PrototypeGridMax ||
				gridZFloat < openrepublic::sim::Simulation::PrototypeGridMin || gridZFloat > openrepublic::sim::Simulation::PrototypeGridMax)
			{
				uiNotice = "Placement location is outside the prototype construction grid.";
				UpdateStatusLabel();
				return;
			}
			const auto gridX = static_cast<std::int32_t>(gridXFloat);
			const auto gridZ = static_cast<std::int32_t>(gridZFloat);
			const std::uint64_t commandId = simulation.QueueFacilityPlacement(*activeFacilityPlacement, gridX, gridZ);
			if (commandId == 0)
			{
				uiNotice = "Could not queue construction; the simulation queue may be full or at its terminal tick.";
			}
			else
			{
				uiNotice = "Queued synthetic facility placement command " + std::to_string(commandId) +
					" at grid (" + std::to_string(gridX) + ", " + std::to_string(gridZ) + "); the simulation will validate it at the next tick.";
			}
			UpdateStatusLabel();
			return;
		}
		if (activeStoragePlacement)
		{
			if (!std::isfinite(ray.origin.y) || !std::isfinite(ray.direction.y) || std::abs(ray.direction.y) < 1e-6f)
			{
				uiNotice = "Storage placement ray is parallel to the ground; adjust the camera.";
				UpdateStatusLabel();
				return;
			}
			const float distance = -ray.origin.y / ray.direction.y;
			if (!std::isfinite(distance) || distance < ray.TMin || distance > ray.TMax)
			{
				uiNotice = "No ground cell at this pointer location.";
				UpdateStatusLabel();
				return;
			}
			const float worldX = ray.origin.x + ray.direction.x * distance;
			const float worldZ = ray.origin.z + ray.direction.z * distance;
			const float gridXFloat = std::round(worldX / kPlacementGridSpacing);
			const float gridZFloat = std::round(worldZ / kPlacementGridSpacing);
			if (!std::isfinite(gridXFloat) || !std::isfinite(gridZFloat) ||
				gridXFloat < openrepublic::sim::Simulation::PrototypeGridMin || gridXFloat > openrepublic::sim::Simulation::PrototypeGridMax ||
				gridZFloat < openrepublic::sim::Simulation::PrototypeGridMin || gridZFloat > openrepublic::sim::Simulation::PrototypeGridMax)
			{
				uiNotice = "Storage location is outside the prototype construction grid.";
				UpdateStatusLabel();
				return;
			}
			const auto gridX = static_cast<std::int32_t>(gridXFloat);
			const auto gridZ = static_cast<std::int32_t>(gridZFloat);
			const std::uint64_t commandId = simulation.QueueStoragePlacement(gridX, gridZ);
			if (commandId == 0)
			{
				uiNotice = "Could not queue storage placement; the bounded construction queue may be full or at its terminal tick.";
			}
			else
			{
				uiNotice = "Queued storage placement command " + std::to_string(commandId) + " at grid (" +
					std::to_string(gridX) + ", " + std::to_string(gridZ) + "); occupancy and capacity are validated at the next fixed tick.";
			}
			UpdateStatusLabel();
			return;
		}
		const wi::scene::Scene::RayIntersectionResult hit = previewWorld.Intersects(ray);
		SelectBuilding(hit.entity);
	}

	void OpenRepublicApplication::UpdateSimulationPresentation()
	{
		const openrepublic::sim::Snapshot snapshot = simulation.GetSnapshot();
		SyncPrototypeStorages(snapshot);
		SyncFacilities(snapshot);
		SyncRoads(snapshot);
		for (const auto& result : snapshot.constructionResults)
		{
			if (result.commandId <= lastConstructionResultCommandId) continue;
			lastConstructionResultCommandId = result.commandId;
			if (result.type == openrepublic::sim::ConstructionCommandType::RoadSegment)
			{
				uiNotice = result.accepted
					? "Road construction command " + std::to_string(result.commandId) + " accepted as edge " + std::to_string(result.entityId) + "."
					: "Road construction command " + std::to_string(result.commandId) + " rejected by road graph validation.";
			}
			else if (result.type == openrepublic::sim::ConstructionCommandType::StoragePlacement)
			{
				uiNotice = result.accepted
					? "Storage placement command " + std::to_string(result.commandId) + " accepted as OR-" + std::to_string(result.entityId) + "."
					: "Storage placement command " + std::to_string(result.commandId) + " rejected: occupied cell or storage limit.";
			}
			else if (result.type == openrepublic::sim::ConstructionCommandType::TruckPlan)
			{
				uiNotice = result.accepted
					? "Truck plan command " + std::to_string(result.commandId) + " accepted; the selected route is now active."
					: "Truck plan command " + std::to_string(result.commandId) + " rejected: invalid endpoint, disconnected graph, or truck still active on its previous plan.";
			}
			else
			{
				uiNotice = result.accepted
					? "Construction command " + std::to_string(result.commandId) + " accepted as OR-" + std::to_string(result.entityId) + "."
					: "Construction command " + std::to_string(result.commandId) + " rejected: occupied cell or facility limit.";
			}
			if (!simulation.AcknowledgeConstructionResult(result.commandId))
			{
				uiNotice += " (result acknowledgment was already absent.)";
			}
		}
		for (const auto& road : snapshot.roads)
		{
			if (road.id == 202)
			{
				millApproachRoadEnabled = road.enabled;
				break;
			}
		}
		if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(truckEntity))
		{
			const auto node = std::find_if(snapshot.roadNodes.begin(), snapshot.roadNodes.end(), [&](const auto& item) {
				return item.id == snapshot.truckNode;
			});
			if (node != snapshot.roadNodes.end())
			{
				transform->translation_local = XMFLOAT3(
					static_cast<float>(node->gridX) * openrepublic::sim::Simulation::RoadCellWorldSize,
					0.28f,
					static_cast<float>(node->gridZ) * openrepublic::sim::Simulation::RoadCellWorldSize);
			}
			transform->UpdateTransform();
		}
		roadButton.SetText(millApproachRoadEnabled ? "Close mill road" : "Open mill road");
		buildRoadButton.SetText(activeRoadPlacement ? "Cancel road" : "Build road");
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::SyncPrototypeStorages(const openrepublic::sim::Snapshot& snapshot)
	{
		while (prototypeStorageEntities.size() > snapshot.prototypeStorages.size())
		{
			previewWorld.Entity_Remove(prototypeStorageEntities.back());
			prototypeStorageEntities.pop_back();
			prototypeStorageIds.pop_back();
		}

		while (prototypeStorageEntities.size() < snapshot.prototypeStorages.size())
		{
			const auto& storage = snapshot.prototypeStorages[prototypeStorageEntities.size()];
			const std::string openRepublicId = "OR-" + std::to_string(storage.id);
			const std::string name = "OpenRepublic synthetic storage " + openRepublicId;
			const wi::ecs::Entity entity = previewWorld.Entity_CreateCube(name);
			if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(entity))
			{
				transform->Scale(XMFLOAT3(0.58f, 0.58f, 0.58f));
				transform->UpdateTransform();
			}
			SetEntityColor(entity, kPrototypeStorageColor);
			prototypeStorageEntities.push_back(entity);
			prototypeStorageIds.push_back(openRepublicId);
		}

		for (std::size_t i = 0; i < snapshot.prototypeStorages.size(); ++i)
		{
			const auto& storage = snapshot.prototypeStorages[i];
			if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(prototypeStorageEntities[i]))
			{
				transform->translation_local = XMFLOAT3(
					static_cast<float>(storage.gridX) * kPlacementGridSpacing,
					0.58f,
					GetSyntheticEntityDisplayZ(storage.gridX, storage.gridZ));
				transform->UpdateTransform();
			}
		}
	}

	void OpenRepublicApplication::SyncFacilities(const openrepublic::sim::Snapshot& snapshot)
	{
		const std::size_t firstDynamicFacility = std::min<std::size_t>(2, snapshot.facilities.size());
		const std::size_t dynamicFacilityCount = snapshot.facilities.size() - firstDynamicFacility;
		bool identityChanged = facilityEntities.size() != dynamicFacilityCount;
		for (std::size_t i = 0; !identityChanged && i < snapshot.facilities.size(); ++i)
		{
			if (i < firstDynamicFacility) continue;
			identityChanged = facilityIds[i - firstDynamicFacility] != "OR-" + std::to_string(snapshot.facilities[i].id);
		}
		if (identityChanged)
		{
			for (const wi::ecs::Entity entity : facilityEntities) previewWorld.Entity_Remove(entity);
			facilityEntities.clear();
			facilityIds.clear();
			facilityTypes.clear();
		}
		for (std::size_t i = firstDynamicFacility; i < snapshot.facilities.size(); ++i)
		{
			const auto& facility = snapshot.facilities[i];
			const std::size_t dynamicIndex = i - firstDynamicFacility;
			const std::string openRepublicId = "OR-" + std::to_string(facility.id);
			if (dynamicIndex >= facilityEntities.size())
			{
				const std::string kind = facility.type == openrepublic::sim::FacilityType::Farm ? "farm" : "mill";
				const wi::ecs::Entity entity = previewWorld.Entity_CreateCube("OpenRepublic synthetic " + kind + " " + openRepublicId);
				if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(entity))
				{
					transform->Scale(XMFLOAT3(0.56f, 0.7f, 0.56f));
					transform->UpdateTransform();
				}
				facilityEntities.push_back(entity);
				facilityIds.push_back(openRepublicId);
				facilityTypes.push_back(facility.type);
			}
			facilityTypes[dynamicIndex] = facility.type;
			if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(facilityEntities[dynamicIndex]))
			{
				transform->translation_local = XMFLOAT3(
					static_cast<float>(facility.gridX) * kPlacementGridSpacing, 0.7f,
					GetSyntheticEntityDisplayZ(facility.gridX, facility.gridZ));
				transform->UpdateTransform();
			}
			const XMFLOAT4 baseColor = facility.type == openrepublic::sim::FacilityType::Farm ? kPlacedFarmColor : kPlacedMillColor;
			SetEntityColor(facilityEntities[dynamicIndex], selectedBuilding == facilityEntities[dynamicIndex] ? kSelectedBuildingColor : baseColor);
		}
		if (selectedBuilding != wi::ecs::INVALID_ENTITY && GetOpenRepublicId(selectedBuilding).empty())
		{
			SelectBuilding(wi::ecs::INVALID_ENTITY);
		}
	}

	void OpenRepublicApplication::SyncRoads(const openrepublic::sim::Snapshot& snapshot)
	{
		auto findVisual = [this](openrepublic::sim::EntityId id) {
			return std::find_if(roadVisuals.begin(), roadVisuals.end(), [id](const RoadVisual& visual) {
				return visual.id == id;
			});
		};
		for (const auto& road : snapshot.roads)
		{
			auto visual = findVisual(road.id);
			if (visual == roadVisuals.end())
			{
				const wi::ecs::Entity entity = previewWorld.Entity_CreateCube("OpenRepublic synthetic road edge " + std::to_string(road.id));
				if (road.aGridX == road.bGridX)
				{
					if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(entity))
					{
						transform->RotateRollPitchYaw(XMFLOAT3(0, XM_PIDIV2, 0));
						transform->UpdateTransform();
					}
				}
				roadVisuals.push_back(RoadVisual{ road.id, entity });
				visual = std::prev(roadVisuals.end());
			}
			if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(visual->entity))
			{
				const float ax = static_cast<float>(road.aGridX) * openrepublic::sim::Simulation::RoadCellWorldSize;
				const float az = static_cast<float>(road.aGridZ) * openrepublic::sim::Simulation::RoadCellWorldSize;
				const float bx = static_cast<float>(road.bGridX) * openrepublic::sim::Simulation::RoadCellWorldSize;
				const float bz = static_cast<float>(road.bGridZ) * openrepublic::sim::Simulation::RoadCellWorldSize;
				transform->Scale(XMFLOAT3(1.0f, 0.045f, 0.22f));
				transform->translation_local = XMFLOAT3((ax + bx) * 0.5f, 0.04f, (az + bz) * 0.5f);
				transform->UpdateTransform();
			}
			SetEntityColor(visual->entity, road.enabled ? kRoadColor : kClosedRoadColor);
		}

		for (auto visual = roadVisuals.begin(); visual != roadVisuals.end();)
		{
			const bool stillPresent = std::any_of(snapshot.roads.begin(), snapshot.roads.end(), [&](const auto& road) {
				return road.id == visual->id;
			});
			if (!stillPresent)
			{
				previewWorld.Entity_Remove(visual->entity);
				visual = roadVisuals.erase(visual);
			}
			else
			{
				++visual;
			}
		}
	}

	void OpenRepublicApplication::SaveSimulation()
	{
		const std::filesystem::path path = std::filesystem::current_path() / kSimulationSaveFile;
		std::filesystem::path temporaryPath = path;
		temporaryPath += L".tmp";
		std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			uiNotice = "Could not write " + path.filename().string() + ".";
			UpdateStatusLabel();
			return;
		}

		const std::string saveData = simulation.Save();
		output.write(saveData.data(), static_cast<std::streamsize>(saveData.size()));
		output.flush();
		bool writeSucceeded = static_cast<bool>(output);
		output.close();
		writeSucceeded = writeSucceeded && !output.fail();
		if (!writeSucceeded)
		{
			std::error_code removeError;
			std::filesystem::remove(temporaryPath, removeError);
			uiNotice = "Writing " + path.filename().string() + " failed; the previous save was kept.";
		}
		else if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::error_code removeError;
			std::filesystem::remove(temporaryPath, removeError);
			uiNotice = "Could not replace " + path.filename().string() + "; the previous save was kept.";
		}
		else
		{
			uiNotice = "Saved OpenRepublic simulation to " + path.filename().string() + ".";
		}
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::LoadSimulation()
	{
		const std::filesystem::path path = std::filesystem::current_path() / kSimulationSaveFile;
		std::ifstream input(path, std::ios::binary);
		if (!input)
		{
			uiNotice = "No " + path.filename().string() + " save exists yet.";
			UpdateStatusLabel();
			return;
		}

		std::ostringstream data;
		data << input.rdbuf();
		if (input.bad())
		{
			uiNotice = "Reading " + path.filename().string() + " failed.";
			UpdateStatusLabel();
			return;
		}

		std::string error;
		if (!simulation.Load(data.str(), &error))
		{
			uiNotice = "Save rejected: " + error;
			UpdateStatusLabel();
			return;
		}

		pendingRules.reset();
		lastConstructionResultCommandId = 0;
		pendingMillApproachRoadState.reset();
		truckPlan = simulation.GetSnapshot().truckPlan;
		setTruckFarmButton.SetText("Farm: OR-" + std::to_string(truckPlan.farmId));
		setTruckMillButton.SetText("Mill: OR-" + std::to_string(truckPlan.millId));
		setTruckStorageButton.SetText("Storage: OR-" + std::to_string(truckPlan.storageId));
		activeFacilityPlacement.reset();
		activeStoragePlacement = false;
		activeRoadPlacement = false;
		pendingRoadStart.reset();
		buildRoadButton.SetText("Build road");
		placeStorageButton.SetText("Place storage");
		simulationPaused = true;
		simulationAccumulator = 0.0f;
		pauseButton.SetText("Resume simulation");
		uiNotice = "Loaded OpenRepublic simulation from " + path.filename().string() + ".";
		UpdateSimulationPresentation();
		SelectBuilding(wi::ecs::INVALID_ENTITY);
	}

	void OpenRepublicApplication::ReloadLuaRules()
	{
		#ifdef OPENREPUBLIC_WITH_LUA_RULES
		// A reload request supersedes any candidate queued by an earlier request.
		// Clear it before parsing so an invalid replacement cannot later apply a
		// stale candidate after the user has been told the active rules were kept.
		pendingRules.reset();
		openrepublic::sim::SimulationRules candidate = simulation.GetRules();
		std::string error;
		const auto rulesPath = LocateDemoRulesFile();
		if (!openrepublic::scripting::LuaRulesLoader::LoadFile(rulesPath, candidate, &error))
		{
			uiNotice = "Lua rule reload failed; current rules remain active: " + error;
			UpdateStatusLabel();
			return;
		}

		if (simulationPaused)
		{
			if (simulation.SetRules(candidate, &error))
			{
				uiNotice = "Applied Lua rules at the paused tick boundary.";
			}
			else
			{
				uiNotice = "Lua rules rejected; current rules remain active: " + error;
			}
		}
		else
		{
			pendingRules = candidate;
			uiNotice = "Lua rules validated; changes apply at the next tick boundary.";
		}
		UpdateSimulationPresentation();
		#else
		uiNotice = "Lua support is not available in this build.";
		UpdateStatusLabel();
		#endif
	}

	void OpenRepublicApplication::ApplyPendingRules()
	{
		if (!pendingRules.has_value()) return;
		std::string error;
		if (simulation.SetRules(*pendingRules, &error))
		{
			uiNotice = "Applied Lua rules at a fixed tick boundary.";
		}
		else
		{
			uiNotice = "Lua rules rejected; previous rules remain active: " + error;
		}
		pendingRules.reset();
		UpdateStatusLabel();
	}

	void OpenRepublicApplication::UpdateStatusLabel()
	{
		const openrepublic::sim::Snapshot snapshot = simulation.GetSnapshot();
		std::ostringstream text;
		text << "Tick " << snapshot.tick
			<< " | Farm grain " << snapshot.farm.grain << "/12"
			<< " | Mill grain/flour " << snapshot.mill.grain << '/' << snapshot.mill.flour
			<< " | Warehouse " << snapshot.warehouse.flour << "/5"
			<< " | Storage " << snapshot.prototypeStorages.size()
			<< " | Facilities " << snapshot.facilities.size() << " (synthetic)"
			<< " | Roads " << snapshot.roads.size() << " (synthetic)"
			<< "\nTruck node " << snapshot.truckNode << ", " << snapshot.truckCargoAmount << ' ' << ResourceName(snapshot.truckCargo)
			<< " | Edge 202 " << (millApproachRoadEnabled ? "open" : "closed")
			<< (pendingMillApproachRoadState.has_value() ? " (change pending)" : "")
			<< " | Rules farm " << snapshot.rules.farmGrainPerHarvest << " every " << snapshot.rules.farmHarvestIntervalTicks
			<< " ticks, mill " << snapshot.rules.millGrainPerTick << "/tick"
			<< (pendingRules.has_value() ? " (pending)" : "")
			<< " | " << (simulationPaused ? "paused" : "running")
			<< (activeRoadPlacement ? (pendingRoadStart.has_value() ? " | road start selected" : " | road placement") : "")
			<< (activeStoragePlacement ? " | storage placement" : "")
			<< (activeFacilityPlacement.has_value() ? " | facility placement" : "")
			<< "\nRoad lattice: " << openrepublic::sim::Simulation::RoadCellWorldSize
			<< " world units/cell; service anchors Farm(-2,0), Mill(0,0), Warehouse(2,0)"
			<< "\nActive truck plan: Farm OR-" << snapshot.truckPlan.farmId << " -> Mill OR-" << snapshot.truckPlan.millId
			<< " -> Storage OR-" << snapshot.truckPlan.storageId;
		if (truckPlan.farmId != snapshot.truckPlan.farmId || truckPlan.millId != snapshot.truckPlan.millId ||
			truckPlan.storageId != snapshot.truckPlan.storageId)
		{
			text << "\nDraft truck plan (not active): Farm OR-" << truckPlan.farmId << " -> Mill OR-" << truckPlan.millId
				<< " -> Storage OR-" << truckPlan.storageId;
		}
		else
		{
			text << "\nDraft truck plan matches the active plan.";
		}
		for (const auto& pending : snapshot.pendingTruckPlans)
		{
			text << "\nPending truck plan command " << pending.commandId << ": Farm OR-" << pending.plan.farmId
				<< " -> Mill OR-" << pending.plan.millId << " -> Storage OR-" << pending.plan.storageId
				<< " (queued; not active yet).";
		}
		queueTruckPlanButton.SetText(snapshot.pendingTruckPlans.empty() ? "Queue truck plan" : "Plan pending...");
		if (const auto id = GetSelectedSimulationId())
		{
			const auto facility = std::find_if(snapshot.facilities.begin(), snapshot.facilities.end(), [&](const auto& item) {
				return item.id == *id;
			});
			const auto storage = std::find_if(snapshot.storages.begin(), snapshot.storages.end(), [&](const auto& item) {
				return item.id == *id;
			});
			if (facility != snapshot.facilities.end())
			{
				text << "\nSelected OR-" << *id << " "
					<< (facility->type == openrepublic::sim::FacilityType::Farm ? "Farm" : "Mill")
					<< " inventory: grain " << facility->inventory.grain << ", flour " << facility->inventory.flour;
			}
			else if (storage != snapshot.storages.end())
			{
				text << "\nSelected OR-" << *id << " Storage inventory: grain " << storage->inventory.grain
					<< ", flour " << storage->inventory.flour;
			}
		}
		if (!uiNotice.empty())
		{
			text << "\n" << uiNotice;
		}
		simulationStatus.SetText(text.str());
	}

	void OpenRepublicApplication::SetEntityColor(wi::ecs::Entity entity, const XMFLOAT4& color)
	{
		if (wi::scene::MaterialComponent* material = previewWorld.materials.GetComponent(entity))
		{
			material->SetBaseColor(color);
		}
	}
}
