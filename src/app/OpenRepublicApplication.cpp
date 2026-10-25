#include "OpenRepublicApplication.h"

// Windows headers alias FindResource to FindResourceW, conflicting with Catalog's lookup API.
#ifdef FindResource
#undef FindResource
#endif
#include "ShowcaseProps.h"
#include "openrepublic/procgeo/vehicles.h"
#include "ModelImporter.h"
#include "ContentPreview.h"
#include "MapShowcaseCapture.h"
#include "NmfPreviewCapture.h"
#include "SyntheticRegionFixture.h"
#include "wiHelper.h"
#include "openrepublic/content/glb_container.h"
#include "openrepublic/content/glb_semantic.h"
#include "openrepublic/content/mtl_dependencies.h"
#include "openrepublic/content/mtl_preview_color.h"
#include "openrepublic/content/mtl_preview_texture.h"
#include "openrepublic/content/mtl_submaterial_binding.h"
#include "openrepublic/content/mtl_syntax.h"
#include "openrepublic/content/nmf_geometry.h"
#include "openrepublic/content/nmf_preview_geometry.h"
#include "openrepublic/content/terrain_dds.h"
#include "openrepublic/content/terrain_preview.h"
#include "openrepublic/compat/catalog_import.h"
#include "openrepublic/compat/region_roads.h"

#ifdef OPENREPUBLIC_WITH_LUA_RULES
#include "openrepublic/scripting/lua_rules_loader.h"
#endif

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include "openrepublic/runtime/checkpoint_file.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace openrepublic
{
	using preview::CreateNmfStaticPreview;
	using preview::CreateTerrainPreview;
	using preview::HasValidIndexedTriangleData;
	using preview::ScopedGlbSnapshot;
	using preview::ValidateEmbeddedGlbProfile;
	using preview::kGlbProbeLimitations;

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
		// Current synthetic storage capacity also shown by the demo stock readout.
		constexpr std::uint32_t kStorageFlourCapacity = 5;
		constexpr std::size_t kMaxPendingRegionCommands = 64;

		std::string PendingTruckPlanCommandLabel(const openrepublic::agent_api::AgentApi& api,
			std::uint64_t simulationCommandId)
		{
			if (const auto apiHandle = api.LookupApiCommandHandleForSimulationCommand(simulationCommandId))
			{
				return "API command " + std::to_string(*apiHandle);
			}
			return "simulation command ID " + std::to_string(simulationCommandId) + " (no API handle)";
		}

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

		// Applies the renderer-independent settings to a Wicked path. Anything the device or
		// backend cannot do is logged, never silently dropped.
		void ApplyGraphicsSettings(wi::RenderPath3D& path, const openrepublic::runtime::GraphicsSettings& settings)
		{
			using openrepublic::runtime::AmbientOcclusion;
			using openrepublic::runtime::Upscaler;
			const bool rayTracing = wi::graphics::GetDevice() != nullptr &&
				wi::graphics::GetDevice()->CheckCapability(wi::graphics::GraphicsDeviceCapability::RAYTRACING);
			wilog("[OpenRepublic] Graphics settings: %s (hardware ray tracing %s)",
				openrepublic::runtime::FormatGraphicsSettings(settings).c_str(), rayTracing ? "available" : "unavailable");

			path.setFSR2Enabled(false);
			switch (settings.upscaler)
			{
			case Upscaler::None:
				break;
			case Upscaler::Fsr2Quality:
				path.setFSR2Preset(wi::RenderPath3D::FSR2_Preset::Quality);
				path.setFSR2Enabled(true);
				break;
			case Upscaler::Fsr2Balanced:
				path.setFSR2Preset(wi::RenderPath3D::FSR2_Preset::Balanced);
				path.setFSR2Enabled(true);
				break;
			case Upscaler::Fsr2Performance:
				path.setFSR2Preset(wi::RenderPath3D::FSR2_Preset::Performance);
				path.setFSR2Enabled(true);
				break;
			case Upscaler::Fsr2UltraPerformance:
				path.setFSR2Preset(wi::RenderPath3D::FSR2_Preset::Ultra_Performance);
				path.setFSR2Enabled(true);
				break;
			case Upscaler::Dlss:
				wilog("[OpenRepublic] DLSS requested but unavailable: the NVIDIA Streamline SDK is not integrated; rendering without an upscaler");
				break;
			}

			wi::RenderPath3D::AO ao = wi::RenderPath3D::AO_DISABLED;
			switch (settings.ambientOcclusion)
			{
			case AmbientOcclusion::Off: break;
			case AmbientOcclusion::Ssao: ao = wi::RenderPath3D::AO_SSAO; break;
			case AmbientOcclusion::Hbao: ao = wi::RenderPath3D::AO_HBAO; break;
			case AmbientOcclusion::Msao: ao = wi::RenderPath3D::AO_MSAO; break;
			case AmbientOcclusion::RayTraced:
				if (rayTracing)
				{
					ao = wi::RenderPath3D::AO_RTAO;
				}
				else
				{
					wilog("[OpenRepublic] Ray-traced AO requested without hardware ray tracing; AO disabled");
				}
				break;
			}
			path.setAO(ao);

			const bool wantsRayTracing = settings.rayTracedReflections || settings.rayTracedDiffuse;
			if (wantsRayTracing && !rayTracing)
			{
				wilog("[OpenRepublic] Ray-traced reflections/diffuse requested without hardware ray tracing; they stay off");
			}
			path.setRaytracedReflectionsEnabled(settings.rayTracedReflections && rayTracing);
			path.setRaytracedDiffuseEnabled(settings.rayTracedDiffuse && rayTracing);
			path.setSSREnabled(settings.screenSpaceReflections);
			path.setFXAAEnabled(settings.fxaa);
			path.setBloomEnabled(settings.bloom);
			path.setExposure(settings.exposure);
			if (settings.sharpen >= 0.0f)
			{
				path.setSharpenFilterEnabled(true);
				path.setSharpenFilterAmount(settings.sharpen);
			}
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

		bool BuildAppOwnedRegionEconomy(
			const openrepublic::content::MapPlacementRequest& request,
			const std::filesystem::path& contentRoot, double radiusMeters,
			const openrepublic::compat::CatalogImportOptions& catalogOptions,
			openrepublic::economy::Catalog& catalog,
			std::unique_ptr<openrepublic::economy::World>& world,
			openrepublic::compat::RegionImportReport& importReport,
			std::string& description, std::string& error)
		{
			using namespace openrepublic;
			const std::filesystem::path definitionsPath = contentRoot / "buildings_types";
			std::error_code fileError;
			if (!std::filesystem::is_directory(definitionsPath, fileError) || fileError)
			{
				error = "building definitions directory is unavailable: " + definitionsPath.string();
				return false;
			}

			std::map<std::string, content::BuildingDefinition> definitions;
			for (std::filesystem::directory_iterator it(definitionsPath, fileError), end;
				it != end && !fileError; it.increment(fileError))
			{
				if (!it->is_regular_file(fileError) || fileError || it->path().extension() != ".ini")
				{
					fileError.clear();
					continue;
				}
				std::ifstream input(it->path(), std::ios::binary);
				if (!input)
				{
					continue;
				}
				std::ostringstream buffer;
				buffer << input.rdbuf();
				content::BuildingDefinition definition;
				if (input.bad() || !content::ParseBuildingDefinition(buffer.str(), definition, nullptr))
				{
					continue;
				}
				definitions[it->path().stem().string()] = std::move(definition);
			}
			if (fileError)
			{
				error = "could not enumerate building definitions: " + fileError.message();
				return false;
			}
			if (definitions.empty())
			{
				error = "no parseable building definitions were found under " + definitionsPath.string();
				return false;
			}

			compat::ImportReport catalogReport;
			if (!compat::ImportCatalogFromDefinitions(definitions, catalogOptions, catalog, catalogReport))
			{
				error = "economy catalog import failed: " + catalogReport.error;
				return false;
			}
			if (request.placementIndex >= request.allPlacements.size())
			{
				error = "map region centre placement is outside the loaded placement list";
				return false;
			}

			std::vector<compat::PlacedBuildingRef> placements;
			placements.reserve(request.allPlacements.size());
			for (std::size_t index = 0; index < request.allPlacements.size(); ++index)
			{
				const auto& placed = request.allPlacements[index];
				placements.push_back({ placed.typeName, placed.X(), placed.Z(),
					static_cast<std::uint64_t>(index), placed.sourceBuildingTableIndex });
			}

			compat::RegionImportOptions importOptions;
			importOptions.radiusMeters = radiusMeters;
			if (!compat::ImportRegionIntoWorld(placements, request.placementIndex, catalog,
				importOptions, world, importReport))
			{
				error = "economy region import failed: " + compat::DescribeRegionImport(importReport);
				return false;
			}

			std::vector<compat::RoadLinkRef> roadLinks;
			if (request.hasRoads)
			{
				for (std::size_t i = 0; i < request.roads.nodes.size(); ++i)
				{
					for (const std::uint32_t j : request.roads.nodes[i].neighbors)
					{
						if (j > i && j < request.roads.nodes.size())
						{
							roadLinks.push_back({ request.roads.nodes[i].x, request.roads.nodes[i].z,
								request.roads.nodes[j].x, request.roads.nodes[j].z });
						}
					}
				}
			}
			compat::RegionRoadOptions roadOptions;
			roadOptions.maxConnectorCells = 120;
			compat::RegionRoadReport roadReport;
			if (!compat::ImportRoadsIntoWorld(*world, importReport, importOptions,
				roadLinks, roadOptions, roadReport))
			{
				error = "economy road import failed";
				world.reset();
				return false;
			}
			std::string fleetDescription;
			if (catalogOptions.syntheticRegionFleetCount > 0)
			{
				economy::BuildingTypeId depotType{};
				economy::BuildingId depotId{};
				std::string depotError;
				if (!catalog.FindBuildingType("openrepublic_synthetic_fleet_depot", depotType) ||
					!compat::PlaceSyntheticRegionFleetDepot(*world, depotType, depotId, depotError))
				{
					error = depotError.empty() ? "synthetic fleet depot type was not created" : depotError;
					world.reset();
					return false;
				}
				fleetDescription = " OpenRepublic synthetic fleet: " +
					std::to_string(catalogOptions.syntheticRegionFleetCount) +
					" trucks at depot #" + std::to_string(depotId) + ".";
			}

			std::ostringstream summary;
			summary << "App-owned region economy (experimental): "
				<< compat::DescribeImport(catalogReport) << " "
				<< compat::DescribeRegionImport(importReport) << " "
				<< compat::DescribeRegionRoads(roadReport) << fleetDescription;
			description = summary.str();
			return true;
		}
	}

	OpenRepublicApplication::OpenRepublicApplication()
		: agentApi(simulation, openrepublic::agent_api::Permissions{true, true, true, true, true})
	{
	}

	void OpenRepublicApplication::SetSceneProbePath(std::filesystem::path path)
	{
		sceneProbePath = std::move(path);
	}

	void OpenRepublicApplication::SetGlbProbePath(std::filesystem::path path)
	{
		glbProbePath = std::move(path);
	}

	void OpenRepublicApplication::SetNmfProbePath(std::filesystem::path path)
	{
		nmfProbePath = std::move(path);
	}

	void OpenRepublicApplication::SetNmfTextureRoots(std::vector<std::filesystem::path> roots)
	{
		nmfTextureRoots = std::move(roots);
	}

	void OpenRepublicApplication::SetNmfMaterialPath(std::filesystem::path path)
	{
		nmfMaterialPath = std::move(path);
	}

	void OpenRepublicApplication::SetMapPlacement(openrepublic::content::MapPlacementRequest request, double radiusMeters,
		bool regionPreview, std::filesystem::path contentRoot, int regionModelLevel,
		bool dynamicLod, float lodNearDistance, float lodFarDistance)
	{
		mapPlacement = std::move(request);
		mapRadiusMeters = radiusMeters;
		mapRegionPreview = regionPreview;
		mapRegionModelLevel = regionModelLevel;
		mapRegionDynamicLod = dynamicLod;
		mapLodNearDistance = lodNearDistance;
		mapLodFarDistance = lodFarDistance;
		mapContentRoot = std::move(contentRoot);
	}

	void OpenRepublicApplication::SetSyntheticRegionFixture()
	{
		syntheticRegionFixtureMode = true;
		mapRegionPreview = true;
	}

	void OpenRepublicApplication::SetEconomyCheckpoint(economy::RestoredCheckpoint checkpoint, bool freshWorld)
	{
		nativeEconomyCheckpoint = std::move(checkpoint);
		nativeEconomyFreshWorld = freshWorld;
		nativeObjectiveBaseline.clear();
		if (nativeObjectiveSet && nativeEconomyCheckpoint.world)
			nativeObjectiveBaseline = economy::SaveWorld(*nativeEconomyCheckpoint.world);
		nativeEconomyCheckpointMode = nativeEconomyCheckpoint.catalog && nativeEconomyCheckpoint.world;
		nativeSaveDirectory = runtime::DefaultSaveDirectory();
		nativeSavedRevision = nativeEconomyCheckpointMode ? nativeEconomyCheckpoint.world->Snapshot().revision : 0;
		nextNativeAutosaveTick = nativeEconomyCheckpointMode ? nativeEconomyCheckpoint.world->Snapshot().tick + 2400 : 2400;
		std::vector<runtime::SaveSlotInfo> slots;
		std::string slotsError;
		if (runtime::ListSaveSlots(nativeSaveDirectory, slots, slotsError))
		{
			std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
			for (const auto& slot : slots)
			{
				if (slot.name.rfind("autosave-", 0) != 0) continue;
				const auto suffix = slot.name.substr(9);
				try { const auto number = static_cast<std::size_t>(std::stoul(suffix)); if (number >= 1 && number <= 3 && slot.savedAtUnixSeconds <= oldest) { oldest = slot.savedAtUnixSeconds; nextNativeAutosaveSlot = number % 3; } } catch (...) {}
			}
		}
		nativeCoalDemo = false; nativeCoalAutoRun = false;
		coalDemoBorder = coalDemoDepot = coalDemoStorage = coalDemoPlant = 0; coalDemoResource = coalDemoGrain = economy::InvalidResourceId;
		if (nativeEconomyCheckpointMode) {
			auto& catalog = *nativeEconomyCheckpoint.catalog;
			auto& world = *nativeEconomyCheckpoint.world;
			economy::BuildingTypeId ignored{};
			nativeCoalDemo = catalog.FindBuildingType("border_post", ignored);
			if (nativeCoalDemo) {
				if (!catalog.FindResource("coal", coalDemoResource)) coalDemoResource = economy::InvalidResourceId;
				if (!catalog.FindResource("grain", coalDemoGrain)) coalDemoGrain = economy::InvalidResourceId;
				const auto snap = world.Snapshot();
				for (const auto& b : snap.buildings) { const auto* type = catalog.BuildingType(b.type); if (!type) continue;
					if (type->name == "border_post") coalDemoBorder = b.id;
					else if (type->name == "road_depot") coalDemoDepot = b.id;
					else if (type->name == "storage") coalDemoStorage = b.id;
					else if (type->name == "heating_plant") coalDemoPlant = b.id;
				}
			}
		}
		mapRegionPreview = nativeEconomyCheckpointMode;
	}

	void OpenRepublicApplication::SetNativeObjectives(runtime::NativeObjectiveSet objectiveSet)
	{
		nativeObjectiveSet = std::move(objectiveSet);
	}

	void OpenRepublicApplication::SetNativeTerrainEnabled(bool enabled)
	{
		nativeTerrainEnabled = enabled;
	}

	void OpenRepublicApplication::SetMapShowcaseCapture(std::filesystem::path outputPath, bool enhancedLook,
		float cameraYawDegrees, float cameraElevationDegrees, float cameraDistanceScale,
		bool vegetationWind, bool grassLayer)
	{
		mapShowcaseCaptureMode = !outputPath.empty();
		mapShowcaseCapturePath = std::move(outputPath);
		mapShowcaseEnhancedLook = enhancedLook;
		mapShowcaseCameraYawDegrees = cameraYawDegrees;
		mapShowcaseCameraElevationDegrees = cameraElevationDegrees;
		mapShowcaseCameraDistanceScale = cameraDistanceScale;
		showcaseVegetationWind = vegetationWind;
		showcaseGrassLayer = grassLayer;
		if (enhancedLook && !graphicsSettings.has_value())
		{
			openrepublic::runtime::GraphicsSettings settings;
			settings.ambientOcclusion = openrepublic::runtime::AmbientOcclusion::Ssao;
			settings.fxaa = true;
			settings.bloom = true;
			settings.exposure = 0.96f;
			settings.sharpen = 0.15f;
			graphicsSettings = settings;
		}
	}

	void OpenRepublicApplication::SetGraphicsSettings(openrepublic::runtime::GraphicsSettings settings)
	{
		graphicsSettings = settings;
	}

	void OpenRepublicApplication::SetEconomyOverlay(std::string host, int port)
	{
		economyHost = std::move(host);
		economyPort = port;
	}

	void OpenRepublicApplication::SetLocalEconomyPipeName(std::wstring name)
	{
		localEconomyPipeName = std::move(name);
	}

	void OpenRepublicApplication::SetRegionCatalogImportOptions(openrepublic::compat::CatalogImportOptions options)
	{
		regionCatalogImportOptions = std::move(options);
	}

	void OpenRepublicApplication::SetSceneScriptPath(std::filesystem::path path)
	{
		sceneScriptPath = std::move(path);
	}

	void OpenRepublicApplication::SetScenePipeName(std::wstring name)
	{
		scenePipeName = std::move(name);
	}

	void OpenRepublicApplication::ApplySceneLook(const openrepublic::scenectl::SceneLook& look)
	{
		constexpr float degreesToRadians = XM_PI / 180.0f;
		const float cameraYawDegrees = static_cast<float>(look.camera.yawDegrees);
		// Sun: same construction as the initial setup (direction from the scene toward the sun).
		const float elevation = static_cast<float>(look.sunElevationDegrees) * degreesToRadians;
		const float yaw = (cameraYawDegrees + static_cast<float>(look.sunYawOffsetDegrees)) * degreesToRadians;
		const XMFLOAT3 sunDirection(std::sin(yaw) * std::cos(elevation), std::sin(elevation), std::cos(yaw) * std::cos(elevation));
		const float warmth = std::clamp((static_cast<float>(look.sunElevationDegrees) - 4.0f) / 46.0f, 0.0f, 1.0f);
		const XMFLOAT3 sunColor(1.0f, 0.66f + 0.30f * warmth, 0.46f + 0.46f * warmth);
		nativeEconomyScene.SetDarkLighting(look.sunElevationDegrees <= 3.0f);
		if (wi::scene::TransformComponent* sunTransform = previewWorld.transforms.GetComponent(showcaseSunEntity))
		{
			sunTransform->ClearTransform();
			const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
			const XMVECTOR target = XMLoadFloat3(&sunDirection);
			const XMVECTOR axis = XMVector3Normalize(XMVector3Cross(worldUp, target));
			const float angle = std::acos(std::clamp(sunDirection.y, -1.0f, 1.0f));
			sunTransform->Rotate(XMQuaternionRotationAxis(axis, angle));
			sunTransform->UpdateTransform();
		}
		if (wi::scene::LightComponent* sun = previewWorld.lights.GetComponent(showcaseSunEntity))
		{
			sun->color = sunColor;
			sun->SetCastShadow(look.shadowsEnabled);
		}
		if (wi::scene::WeatherComponent* weather = previewWorld.weathers.GetComponent(showcaseWeatherEntity))
		{
			weather->sunColor = sunColor;
			weather->sunDirection = sunDirection;
			weather->SetOceanEnabled(look.oceanEnabled && showcaseEnvironment.hasWater);
			weather->volumetricCloudParameters.layerFirst.coverageAmount = static_cast<float>(0.15 + 0.7 * look.cloudCoverage);
			weather->fogDensity = static_cast<float>(0.00035 + 0.003 * look.fogDensity);
		}
		{
			// Keep the baked terrain tint balanced when the sun moves (materials multiply the vertex colours).
			const float ratio = preview::ShowcaseTerrainSunGain(static_cast<float>(look.sunElevationDegrees)) /
				std::max(0.01f, preview::ShowcaseBakedTerrainSunGain());
			for (std::size_t i = 0; i < previewWorld.names.GetCount(); ++i)
			{
				const std::string& name = previewWorld.names[i].name;
				if (name == "OpenRepublic map region terrain" || name == "OpenRepublic showcase far landscape")
				{
					const wi::ecs::Entity entity = previewWorld.names.GetEntity(i);
					if (wi::scene::MaterialComponent* material = previewWorld.materials.GetComponent(entity))
					{
						material->SetBaseColor(XMFLOAT4(ratio, ratio, ratio, 1.0f));
					}
				}
			}
		}
		renderPath->setExposure(static_cast<float>(look.exposure));
		renderPath->setBloomEnabled(look.bloomEnabled);
		renderPath->setAO(look.rtaoEnabled ? wi::RenderPath3D::AO_RTAO : (look.ssaoEnabled ? wi::RenderPath3D::AO_HBAO : wi::RenderPath3D::AO_DISABLED));
		renderPath->setRaytracedReflectionsEnabled(look.rayTracedReflectionsEnabled);

		// Camera: orbit the region centre like the showcase capture camera.
		const float cameraElevation = static_cast<float>(look.camera.elevationDegrees) * degreesToRadians;
		const float cameraYaw = cameraYawDegrees * degreesToRadians;
		const float distance = sceneCameraDistance * static_cast<float>(look.camera.distanceFactor);
		const XMFLOAT3 direction(std::sin(cameraYaw) * std::cos(cameraElevation), std::sin(cameraElevation), std::cos(cameraYaw) * std::cos(cameraElevation));
		wi::scene::TransformComponent cameraTransform;
		cameraTransform.Translate(XMFLOAT3(sceneCameraTarget.x + distance * direction.x,
			sceneCameraTarget.y + distance * direction.y, sceneCameraTarget.z + distance * direction.z));
		cameraTransform.RotateRollPitchYaw(XMFLOAT3(cameraElevation, std::atan2(-direction.x, -direction.z), 0.0f));
		cameraTransform.UpdateTransform();
		renderPath->camera->TransformCamera(cameraTransform);
		renderPath->camera->UpdateCamera();
	}

	void OpenRepublicApplication::UpdateSceneControl(float dt)
	{
		if ((scenePipeName.empty() && sceneScriptPath.empty()) || !mapRegionPreview || !sceneCameraKnown || renderPath == nullptr)
		{
			return;
		}
		if (!sceneControl)
		{
			openrepublic::scenectl::SceneLook initial;
			if (std::isfinite(mapShowcaseSunElevationDegrees))
			{
				initial.sunElevationDegrees = mapShowcaseSunElevationDegrees;
				initial.sunYawOffsetDegrees = mapShowcaseSunYawOffsetDegrees;
			}
			initial.camera.yawDegrees = mapShowcaseCameraYawDegrees;
			initial.camera.elevationDegrees = mapShowcaseCameraElevationDegrees;
			initial.camera.distanceFactor = 1.0;
			initial.exposure = renderPath->getExposure();
			sceneControl = std::make_unique<openrepublic::scenectl::SceneControlState>(initial);
			if (!scenePipeName.empty())
			{
				sceneEndpoint = std::make_unique<openrepublic::economy_host::LocalEconomyPipeEndpoint>(scenePipeName);
				std::string endpointError;
				if (!sceneEndpoint->Start(endpointError))
				{
					wilog("[OpenRepublic] Scene control endpoint failed: %s", endpointError.c_str());
					sceneEndpoint.reset();
					scenePipeName.clear();
				}
				else
				{
					wilog("[OpenRepublic] Scene control endpoint listening (JSONL protocol v%u).", static_cast<unsigned>(openrepublic::scenectl::ProtocolVersion));
				}
			}
#ifdef OPENREPUBLIC_WITH_LUA_RULES
			if (!sceneScriptPath.empty())
			{
				sceneScript = std::make_unique<openrepublic::scripting::SceneScript>();
				std::string scriptError;
				if (sceneScript->LoadFile(sceneScriptPath.string(), &scriptError))
				{
					wilog("[OpenRepublic] Scene script loaded: %s", sceneScriptPath.string().c_str());
				}
				else
				{
					wilog("[OpenRepublic] Scene script rejected: %s", scriptError.c_str());
					sceneScript.reset();
				}
			}
#endif
		}
#ifdef OPENREPUBLIC_WITH_LUA_RULES
		if (sceneScript)
		{
			if (!sceneScript->Tick(static_cast<double>(dt), *sceneControl))
			{
				wilog("[OpenRepublic] Scene script stopped: %s", sceneScript->LastError().c_str());
				sceneScript.reset();
			}
			else
			{
				const std::string logs = sceneScript->TakeLogs();
				if (!logs.empty())
				{
					wilog("[OpenRepublic] Scene script: %s", logs.c_str());
				}
			}
		}
#endif
		for (int handled = 0; handled < 8 && sceneEndpoint; ++handled)
		{
			openrepublic::economy_host::LocalEconomyPipeEndpoint::Request request;
			if (!sceneEndpoint->TryPop(request))
			{
				break;
			}
			try
			{
				sceneEndpoint->Complete(request.token, sceneControl->Apply(request.line));
			}
			catch (...)
			{
				sceneEndpoint->Complete(request.token, {});
			}
		}
		if (sceneControl->ConsumeDirty())
		{
			ApplySceneLook(sceneControl->CurrentLook());
		}
	}

	void OpenRepublicApplication::SetMapShowcaseSun(float elevationDegrees, float yawOffsetDegrees)
	{
		mapShowcaseSunElevationDegrees = elevationDegrees;
		preview::SetShowcaseBakedSunElevation(elevationDegrees);
		mapShowcaseSunYawOffsetDegrees = yawOffsetDegrees;
	}

	void OpenRepublicApplication::SetNmfUnlitPreview(bool enabled)
	{
		nmfUnlitPreview = enabled;
	}

	void OpenRepublicApplication::SetNmfMaterialUnlitPreview(bool enabled)
	{
		nmfMaterialUnlitPreview = enabled;
	}

	void OpenRepublicApplication::SetNmfCapturePaths(std::vector<std::filesystem::path> paths)
	{
		nmfCaptureMode = !paths.empty();
		nmfCapturePaths = std::move(paths);
	}

	void OpenRepublicApplication::SetNmfCaptureZoom(float zoom)
	{
		if (std::isfinite(zoom) && zoom >= 0.5f && zoom <= 2.5f)
		{
			nmfCaptureZoom = zoom;
		}
	}

	void OpenRepublicApplication::SetNmfCaptureElevationDegrees(std::optional<float> elevationDegrees)
	{
		if (!elevationDegrees.has_value() ||
			(std::isfinite(*elevationDegrees) && *elevationDegrees >= 0.0f && *elevationDegrees <= 60.0f))
		{
			nmfCaptureElevationDegrees = elevationDegrees;
		}
	}

	void OpenRepublicApplication::SetTerrainProbePath(std::filesystem::path path)
	{
		terrainProbePath = std::move(path);
	}

	void OpenRepublicApplication::SetTerrainProbeTransform(
		std::optional<openrepublic::content::TerrainTransform> transform, std::string error)
	{
		terrainProbeTransform = std::move(transform);
		terrainProbeTransformError = std::move(error);
	}

	void OpenRepublicApplication::Initialize()
	{
		wi::Application::Initialize();
		const bool capturePreview = !nmfCapturePaths.empty() || mapShowcaseCaptureMode;
		bool terrainPreviewLoaded = false;
		bool nmfPreviewLoaded = false;
		preview::SyntheticRegionFixtureData syntheticRegionData;
		XMFLOAT3 nmfPreviewCameraTarget(
			static_cast<float>(openrepublic::content::NmfPreviewCenterX),
			static_cast<float>(openrepublic::content::NmfPreviewDisplaySpan * 0.5), 0.0f);
		float nmfPreviewCameraDistance = static_cast<float>(
			openrepublic::content::NmfPreviewDisplaySpan *
			openrepublic::content::NmfPreviewCameraDistanceFactor);

		#ifdef OPENREPUBLIC_WITH_LUA_RULES
		{
			openrepublic::sim::SimulationRules initialRules;
			std::string error;
			const auto rulesPath = LocateDemoRulesFile();
			if (openrepublic::scripting::LuaRulesLoader::LoadFile(rulesPath, initialRules, &error) &&
				agentApi.SetSimulationRules(QuerySimulationSnapshot().stateRevision, initialRules, &error) ==
					openrepublic::agent_api::ErrorCode::None)
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
		if (nativeEconomyCheckpointMode)
		{
			std::string error;
			const auto snapshot = nativeEconomyCheckpoint.world->Snapshot();
			if (nativeEconomyScene.Initialize(previewWorld, snapshot, *nativeEconomyCheckpoint.catalog,
				regionInstances, error, nativeTerrainEnabled))
			{
				nativeEconomyPresentationValid = true;
				nmfPreviewCameraTarget = nativeEconomyScene.CameraTarget();
				nmfPreviewCameraDistance = nativeEconomyScene.CameraDistance() * (nativeCoalDemo ? 0.68f : 1.0f);
				nmfPreviewLoaded = true;
				regionEconomyApi = std::make_unique<economy_api::EconomyApi>(
					*nativeEconomyCheckpoint.world, economy_api::Permissions{ true, true });
				regionEconomyDescription = nativeEconomyFreshWorld
					? "Native economy sandbox — authored catalog, 64 x 64 cells"
					: "Native economy checkpoint — grid view";
				if (nativeTerrainEnabled) regionEconomyDescription += " — synthetic terrain (seed 20261025)";
				uiNotice = nativeEconomyFreshWorld
					? "Choose a building and Place building, then click a ground cell. Commands apply when you step."
					: "Restored native economy; pending commands apply on the next step. Display scale: 4 units per cell.";
				if (nativeTerrainEnabled) uiNotice += " Terrain uses OpenRepublic synthetic rules; WRSR parity UNKNOWN.";
				RefreshRegionEconomy();
				UpdateRegionEconomyPresentation();
				StartLocalEconomyEndpoint();
				wilog("[OpenRepublic] Native economy: tick %llu, revision %llu, %zu buildings, %zu roads.",
					static_cast<unsigned long long>(snapshot.tick), static_cast<unsigned long long>(snapshot.revision),
					snapshot.buildings.size(), snapshot.roads.size());
			}
			else
			{
				uiNotice = "Native checkpoint scene could not be created: " + error;
				wilog("[OpenRepublic] %s", uiNotice.c_str());
			}
		}
		else if (syntheticRegionFixtureMode)
		{
			std::string error;
			if (preview::CreateSyntheticRegionFixture(previewWorld, syntheticRegionData, error))
			{
				regionInstances = syntheticRegionData.instances;
				syntheticRegionCenterX = syntheticRegionData.centerX;
				syntheticRegionCenterZ = syntheticRegionData.centerZ;
				nmfPreviewCameraTarget = syntheticRegionData.cameraTarget;
				nmfPreviewCameraDistance = syntheticRegionData.cameraDistance;
				nmfPreviewLoaded = true;
				uiNotice = syntheticRegionData.description;
				wilog("[OpenRepublic] %s", syntheticRegionData.description.c_str());
			}
			else
			{
				uiNotice = "Synthetic region fixture could not be created: " + error;
				wilog("[OpenRepublic] %s", uiNotice.c_str());
			}
		}
		else if (!capturePreview && !mapRegionPreview)
		{
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
		}

		const bool customSun = mapShowcaseEnhancedLook && std::isfinite(mapShowcaseSunElevationDegrees);
		// Warm, low sun for golden hour; near-white high sun at midday.
		const float sunWarmth = customSun
			? std::clamp((mapShowcaseSunElevationDegrees - 4.0f) / 46.0f, 0.0f, 1.0f) : 0.7f;
		const XMFLOAT3 previewSunColor = customSun
			? XMFLOAT3(1.0f, 0.66f + 0.30f * sunWarmth, 0.46f + 0.46f * sunWarmth)
			: (mapShowcaseEnhancedLook ? XMFLOAT3(1.0f, 0.92f, 0.82f) : XMFLOAT3(1.0f, 0.88f, 0.72f));
		const wi::ecs::Entity sun = previewWorld.Entity_CreateLight(
			"OpenRepublic preview light", XMFLOAT3(0, 0, 0), previewSunColor,
			4.0f, 100.0f, wi::scene::LightComponent::DIRECTIONAL);
		showcaseSunEntity = sun;
		if (wi::scene::TransformComponent* transform = previewWorld.transforms.GetComponent(sun))
		{
			if (mapShowcaseEnhancedLook)
			{
				// Wicked's directional light points along the transform's local +Y axis.
				// Match the camera's complete outward direction so the showcase sun sits
				// directly behind the camera, including its elevation, at every preset.
				constexpr float degreesToRadians = XM_PI / 180.0f;
				const float sunElevationRadians = (customSun ? mapShowcaseSunElevationDegrees
					: mapShowcaseCameraElevationDegrees) * degreesToRadians;
				const float cameraYawRadians = (mapShowcaseCameraYawDegrees +
					(customSun ? mapShowcaseSunYawOffsetDegrees : 0.0f)) * degreesToRadians;
				const float horizontalSun = std::cos(sunElevationRadians);
				const XMFLOAT3 sunDirection(
					std::sin(cameraYawRadians) * horizontalSun,
					std::sin(sunElevationRadians),
					std::cos(cameraYawRadians) * horizontalSun);
				const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
				const XMVECTOR targetDirection = XMLoadFloat3(&sunDirection);
				const XMVECTOR axis = XMVector3Normalize(XMVector3Cross(worldUp, targetDirection));
				const float angle = std::acos(std::clamp(sunDirection.y, -1.0f, 1.0f));
				transform->Rotate(XMQuaternionRotationAxis(axis, angle));
			}
			else
			{
				transform->RotateRollPitchYaw(XMFLOAT3(-0.7f, 0.6f, 0.0f));
			}
			transform->UpdateTransform();
		}
		if (mapShowcaseEnhancedLook)
		{
			if (wi::scene::LightComponent* light = previewWorld.lights.GetComponent(sun))
			{
				// The sun is already aligned with the camera's outward ray. Raise its
				// direct contribution so camera-facing building surfaces read as sunlit.
				light->intensity = customSun ? 12.0f : 6.0f;
				light->forced_shadow_resolution = 2048;
				light->SetCastShadow(std::getenv("OR_NOSHADOW") == nullptr); // debug switch
				light->SetVolumetricCloudsEnabled(true);
			}
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
				std::vector<std::uint8_t> glbBytes;
				content::GlbContainerInfo containerInfo;
				content::GlbSemanticSummary semanticSummary;
				std::string preflightError;
				if (!content::ReadBoundedGlbContainer(glbProbePath, glbBytes, &preflightError) ||
					!content::PreflightGlbContainer(glbBytes, containerInfo, &preflightError))
				{
					uiNotice = "GLB container framing preflight failed: " + preflightError +
						" No import was attempted; " + std::string(kGlbProbeLimitations);
				}
				else if (!ValidateEmbeddedGlbProfile(glbBytes, containerInfo, semanticSummary, preflightError))
				{
					uiNotice = "GLB probe rejected the bounded semantic profile: " + preflightError + " " + kGlbProbeLimitations;
				}
				else
				{
					ScopedGlbSnapshot snapshot;
					if (!snapshot.Write(glbBytes, preflightError))
					{
						uiNotice = "GLB probe validated the file but could not create an exact temporary snapshot: " +
							preflightError + " No import was attempted.";
					}
					else
					{
						wi::scene::Scene stagingScene;
						bool importSucceeded = true;
						try
						{
							ImportModel_GLTF(snapshot.Path().u8string(), stagingScene);
						}
						catch (const std::exception& exception)
						{
							importSucceeded = false;
							uiNotice = std::string("Pinned GLB importer threw an exception: ") + exception.what();
						}
						catch (...)
						{
							importSucceeded = false;
							uiNotice = "Pinned GLB importer failed with an unknown exception.";
						}

						if (importSucceeded)
						{
							wi::unordered_set<wi::ecs::Entity> stagedEntities;
							stagingScene.FindAllEntities(stagedEntities);
							std::size_t usableMeshCount = 0;
							std::size_t stagedMeshCount = 0;
							bool allStagedMeshesValid = true;
							for (const wi::ecs::Entity entity : stagedEntities)
							{
								const wi::scene::MeshComponent* mesh = stagingScene.meshes.GetComponent(entity);
								if (mesh == nullptr)
								{
									continue;
								}
								++stagedMeshCount;
								if (HasValidIndexedTriangleData(*mesh))
								{
									++usableMeshCount;
								}
								else
								{
									allStagedMeshesValid = false;
								}
							}

							if (usableMeshCount == 0 || stagedMeshCount == 0 || !allStagedMeshesValid)
							{
								uiNotice = "Bounded GLB validation passed, but the Wicked importer produced no meshes or at least one mesh "
									"failed valid indexed triangle-data checks. " +
									std::string(kGlbProbeLimitations);
							}
							else
							{
								const std::size_t transformCount = stagingScene.transforms.GetCount();
								const std::size_t meshCount = stagedMeshCount;
								const std::size_t materialCount = stagingScene.materials.GetCount();
								previewWorld.Merge(stagingScene);
								uiNotice = "Imported local GLB probe after bounded profile validation: " + std::to_string(meshCount) + " meshes, " +
									std::to_string(materialCount) + " materials, " + std::to_string(transformCount) + " transforms. Input profile: " +
									std::to_string(semanticSummary.primitiveCount) + " triangles, " +
									std::to_string(semanticSummary.textureCount) + " textures, " +
									std::to_string(semanticSummary.imageCount) + " embedded images. " +
									kGlbProbeLimitations;
							}
						}
					}
				}
			}
			wilog("[OpenRepublic] GLB probe: %s", uiNotice.c_str());
		}

		if (!terrainProbeTransformError.empty())
		{
			uiNotice = "Terrain probe transform rejected: " + terrainProbeTransformError;
			wilog("[OpenRepublic] %s", uiNotice.c_str());
		}

		if (!nmfProbePath.empty() && !terrainProbePath.empty())
		{
			uiNotice = "NMF and terrain probes cannot be combined; choose one local content preview per launch.";
			wilog("[OpenRepublic] Content probe rejected: %s", uiNotice.c_str());
		}
		else if (!nmfProbePath.empty())
		{
			std::string geometryAuditSummary;
			if (!HasExtensionInsensitive(nmfProbePath, L".nmf"))
			{
				uiNotice = "NMF probe accepts only .nmf files.";
			}
			else
			{
				wi::ecs::Entity meshEntity = wi::ecs::INVALID_ENTITY;
				std::string description;
				std::string error;
				preview::NmfPreviewFrame previewFrame;
				bool created = false;
				if (mapRegionPreview && mapPlacement)
				{
					created = preview::CreateMapRegionPreview(previewWorld, *mapPlacement, mapContentRoot,
						mapRadiusMeters, mapRegionModelLevel, mapShowcaseEnhancedLook,
						nmfPreviewCameraTarget, nmfPreviewCameraDistance,
						description, error, &regionInstances, &showcaseEnvironment,
						&buildingLodController, mapRegionDynamicLod, mapLodNearDistance, mapLodFarDistance,
						showcaseVegetationWind, showcaseGrassLayer);
					geometryAuditSummary = "map region preview (per-model audits are in the records above)";
				}
				else
				{
					created = CreateNmfStaticPreview(previewWorld, nmfProbePath, meshEntity,
						nmfPreviewCameraTarget, nmfPreviewCameraDistance, nmfUnlitPreview,
						nmfMaterialUnlitPreview, nmfTextureRoots, nmfMaterialPath,
						description, geometryAuditSummary, error, &previewFrame);
				}
				if (created)
				{
					nmfPreviewLoaded = true;
					sceneCameraTarget = nmfPreviewCameraTarget;
					sceneCameraDistance = nmfPreviewCameraDistance;
					sceneCameraKnown = true;
					uiNotice = description;
					if (mapRegionPreview)
					{
						wilog("[OpenRepublic] %s", description.c_str());
						if (!nmfCaptureMode && !mapShowcaseCaptureMode && economyPort > 0)
						{
							economyOverlay = std::make_unique<openrepublic::preview::EconomyOverlay>(economyHost, economyPort);
						}
						else if (!nmfCaptureMode && !mapShowcaseCaptureMode && mapPlacement)
						{
							const double effectiveRadius = mapRadiusMeters > 0.0 ? mapRadiusMeters : 400.0;
							std::string economyDescription;
							std::string economyError;
							if (BuildAppOwnedRegionEconomy(*mapPlacement, mapContentRoot, effectiveRadius, regionCatalogImportOptions,
								regionEconomyCatalog, regionEconomyWorld, regionEconomyImport,
								economyDescription, economyError))
							{
								regionEconomyApi = std::make_unique<openrepublic::economy_api::EconomyApi>(
									*regionEconomyWorld, openrepublic::economy_api::Permissions{ true, true });
								const auto initial = regionEconomyApi->QueryWorld();
								if (initial.error == openrepublic::economy_api::ErrorCode::None)
								{
									regionEconomySnapshot = initial.world;
									regionEconomyDescription = economyDescription;
									regionWorldStatus = economyDescription;
									if (!localEconomyPipeName.empty())
									{
										localEconomyDispatcher = std::make_unique<openrepublic::economy_host::Dispatcher>(
											openrepublic::economy_host::Role::Controller, *regionEconomyApi, true, false);
										localEconomyDispatcher->SetRegionDescription(economyDescription);

										auto endpoint = std::make_unique<openrepublic::economy_host::LocalEconomyPipeEndpoint>(
											localEconomyPipeName);
										std::string endpointError;
										if (endpoint->Start(endpointError))
										{
											std::string endpointName;
											endpointName.reserve(localEconomyPipeName.size());
											for (wchar_t character : localEconomyPipeName)
											{
												endpointName.push_back(static_cast<char>(character));
											}
											uiNotice = "Trusted-local controller endpoint listening at \\\\.\\pipe\\" +
												endpointName + " (JSONL protocol v" +
												std::to_string(openrepublic::economy_host::ProtocolVersion) + ").";
											wilog("[OpenRepublic] %s", uiNotice.c_str());
											localEconomyPipeEndpoint = std::move(endpoint);
										}
										else
										{
											uiNotice = "Trusted-local controller endpoint could not start: " + endpointError;
											wilog("[OpenRepublic] %s", uiNotice.c_str());
											localEconomyDispatcher.reset();
										}
									}
									UpdateRegionEconomyPresentation();
								}
								else
								{
									regionEconomyApi.reset();
									regionEconomyWorld.reset();
									uiNotice = "Could not read the imported economy world through EconomyApi.";
								}
							}
							else
							{
								uiNotice = "Map preview loaded, but the app-owned economy could not start: " + economyError;
								wilog("[OpenRepublic] %s", uiNotice.c_str());
							}
						}
					}
					else if (mapPlacement)
					{
						std::string terrainDescription;
						std::string terrainError;
						if (preview::CreateMapTerrainAroundPlacement(previewWorld, *mapPlacement, previewFrame,
							mapRadiusMeters, terrainDescription, terrainError))
						{
							wilog("[OpenRepublic] %s", terrainDescription.c_str());
						}
						else
						{
							wilog("[OpenRepublic] Map terrain placement failed: %s", terrainError.c_str());
						}
					}
					// Fill light so faces turned away from the preview sun remain legible; the
					// synthetic scene keeps Wicked's default weather.
					const wi::ecs::Entity weatherEntity = wi::ecs::CreateEntity();
					previewWorld.names.Create(weatherEntity) = "OpenRepublic NMF preview weather";
					showcaseWeatherEntity = weatherEntity;
					wi::scene::WeatherComponent& weather = previewWorld.weathers.Create(weatherEntity);
					weather.ambient = XMFLOAT3(0.55f, 0.55f, 0.6f);
					weather.horizon = XMFLOAT3(0.55f, 0.6f, 0.7f);
					weather.zenith = XMFLOAT3(0.45f, 0.55f, 0.75f);
					if (mapRegionPreview && mapShowcaseEnhancedLook)
					{
						// Physically based sky: sun-driven scattering, aerial perspective on distant
						// terrain, soft volumetric clouds that also shade the ground.
						weather.ambient = XMFLOAT3(0.30f, 0.34f, 0.42f);
						weather.horizon = XMFLOAT3(0.56f, 0.68f, 0.84f);
						weather.zenith = XMFLOAT3(0.20f, 0.40f, 0.72f);
						weather.skyExposure = 1.0f;
						weather.SetRealisticSky(true);
						weather.SetRealisticSkyAerialPerspective(true);
						weather.SetRealisticSkyHighQuality(true);
						weather.SetRealisticSkyReceiveShadow(true);
						weather.SetOverrideFogColor(false);
						weather.SetHeightFog(true);
						weather.fogStart = 600.0f;
						weather.fogDensity = 0.00035f;
						weather.fogHeightStart = showcaseEnvironment.hasWater ? showcaseEnvironment.waterLevel : -50.0f;
						weather.fogHeightEnd = (showcaseEnvironment.hasWater ? showcaseEnvironment.waterLevel : 0.0f) + 140.0f;
						weather.SetVolumetricClouds(true);
						weather.SetVolumetricCloudsCastShadow(true);
						weather.SetVolumetricCloudsReceiveShadow(true);
						weather.volumetricCloudParameters.cloudStartHeight = 1500.0f;
						weather.volumetricCloudParameters.cloudThickness = 2200.0f;
						weather.volumetricCloudParameters.layerFirst.coverageAmount = 0.55f;
						weather.volumetricCloudParameters.layerFirst.coverageMinimum = 0.16f;
						weather.windDirection = XMFLOAT3(2.0f, 0.0f, 1.5f);
						weather.windSpeed = showcaseVegetationWind ? 1.25f : 0.0f;
						weather.windWaveSize = 0.75f;
						weather.windRandomness = 0.65f;
						weather.sunColor = previewSunColor;
						if (showcaseEnvironment.hasWater)
						{
							weather.SetOceanEnabled(true);
							weather.oceanParameters.waterHeight = showcaseEnvironment.waterLevel;
						// Calm, deep teal presentation water: keep vertical displacement well below
						// the terrain shore tolerance so the plane cannot wash over nearby buildings.
						weather.oceanParameters.wave_amplitude = 0.65f;
						weather.oceanParameters.wind_speed = 28.0f;
						weather.oceanParameters.patch_length = 220.0f;
						weather.oceanParameters.choppy_scale = 0.3f;
						weather.oceanParameters.surfaceDetail = 4;
						weather.oceanParameters.surfaceDisplacementTolerance = 0.8f;
						weather.oceanParameters.waterColor = XMFLOAT4(0.015f, 0.075f, 0.095f, 0.78f);
						weather.oceanParameters.extinctionColor = XMFLOAT4(0.10f, 0.32f, 0.36f, 1.0f);
							// OR_WATER_ORIGINAL=1: use the water colour the shared waterriver script declares
							// ($COLOR 0.15 0.2167 0.10; shader constant WaterColor, VERIFIED by static trace in
							// docs/MAP_WATER_DATA.md). Channel order/colour space are UNKNOWN: opt-in comparison aid, not parity.
							if (const char* originalWater = std::getenv("OR_WATER_ORIGINAL"); originalWater && std::string_view(originalWater) == "1")
								weather.oceanParameters.waterColor = XMFLOAT4(0.15f, 0.216667f, 0.10f, 0.78f);
						}
						if (const wi::scene::TransformComponent* sunTransform = previewWorld.transforms.GetComponent(sun))
						{
							// The renderer and Wicked's sky both use the light's local +Y
							// direction: from the scene toward the sun.
							weather.sunDirection = sunTransform->GetUp();
							wilog("[OpenRepublic] Showcase sun direction=(%.3f, %.3f, %.3f), camera yaw=%.2f deg, sun elevation=%s",
								weather.sunDirection.x, weather.sunDirection.y, weather.sunDirection.z,
								mapShowcaseCameraYawDegrees, customSun ? "custom" : "camera-aligned");
						}
						wilog("[OpenRepublic] Enhanced map showcase look: physically based sky with aerial perspective, volumetric clouds and valley fog, extended real-heightmap landscape with a lake, procedural trees and terrain/road detail textures (OpenRepublic presentation assets; WRSR terrain textures and vegetation remain unimported).");
					}
				}
				else
				{
					uiNotice = "NMF probe failed: " + error;
				}
			}
		if (nmfPreviewLoaded)
			{
				wilog("[OpenRepublic] NMF geometry audit: %s", geometryAuditSummary.c_str());
				wilog("[OpenRepublic] NMF limits: UV pair-to-set mapping is inferred; slot 0 to Wicked base-color/UV0 is an opt-in inference; other texture slots remain unmapped; source axes, winding, units, normal direction, and visual fidelity remain unverified.");
				if (nmfMaterialUnlitPreview)
				{
					wilog("[OpenRepublic] NMF capture mode: force-unlit while retaining MTL colors, UVs, and optional local slot-0 texture candidates.");
				}
			}
			else
			{
				wilog("[OpenRepublic] NMF probe: %s", uiNotice.c_str());
			}
		}
		else if (!terrainProbePath.empty())
		{
			wi::ecs::Entity terrainEntity = wi::ecs::INVALID_ENTITY;
			std::string description;
			std::string error;
			if (CreateTerrainPreview(previewWorld, terrainProbePath, terrainProbeTransform,
				terrainEntity, description, error))
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

		if (syntheticRegionFixtureMode && nmfPreviewLoaded)
		{
			using namespace openrepublic;
			regionEconomyCatalog = economy::MakeSyntheticCatalog();
			compat::RegionImportOptions importOptions;
			importOptions.radiusMeters = syntheticRegionData.radiusMeters;
			std::string economyError;
			if (compat::ImportRegionIntoWorld(syntheticRegionData.placements,
				syntheticRegionData.centerPlacementIndex, regionEconomyCatalog, importOptions,
				regionEconomyWorld, regionEconomyImport))
			{
				compat::RegionRoadOptions roadOptions;
				roadOptions.maxConnectorCells = 120;
				compat::RegionRoadReport roadReport;
				if (regionEconomyWorld && compat::ImportRoadsIntoWorld(*regionEconomyWorld,
					regionEconomyImport, importOptions, syntheticRegionData.roads, roadOptions, roadReport))
				{
					regionEconomyApi = std::make_unique<economy_api::EconomyApi>(
						*regionEconomyWorld, economy_api::Permissions{ true, true });
					const auto initial = regionEconomyApi->QueryWorld();
					if (initial.error == economy_api::ErrorCode::None)
					{
						regionEconomySnapshot = initial.world;
						std::ostringstream fixtureStatus;
						fixtureStatus << "Synthetic/open fixture (OpenRepublic-authored; no WRSR inputs)\n"
							<< "Import: " << regionEconomyImport.placed << "/" << regionEconomyImport.considered
							<< " buildings; roads " << roadReport.linksConsidered << " links, "
							<< roadReport.segmentsAccepted << "/" << roadReport.segmentsQueued
							<< " segments, " << roadReport.buildingsConnected << " connected, "
							<< roadReport.buildingsUnconnected << " unconnected.";
						regionEconomyDescription = fixtureStatus.str();
						wilog("[OpenRepublic] Synthetic region import detail: %s %s",
							compat::DescribeRegionImport(regionEconomyImport).c_str(),
							compat::DescribeRegionRoads(roadReport).c_str());
						RefreshRegionEconomy();
						UpdateRegionEconomyPresentation();
					}
					else
					{
						regionEconomyApi.reset();
						regionEconomyWorld.reset();
						uiNotice = "Synthetic fixture EconomyApi could not read the created world.";
					}
				}
				else
				{
					regionEconomyWorld.reset();
					uiNotice = "Synthetic fixture road import failed.";
				}
			}
			else
			{
				regionEconomyWorld.reset();
				uiNotice = "Synthetic fixture economy import failed: " + compat::DescribeRegionImport(regionEconomyImport);
			}
			if (!regionEconomyApi)
			{
				regionWorldStatus = uiNotice;
				wilog("[OpenRepublic] %s", uiNotice.c_str());
			}
		}

		if (mapShowcaseCaptureMode && mapRegionPreview && nmfPreviewLoaded)
		{
			const double effectiveRadius = mapRadiusMeters > 0.0 ? mapRadiusMeters : 400.0;
			const float distance = static_cast<float>(effectiveRadius * mapShowcaseCameraDistanceScale);
			renderPath = std::make_unique<MapShowcaseCaptureRenderPath>(mapShowcaseCapturePath,
				nmfPreviewCameraTarget, distance, mapShowcaseCameraYawDegrees, mapShowcaseCameraElevationDegrees);
			wilog("[OpenRepublic] Map showcase preset: %s",
				mapShowcaseEnhancedLook ? "enhanced" : "baseline");
		}
		else if (capturePreview && nmfPreviewLoaded)
		{
			if (nmfUnlitPreview || nmfMaterialUnlitPreview)
			{
				// Use Wicked's existing global diagnostic to bypass lighting without
				// selecting a separate material shader permutation. The material-unlit
				// mode retains MTL colors, UVs, and optional local texture candidates.
				wi::renderer::SetForceUnlit(true);
			}
			renderPath = std::make_unique<NmfPreviewCaptureRenderPath>(std::move(nmfCapturePaths),
			nmfPreviewCameraTarget, nmfPreviewCameraDistance, nmfCaptureZoom,
			nmfCaptureElevationDegrees);
		}
		else
		{
			renderPath = std::make_unique<wi::RenderPath3D>();
			if (capturePreview)
			{
				if (mapShowcaseCaptureMode)
				{
					wilog("[OpenRepublic] Map showcase capture failed because the requested region preview did not load");
					PostQuitMessage(2);
				}
				else
				{
					wilog("[OpenRepublic] Preview capture skipped because no supported model/region was loaded");
					PostQuitMessage(2);
				}
			}
		}
		renderPath->scene = &previewWorld;
		renderPath->setBloomEnabled(false);
		renderPath->setDepthOfFieldEnabled(false);
		if (mapShowcaseEnhancedLook && mapRegionPreview)
		{
			// Showcase defaults; an explicit --graphics string overrides them below.
			const bool rayTracing = wi::graphics::GetDevice() != nullptr &&
				wi::graphics::GetDevice()->CheckCapability(wi::graphics::GraphicsDeviceCapability::RAYTRACING);
			renderPath->setBloomEnabled(true);
			renderPath->setBloomThreshold(1.6f);
			renderPath->setAO(rayTracing ? wi::RenderPath3D::AO_RTAO : wi::RenderPath3D::AO_HBAO);
			renderPath->setSSREnabled(true);
			renderPath->setRaytracedReflectionsEnabled(rayTracing);
			renderPath->setFXAAEnabled(true);
			renderPath->setSharpenFilterEnabled(true);
			renderPath->setSharpenFilterAmount(0.25f);
			// Auto exposure by sun height: low golden sun needs lift, a high noon sun clips the grass.
			renderPath->setEyeAdaptionEnabled(false);
			const float sunElevation = std::isfinite(mapShowcaseSunElevationDegrees) ? mapShowcaseSunElevationDegrees : 20.0f;
			renderPath->setExposure(std::clamp(1.7f - 0.95f * (sunElevation - 10.0f) / 45.0f, 0.75f, 1.8f));
			wilog("[OpenRepublic] Showcase post effects: exposure %.2f (sun elevation %.1f deg)", renderPath->getExposure(), sunElevation);
			renderPath->setTonemap(wi::renderer::Tonemap::ACES);
			renderPath->setLightShaftsEnabled(true);
			renderPath->setLightShaftsStrength(0.35f);
		}
		if (graphicsSettings)
		{
			ApplyGraphicsSettings(*renderPath, *graphicsSettings);
			const bool temporal = graphicsSettings->upscaler != openrepublic::runtime::Upscaler::None ||
				graphicsSettings->rayTracedReflections || graphicsSettings->rayTracedDiffuse ||
				graphicsSettings->ambientOcclusion == openrepublic::runtime::AmbientOcclusion::RayTraced;
			if (auto* capturePath = dynamic_cast<NmfPreviewCaptureRenderPath*>(renderPath.get()); capturePath && temporal)
			{
				capturePath->SetSettleFrames(48);
			}
		}

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
			if (agentApi.Step() != openrepublic::agent_api::ErrorCode::None)
			{
				uiNotice = "The Agent API rejected the fixed-tick step.";
				UpdateStatusLabel();
				return;
			}
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

		selectNextStorageButton.Create("SelectNextStorageButton");
		selectNextStorageButton.SetText("Next storage");
		StyleButton(selectNextStorageButton);
		selectNextStorageButton.SetSize(XMFLOAT2(130, 28));
		selectNextStorageButton.SetPos(XMFLOAT2(650, 90));
		selectNextStorageButton.OnClick([this](const wi::gui::EventArgs&) { SelectNextStorage(); });
		gui.AddWidget(&selectNextStorageButton);
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

		if (!capturePreview)
		{
			wi::scene::TransformComponent cameraTransform;
			if (mapRegionPreview && nmfPreviewLoaded)
			{
				// Metre-scale region: frame it from above and scale fly speed with its size.
				if (mapShowcaseEnhancedLook)
				{
					// Modern look: start at the hero pose (yaw -110, 16 degrees up, 0.9 of the region distance).
					constexpr float yaw = -110.0f * XM_PI / 180.0f;
					constexpr float elevation = 16.0f * XM_PI / 180.0f;
					const XMFLOAT3 direction(std::sin(yaw) * std::cos(elevation), std::sin(elevation), std::cos(yaw) * std::cos(elevation));
					const float distance = nmfPreviewCameraDistance * 0.9f;
					cameraTransform.Translate(XMFLOAT3(nmfPreviewCameraTarget.x + distance * direction.x,
						nmfPreviewCameraTarget.y + distance * direction.y, nmfPreviewCameraTarget.z + distance * direction.z));
					cameraTransform.RotateRollPitchYaw(XMFLOAT3(elevation, std::atan2(-direction.x, -direction.z), 0.0f));
				}
				else
				{
				cameraTransform.Translate(XMFLOAT3(nmfPreviewCameraTarget.x,
					nmfPreviewCameraTarget.y + nmfPreviewCameraDistance * 0.55f,
					nmfPreviewCameraTarget.z - nmfPreviewCameraDistance * 0.85f));
				cameraTransform.RotateRollPitchYaw(XMFLOAT3(0.6f, 0, 0));
				}
				cameraSpeedScale = std::max(1.0f, nmfPreviewCameraDistance / 15.0f);
			}
			else
			{
				cameraTransform.Translate(terrainPreviewLoaded ? XMFLOAT3(0, 18.0f, -27.0f) :
					(nmfPreviewLoaded ? XMFLOAT3(8.0f, 8.0f, -17.0f) : XMFLOAT3(0, 6.5f, -11.5f)));
				cameraTransform.RotateRollPitchYaw(XMFLOAT3(terrainPreviewLoaded ? 0.52f : (nmfPreviewLoaded ? 0.38f : 0.43f), 0, 0));
			}
			cameraTransform.UpdateTransform();
			renderPath->camera->TransformCamera(cameraTransform);
			if (mapShowcaseEnhancedLook && mapRegionPreview)
			{
				renderPath->camera->zNearP = 0.5f;
				renderPath->camera->zFarP = 40000.0f;
			}
			if (nativeEconomyCheckpointMode)
			{
				renderPath->camera->zNearP = std::max(0.01f, nmfPreviewCameraDistance / 100000.0f);
				renderPath->camera->zFarP = std::max(1000.0f, nmfPreviewCameraDistance * 4.0f);
			}
			renderPath->camera->UpdateCamera();
		}

		if (mapRegionPreview && nmfPreviewLoaded)
		{
			// The imported region is its own simulation presentation. Keep only controls
			// that operate its EconomyApi (in-process by default, or an explicit bridge).
			for (wi::gui::Widget* widget : { static_cast<wi::gui::Widget*>(&pauseButton), static_cast<wi::gui::Widget*>(&stepButton),
				static_cast<wi::gui::Widget*>(&roadButton), static_cast<wi::gui::Widget*>(&buildRoadButton),
				static_cast<wi::gui::Widget*>(&placeStorageButton), static_cast<wi::gui::Widget*>(&buildFarmButton),
				static_cast<wi::gui::Widget*>(&buildMillButton), static_cast<wi::gui::Widget*>(&cancelPlacementButton),
				static_cast<wi::gui::Widget*>(&setTruckFarmButton), static_cast<wi::gui::Widget*>(&setTruckMillButton),
				static_cast<wi::gui::Widget*>(&setTruckStorageButton), static_cast<wi::gui::Widget*>(&queueTruckPlanButton),
				static_cast<wi::gui::Widget*>(&selectNextStorageButton), static_cast<wi::gui::Widget*>(&saveButton),
				static_cast<wi::gui::Widget*>(&loadButton), static_cast<wi::gui::Widget*>(&reloadRulesButton) })
			{
				gui.RemoveWidget(widget);
			}
			if (economyOverlay || regionEconomyApi)
			{
				regionStepOneButton.Create("RegionEconomyStepOneTickButton");
				regionStepOneButton.SetText("Step 1 tick");
				StyleButton(regionStepOneButton);
				regionStepOneButton.SetSize(XMFLOAT2(132, 28));
				regionStepOneButton.SetPos(XMFLOAT2(18, 18));
				regionStepOneButton.OnClick([this](const wi::gui::EventArgs&) { if (nativeEconomyCheckpointMode) { nativeEconomyRunning = false; nativeEconomyRunClock.Reset(); nativeRunPauseButton.SetText("Run"); } QueueRegionStep(1); });
				gui.AddWidget(&regionStepOneButton);

				regionStepButton.Create("RegionEconomyStepButton");
				regionStepButton.SetText("Step 24 ticks");
				StyleButton(regionStepButton);
				regionStepButton.SetSize(XMFLOAT2(132, 28));
				regionStepButton.SetPos(XMFLOAT2(156, 18));
				regionStepButton.OnClick([this](const wi::gui::EventArgs&) { if (nativeEconomyCheckpointMode) { nativeEconomyRunning = false; nativeEconomyRunClock.Reset(); nativeRunPauseButton.SetText("Run"); } QueueRegionStep(24); });
				gui.AddWidget(&regionStepButton);

				regionToggleButton.Create("RegionEconomyToggleBuildingButton");
				regionToggleButton.SetText("Toggle selected");
				StyleButton(regionToggleButton);
				regionToggleButton.SetSize(XMFLOAT2(132, 28));
				regionToggleButton.SetPos(XMFLOAT2(294, 18));
				regionToggleButton.OnClick([this](const wi::gui::EventArgs&) { QueueRegionBuildingToggle(); });
				gui.AddWidget(&regionToggleButton);

				regionHaulSourceButton.Create("RegionEconomyHaulSourceButton");
				regionHaulSourceButton.SetText("Set haul source");
				StyleButton(regionHaulSourceButton);
				regionHaulSourceButton.SetSize(XMFLOAT2(132, 28));
				regionHaulSourceButton.SetPos(XMFLOAT2(432, 18));
				regionHaulSourceButton.OnClick([this](const wi::gui::EventArgs&) { SetRegionHaulSource(); });
				gui.AddWidget(&regionHaulSourceButton);

				regionHaulButton.Create("RegionEconomyHaulToSelectedButton");
				regionHaulButton.SetText("Haul to selected");
				StyleButton(regionHaulButton);
				regionHaulButton.SetSize(XMFLOAT2(132, 28));
				regionHaulButton.SetPos(XMFLOAT2(570, 18));
				regionHaulButton.OnClick([this](const wi::gui::EventArgs&) { QueueRegionHaul(); });
				gui.AddWidget(&regionHaulButton);
				if (nativeEconomyCheckpointMode)
				{
					nativeFrameButton.Create("NativeEconomyFrameWorldButton");
					nativeFrameButton.SetText("Frame world");
					StyleButton(nativeFrameButton);
					nativeFrameButton.SetSize(XMFLOAT2(132, 28));
					nativeFrameButton.SetPos(XMFLOAT2(708, 18));
					nativeFrameButton.OnClick([this](const wi::gui::EventArgs&) { FrameNativeEconomyWorld(); });
					gui.AddWidget(&nativeFrameButton);
					nativeCameraFollowButton.Create("NativeEconomyCameraFollowButton");
					nativeCameraFollowButton.SetText("Follow truck: off");
					StyleButton(nativeCameraFollowButton);
					nativeCameraFollowButton.SetSize(XMFLOAT2(132, 28));
					nativeCameraFollowButton.OnClick([this](const wi::gui::EventArgs&) {
						nativeCameraFollow = !nativeCameraFollow;
						nativeCameraFollowButton.SetText(nativeCameraFollow ? "Follow truck: on" : "Follow truck: off");
						UpdateRegionStatus();
					});
					gui.AddWidget(&nativeCameraFollowButton);
					nativeStepManyButton.Create("NativeEconomyStepManyButton");
					nativeStepManyButton.SetText("Step 240 ticks");
					StyleButton(nativeStepManyButton);
					nativeStepManyButton.SetSize(XMFLOAT2(132, 28));
					nativeStepManyButton.OnClick([this](const wi::gui::EventArgs&) { nativeEconomyRunning = false; nativeEconomyRunClock.Reset(); nativeRunPauseButton.SetText("Run"); QueueRegionStep(240); });
					gui.AddWidget(&nativeStepManyButton);
					nativeRunPauseButton.Create("NativeEconomyRunPauseButton");
					nativeRunPauseButton.SetText("Run");
					StyleButton(nativeRunPauseButton);
					nativeRunPauseButton.SetSize(XMFLOAT2(132, 28));
					nativeRunPauseButton.OnClick([this](const wi::gui::EventArgs&) { ToggleNativeRunPause(); });
					gui.AddWidget(&nativeRunPauseButton);
					nativeDetailsButton.Create("NativeEconomyDetailsButton");
					nativeDetailsButton.SetText("Details");
					StyleButton(nativeDetailsButton);
					nativeDetailsButton.SetSize(XMFLOAT2(132, 28));
					nativeDetailsButton.OnClick([this](const wi::gui::EventArgs&) {
						nativeDetailsExpanded = !nativeDetailsExpanded;
						nativeDetailsButton.SetText(nativeDetailsExpanded ? "Compact" : "Details");
						UpdateRegionStatus();
					});
					gui.AddWidget(&nativeDetailsButton);
					nativeOverviewButton.Create("NativeEconomyOverviewButton");
					nativeOverviewButton.SetText("Overview");
					nativeOverviewButton.SetTooltip("Show economy totals and recent stock changes.");
					StyleButton(nativeOverviewButton);
					nativeOverviewButton.SetSize(XMFLOAT2(132, 28));
					nativeOverviewButton.OnClick([this](const wi::gui::EventArgs&) {
						nativeOverviewExpanded = !nativeOverviewExpanded;
						nativeOverviewButton.SetText(nativeOverviewExpanded ? "Hide overview" : "Overview");
						UpdateRegionStatus();
					});
					gui.AddWidget(&nativeOverviewButton);
					nativeSaveButton.Create("NativeEconomySaveCheckpointButton");
					nativeSaveButton.SetText("Save game slot");
					StyleButton(nativeSaveButton);
					nativeSaveButton.SetSize(XMFLOAT2(132, 28));
					nativeSaveButton.SetTooltip("Pause and save the native game in the user-data save directory.");
					nativeSaveButton.OnClick([this](const wi::gui::EventArgs&) { SaveNativeCheckpoint(); });
					if (nativeObjectiveSet)
					{
						nativeObjectiveRestartButton.Create("NativeObjectivesRestartButton");
						nativeObjectiveRestartButton.SetText("Restart objectives");
						StyleButton(nativeObjectiveRestartButton);
						nativeObjectiveRestartButton.SetSize(XMFLOAT2(132, 28));
						nativeObjectiveRestartButton.SetTooltip("Restore the authored starting world and restart this objective set.");
						nativeObjectiveRestartButton.OnClick([this](const wi::gui::EventArgs&) { RestartNativeObjectives(); });
						gui.AddWidget(&nativeObjectiveRestartButton);
						nativeObjectiveLabel.Create("NativeObjectivesChecklist");
						nativeObjectiveLabel.SetSize(XMFLOAT2(540, 142));
						nativeObjectiveLabel.SetText(nativeObjectiveSet->title);
						gui.AddWidget(&nativeObjectiveLabel);
					}
					gui.AddWidget(&nativeSaveButton); nativeSaveButton.SetVisible(false);
					nativeQuitButton.Create("NativeEconomyQuitButton");
					nativeQuitButton.SetText("Quit game"); StyleButton(nativeQuitButton); nativeQuitButton.SetSize(XMFLOAT2(132, 28));
					nativeQuitButton.SetTooltip("Close the current game. Unsaved changes prompt first.");
					nativeQuitButton.OnClick([this](const wi::gui::EventArgs&) { PostMessageW(window, WM_CLOSE, 0, 0); });
					gui.AddWidget(&nativeQuitButton); nativeQuitButton.SetVisible(false);
					nativeGameMenuButton.Create("NativeEconomyGameMenuButton");
					nativeGameMenuButton.SetText("Game");
					StyleButton(nativeGameMenuButton);
					nativeGameMenuButton.SetSize(XMFLOAT2(132, 28));
					nativeGameMenuButton.OnClick([this](const wi::gui::EventArgs&) {
						nativeGameMenuExpanded = !nativeGameMenuExpanded;
						if (nativeGameMenuExpanded) RefreshNativeSaveSlots();
						UpdateRegionStatus();
					});
					gui.AddWidget(&nativeGameMenuButton);
					const std::array<const char*, 3> newGameLabels{{"New sandbox", "New coal demo", "New construction game"}};
					const std::array<const wchar_t*, 3> newGameArguments{{L"--native-economy", L"--native-economy --native-coal-demo", L"--native-economy --native-construction-sites"}};
					for (std::size_t i = 0; i < nativeNewGameButtons.size(); ++i)
					{
						auto& button = nativeNewGameButtons[i];
						button.Create("NativeEconomyNewGame" + std::to_string(i));
						button.SetText(newGameLabels[i]); StyleButton(button); button.SetSize(XMFLOAT2(190, 28));
						button.OnClick([this, args = std::wstring(newGameArguments[i])](const wi::gui::EventArgs&) { StartNativeNewGame(args.c_str()); });
						gui.AddWidget(&button); button.SetVisible(false);
					}
					for (std::size_t i = 0; i < nativeSaveSlotButtons.size(); ++i)
					{
						auto& button = nativeSaveSlotButtons[i];
						button.Create("NativeEconomySaveSlot" + std::to_string(i));
						StyleButton(button); button.SetSize(XMFLOAT2(500, 28));
						button.OnClick([this, i](const wi::gui::EventArgs&) { LoadNativeSaveSlot(i); });
						gui.AddWidget(&button); button.SetVisible(false);
					}
					for (std::size_t i = 0; i < nativeSavePageButtons.size(); ++i)
					{
						auto& button = nativeSavePageButtons[i];
						button.Create("NativeEconomySavePage" + std::to_string(i));
						button.SetText(i == 0 ? "Previous saves" : "More saves"); StyleButton(button); button.SetSize(XMFLOAT2(132, 28));
						button.OnClick([this, i](const wi::gui::EventArgs&) {
							const std::size_t pageSize = nativeSaveSlotButtons.size();
							if (i == 0) nativeSaveSlotOffset = nativeSaveSlotOffset >= pageSize ? nativeSaveSlotOffset - pageSize : 0;
							else if (nativeSaveSlotOffset + pageSize < nativeSaveSlots.size()) nativeSaveSlotOffset += pageSize;
							RefreshNativeSaveSlots(); UpdateRegionStatus();
						});
						gui.AddWidget(&button); button.SetVisible(false);
					}
				}
				if (nativeEconomyCheckpointMode)
				{
					nativeEconomyTools.Create(gui, nativeEconomyCheckpoint.world->GetCatalog(), {
						[this] { BeginNativeBuildingPlacement(); },
						[this] { BeginNativeRoadPlacement(); },
						[this] { CancelNativeConstruction(); },
						[this] { QueueNativeRemoval(); },
						[this] { QueueNativeOrderChange(true); },
						[this] { QueueNativeOrderChange(false); },
						[this] { QueueNativeRoadToggle(); },
						[this] { QueueNativeUtilityConnection(true); },
						[this] { QueueNativeUtilityConnection(false); },
						[this] { UpdateRegionStatus(); },
						[this] { QueueNativeDispatch(); }, [this] { QueueNativeCoalImport(); },
						[this] { nativeCoalAutoRun = !nativeCoalAutoRun; UpdateRegionStatus(); } });
					nativeToolsReady = true;
					RefreshNativeHaulChoices();
					nativeEconomyTools.SetAutoRun(nativeCoalAutoRun);
					nativeEconomyTools.SetCoalDemoControls(nativeCoalDemo);
				}
				else if (regionEconomyWorld && regionEconomyApi)
				{
					nativeDetailsButton.Create("RegionEconomyDetailsButton");
					nativeDetailsButton.SetText("Details");
					StyleButton(nativeDetailsButton);
					nativeDetailsButton.SetSize(XMFLOAT2(132, 28));
					nativeDetailsButton.OnClick([this](const wi::gui::EventArgs&) {
						nativeDetailsExpanded = !nativeDetailsExpanded;
						nativeDetailsButton.SetText(nativeDetailsExpanded ? "Compact" : "Details");
						UpdateRegionStatus();
					});
					gui.AddWidget(&nativeDetailsButton);
					nativeOverviewButton.Create("RegionEconomyOverviewButton");
					nativeOverviewButton.SetText("Overview");
					nativeOverviewButton.SetTooltip("Show economy totals and recent stock changes.");
					StyleButton(nativeOverviewButton);
					nativeOverviewButton.SetSize(XMFLOAT2(132, 28));
					nativeOverviewButton.OnClick([this](const wi::gui::EventArgs&) {
						nativeOverviewExpanded = !nativeOverviewExpanded;
						nativeOverviewButton.SetText(nativeOverviewExpanded ? "Hide overview" : "Overview");
						UpdateRegionStatus();
					});
					gui.AddWidget(&nativeOverviewButton);
					nativeEconomyTools.Create(gui, regionEconomyWorld->GetCatalog(), {
						{}, {}, {}, {},
						[this] { QueueNativeOrderChange(true); },
						[this] { QueueNativeOrderChange(false); }, {},
						[this] { QueueNativeUtilityConnection(true); },
						[this] { QueueNativeUtilityConnection(false); },
						[this] { UpdateRegionStatus(); },
						[this] { QueueNativeDispatch(); } }, false,
						regionCatalogImportOptions.syntheticRegionFleetCount > 0);
					nativeToolsReady = true;
					RefreshNativeHaulChoices();
				}
			}
			simulationStatus.SetPos(XMFLOAT2(18, 54));
			simulationStatus.scrollbar.SetEnabled(true);
			simulationStatus.SetSize(XMFLOAT2(864, 96));
			regionWorldStatus = economyOverlay
				? "Economy bridge connected; waiting for the first world snapshot..."
				: (regionEconomyApi ? regionWorldStatus : uiNotice);
			if (economyOverlay || regionEconomyApi) UpdateRegionStatus();
			else simulationStatus.SetText(regionWorldStatus);
		}

		ActivatePath(renderPath.get());
		if (!capturePreview && !mapRegionPreview)
		{
			UpdateSimulationPresentation();
		}
	}

	void OpenRepublicApplication::Update(float dt)
	{
		// Capture pre-update input ownership: a popup dismissal must not also click
		// through to construction or consume Escape in the world below it.
		const bool nativeInputWasActive = nativeToolsReady &&
			(nativeEconomyTools.HasActiveInput() || (renderPath && renderPath->GetGUI().IsTyping()));
		wi::Application::Update(dt);
		if (nmfCaptureMode)
		{
			return;
		}
		if (mapShowcaseCaptureMode)
		{
			if (mapRegionPreview && mapRegionDynamicLod && renderPath != nullptr && renderPath->camera != nullptr)
				buildingLodController.Update(previewWorld, renderPath->camera->Eye);
			// Matched capture telemetry: ignore the capture path's >=12 s warmup, then
			// time the following 48 update frames (the same settle window before readback).
			static const auto captureStart = std::chrono::steady_clock::now();
			static double captureSampleSeconds = 0.0;
			static float captureWorstFrame = 0.0f;
			static std::uint32_t captureSampleFrames = 0;
			if (std::chrono::steady_clock::now() - captureStart >= std::chrono::seconds(12) && captureSampleFrames < 48)
			{
				captureSampleSeconds += std::max(0.0f, dt);
				captureWorstFrame = std::max(captureWorstFrame, dt);
				++captureSampleFrames;
				if (captureSampleFrames == 48)
				{
					wilog("[OpenRepublic] Map LOD capture frame sample: %u frames, %.2f ms average, %.2f ms worst (%.1f fps from update dt)",
						captureSampleFrames, captureSampleSeconds * 1000.0 / captureSampleFrames,
						captureWorstFrame * 1000.0f, captureSampleSeconds > 0.0 ? captureSampleFrames / captureSampleSeconds : 0.0);
				}
			}
			return;
		}
		UpdateSceneControl(dt);
		UpdateNativeCoalAutomation();
		UpdateNativeAutosave();
		if (mapRegionPreview && mapRegionDynamicLod && renderPath != nullptr && renderPath->camera != nullptr)
		{
			buildingLodController.Update(previewWorld, renderPath->camera->Eye);
		}
		if (mapShowcaseEnhancedLook && mapRegionPreview)
		{
			// Frame-time telemetry for the modern look (logged every ~5 s of wall time).
			static double accumulated = 0.0;
			static std::uint32_t frames = 0;
			static float worst = 0.0f;
			// Cold starts (shader compilation, texture streaming) can leave the ocean half-initialised and
			// flood the town; keep it off for the first seconds of the interactive view.
			static std::uint32_t totalFrames = 0;
			++totalFrames;
			if (!mapShowcaseCaptureMode && showcaseEnvironment.hasWater && (totalFrames == 2 || totalFrames == 300))
			{
				for (std::size_t i = 0; i < previewWorld.weathers.GetCount(); ++i)
				{
					previewWorld.weathers[i].SetOceanEnabled(totalFrames == 300);
				}
			}
			accumulated += dt;
			++frames;
			worst = std::max(worst, dt);
			if (accumulated >= 5.0)
			{
				wilog("[OpenRepublic] Modern view: %.1f fps average, worst frame %.1f ms over %u frames", frames / accumulated, worst * 1000.0f, frames);
				accumulated = 0.0;
				frames = 0;
				worst = 0.0f;
			}
		}
		PumpLocalEconomyRequests();
		if (nativeEconomyCheckpointMode && nativeEconomyRunning)
		{
			if (!nativeEconomyPresentationValid || !regionEconomyApi)
			{
				nativeEconomyRunning = false;
				nativeEconomyRunClock.Reset();
				nativeRunPauseButton.SetText("Run");
				uiNotice = "Run paused because the native scene or EconomyApi is unavailable; Frame world before continuing.";
			}
			else if (nativeEconomyRunClock.Advance(static_cast<double>(dt)) != 0)
			{
				const bool stepSucceeded = QueueRegionStep(nativeEconomySpeed, true);
				if (!stepSucceeded || !nativeEconomyPresentationValid)
				{
					nativeEconomyRunning = false;
					nativeEconomyRunClock.Reset();
					nativeRunPauseButton.SetText("Run");
					if (!nativeEconomyPresentationValid) uiNotice = "Run paused because the native scene is stale; Frame world before continuing.";
					UpdateRegionStatus();
				}
			}
		}
		if (economyOverlay && mapPlacement)
		{
			const double effectiveMapRadius = mapRadiusMeters > 0.0 ? mapRadiusMeters : 400.0;
			const std::string status = economyOverlay->Update(previewWorld, regionInstances, *mapPlacement, effectiveMapRadius);
			if (!status.empty())
			{
				regionWorldStatus = status;
				if (selectedRegionBuildingEntity != wi::ecs::INVALID_ENTITY &&
					!economyOverlay->GetBuildingForInstance(selectedRegionBuildingEntity).has_value())
				{
					selectedRegionBuildingEntity = wi::ecs::INVALID_ENTITY;
					selectedRegionBuildingId.reset();
				}
			}
			UpdateRegionStatus();
		}
		else if (regionEconomyApi && (mapPlacement || syntheticRegionFixtureMode || nativeEconomyCheckpointMode))
		{
			UpdateRegionEconomyPresentation();
			if (nativeEconomyCheckpointMode && nativeEconomyPresentationValid)
				nativeEconomyScene.UpdateVehicleMotion(previewWorld,
					nativeEconomyRunning ? nativeEconomyRunClock.InterpolationAlpha() : 1.0);
			else if (mapPlacement && regionVehicleTransform &&
				regionCatalogImportOptions.syntheticRegionFleetCount > 0)
			{
				const auto terrainTransform = openrepublic::content::MakeMapTerrainTransform(mapPlacement->metadata);
				nativeEconomyScene.UpdateVehicleMotion(previewWorld, 1.0,
					[this, terrainTransform](double localX, double localZ) {
						double height = 0.0;
						const double worldX = localX + mapPlacement->placement.X();
						const double worldZ = localZ + mapPlacement->placement.Z();
						openrepublic::content::SampleTerrainHeight(mapPlacement->heightmap, terrainTransform,
							std::clamp(worldX, terrainTransform.originX, terrainTransform.originX + terrainTransform.sizeX),
							std::clamp(worldZ, terrainTransform.originZ, terrainTransform.originZ + terrainTransform.sizeZ),
							height, nullptr);
						return height;
					});
			}
			if (nativeEconomyCheckpointMode && nativeEconomyPresentationValid && nativeCameraFollow &&
				regionEconomySnapshot && renderPath && renderPath->camera)
			{
				const auto vehicleId = nativeEconomyTools.SelectedDispatchVehicle();
				const auto vehicle = std::find_if(regionEconomySnapshot->vehicles.begin(), regionEconomySnapshot->vehicles.end(),
					[&](const auto& item) { return item.id == vehicleId; });
				if (vehicle != regionEconomySnapshot->vehicles.end())
				{
					const double alpha = nativeEconomyRunning ? nativeEconomyRunClock.InterpolationAlpha() : 1.0;
					const auto target = nativeEconomyScene.VehiclePosition(vehicleId, alpha)
						.value_or(nativeEconomyScene.CellPosition(vehicle->cell));
					const float distance = nativeEconomyScene.CameraDistance() * (nativeCoalDemo ? 0.68f : 1.0f);
					wi::scene::TransformComponent cameraTransform;
					cameraTransform.Translate(XMFLOAT3(target.x, target.y + distance * 0.55f, target.z - distance * 0.85f));
					cameraTransform.RotateRollPitchYaw(XMFLOAT3(0.6f, 0, 0));
					cameraTransform.UpdateTransform();
					renderPath->camera->TransformCamera(cameraTransform);
					renderPath->camera->UpdateCamera();
				}
			}
			if (selectedRegionBuildingEntity != wi::ecs::INVALID_ENTITY &&
				!GetRegionBuildingForInstance(selectedRegionBuildingEntity).has_value())
			{
				selectedRegionBuildingEntity = wi::ecs::INVALID_ENTITY;
				selectedRegionBuildingId.reset();
			}
			UpdateRegionStatus();
		}

		if (mapRegionPreview)
		{
			// Region mode is driven by its EconomyApi; do not also advance the
			// unrelated synthetic src/sim demo hidden behind the imported world.
		}
		else if (simulationPaused)
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
				if (agentApi.Step() != openrepublic::agent_api::ErrorCode::None)
				{
					uiNotice = "The Agent API rejected the fixed-tick step.";
					simulationAccumulator = 0.0f;
					UpdateStatusLabel();
					break;
				}
				simulationAccumulator -= kSimulationTickSeconds;
				UpdateSimulationPresentation();
			}
		}

		if (!is_window_active || renderPath == nullptr || renderPath->camera == nullptr)
		{
			return;
		}

		const XMFLOAT4 pointer = wi::input::GetPointer();
		const wi::primitive::Hitbox2D pointerHitbox(XMFLOAT2(pointer.x, pointer.y), XMFLOAT2(1, 1));
		// GUI::HasFocus() includes passive labels. The status panel spans a large part
		// of the viewport, so let world input pass through it while preserving the
		// normal input guard for buttons and an interactive overflow scrollbar.
		const bool pointerOnStatusLabel = simulationStatus.IsVisible() &&
			pointerHitbox.intersects(simulationStatus.hitBox);
		const bool statusScrollActive = simulationStatus.IsVisible() &&
			simulationStatus.scrollbar.IsEnabled() && simulationStatus.scrollbar.IsScrollbarRequired() &&
			(pointerOnStatusLabel || simulationStatus.scrollbar.scrollbar_state ==
				wi::gui::ScrollBar::SCROLLBAR_GRABBED);
		const bool nativeControlsHaveFocus = nativeToolsReady && (nativeInputWasActive ||
			nativeEconomyTools.HasActiveInput() || renderPath->GetGUI().IsTyping() ||
			nativeEconomyTools.PointerOnControls(XMFLOAT2(pointer.x, pointer.y)));
		const bool guiHasFocus = nativeControlsHaveFocus || statusScrollActive ||
			(renderPath->GetGUI().HasFocus() && !pointerOnStatusLabel);
		if (nativeEconomyCheckpointMode) UpdateNativeConstructionPreview(!guiHasFocus);
		if (!guiHasFocus && wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
		{
			if (mapRegionPreview)
			{
				if (nativeConstructionMode != NativeConstructionMode::None)
				{
					CancelNativeConstruction();
					return;
				}
				selectedRegionBuildingEntity = wi::ecs::INVALID_ENTITY;
				selectedRegionBuildingId.reset();
				regionHaulSourceId.reset();
				UpdateRegionStatus();
			}
			else if (activeRoadPlacement) CancelRoadPlacement();
			else if (activeFacilityPlacement.has_value() || activeStoragePlacement) CancelFacilityPlacement();
			else SelectBuilding(wi::ecs::INVALID_ENTITY);
		}
		if (nativeEconomyCheckpointMode && !guiHasFocus) {
			if (wi::input::Press(static_cast<wi::input::BUTTON>('B'))) BeginNativeBuildingPlacement();
			else if (wi::input::Press(static_cast<wi::input::BUTTON>('R'))) BeginNativeRoadPlacement();
			else if (wi::input::Press(static_cast<wi::input::BUTTON>('X'))) { CancelNativeConstruction(); nativeConstructionMode = NativeConstructionMode::Remove; uiNotice = "Bulldoze: click a building under the pointer. Road removal is not yet supported by the economy API."; UpdateRegionStatus(); }
			else if (wi::input::Press(wi::input::KEYBOARD_BUTTON_SPACE)) ToggleNativeRunPause();
			else if (wi::input::Press(static_cast<wi::input::BUTTON>('1'))) nativeEconomySpeed = 1;
			else if (wi::input::Press(static_cast<wi::input::BUTTON>('2'))) nativeEconomySpeed = 2;
			else if (wi::input::Press(static_cast<wi::input::BUTTON>('3'))) nativeEconomySpeed = 3;
			else if (wi::input::Press(static_cast<wi::input::BUTTON>('4'))) nativeEconomySpeed = 4;
		}
		if (!guiHasFocus && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
		{
			if (nativeEconomyCheckpointMode && nativeConstructionMode == NativeConstructionMode::Remove) {
				const auto cell = NativeCellAtPointer();
				if (cell && regionEconomySnapshot) {
					const auto nativeSnapshot = nativeEconomyCheckpoint.world->Snapshot();
					const auto it = std::find_if(nativeSnapshot.buildings.begin(), nativeSnapshot.buildings.end(), [&](const auto& b) {
						const auto* type = nativeEconomyCheckpoint.catalog->BuildingType(b.type);
						return type && cell->x >= b.origin.x && cell->z >= b.origin.z && cell->x < b.origin.x + static_cast<std::int32_t>(type->width) && cell->z < b.origin.z + static_cast<std::int32_t>(type->depth);
					});
					if (it != nativeSnapshot.buildings.end()) {
						TrackNativeSubmission(regionEconomyApi->RemoveBuilding(regionEconomySnapshot->revision, it->id), "bulldoze building");
						CancelNativeConstruction();
					}
				}
			} else PickAtPointer();
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
		if (isDownOrPressed(wi::input::KEYBOARD_BUTTON_SPACE) && !nativeEconomyCheckpointMode) movement = XMVectorAdd(movement, XMVectorSet(0, 1, 0, 0));
		if (isDownOrPressed(wi::input::KEYBOARD_BUTTON_LCONTROL)) movement = XMVectorSubtract(movement, XMVectorSet(0, 1, 0, 0));

		bool cameraMoved = false;
		if (XMVectorGetX(XMVector3LengthSq(movement)) > 0)
		{
			movement = XMVector3Normalize(movement);
			const XMVECTOR eye = XMLoadFloat3(&camera.Eye);
			// Keep movement in world-units per second and bound catch-up after a stalled frame.
			const float movementDelta = std::clamp(dt, 0.0f, kMaxCameraMoveDeltaSeconds);
			XMStoreFloat3(&camera.Eye, XMVectorAdd(eye, XMVectorScale(movement, kCameraMoveSpeed * cameraSpeedScale * movementDelta)));
			cameraMoved = true;
		}
		if (pointer.z != 0)
		{
			const XMVECTOR eye = XMLoadFloat3(&camera.Eye);
			XMStoreFloat3(&camera.Eye, XMVectorAdd(eye, XMVectorScale(forward, pointer.z * kWheelZoomSpeed * cameraSpeedScale)));
			cameraMoved = true;
		}
		if (cameraMoved)
		{
			camera.UpdateCamera();
		}
	}

	void OpenRepublicApplication::PumpLocalEconomyRequests()
	{
		if (!localEconomyPipeEndpoint || !localEconomyDispatcher)
		{
			return;
		}
		if (!localEconomyPipeEndpoint->IsRunning())
		{
			const std::string error = localEconomyPipeEndpoint->LastError();
			uiNotice = "Trusted-local controller endpoint stopped" +
				(error.empty() ? std::string(".") : ": " + error);
			wilog("[OpenRepublic] %s", uiNotice.c_str());
			localEconomyPipeEndpoint->Stop();
			localEconomyPipeEndpoint.reset();
			localEconomyDispatcher.reset();
			return;
		}

		bool dispatched = false;
		constexpr std::size_t MaxRequestsPerUpdate = 8;
		for (std::size_t handled = 0; handled < MaxRequestsPerUpdate; ++handled)
		{
			openrepublic::economy_host::LocalEconomyPipeEndpoint::Request request;
			if (!localEconomyPipeEndpoint->TryPop(request))
			{
				break;
			}

			try
			{
				dispatched = true;
				std::string response = localEconomyDispatcher->HandleLine(request.line);
				if (!localEconomyPipeEndpoint->Complete(request.token, std::move(response)))
				{
					wilog("[OpenRepublic] Local economy pipe response was rejected by its bounded transport.");
				}
			}
			catch (...)
			{
				// Closing the current connection is safer than allowing an unexpected
				// serialization exception to escape the app update loop.
				localEconomyPipeEndpoint->Complete(request.token, {});
				wilog("[OpenRepublic] Local economy pipe request dispatch failed unexpectedly.");
			}
		}
		if (dispatched && regionEconomyApi)
		{
			const auto latest = regionEconomyApi->QueryWorld();
			if (latest.error == economy_api::ErrorCode::None &&
				(!regionEconomySnapshot || latest.world.revision != regionEconomySnapshot->revision))
			{
				RefreshRegionEconomy();
			}
		}
	}

	void OpenRepublicApplication::StartLocalEconomyEndpoint()
	{
		if (localEconomyPipeName.empty() || !regionEconomyApi || localEconomyPipeEndpoint)
			return;
		localEconomyDispatcher = std::make_unique<economy_host::Dispatcher>(
			economy_host::Role::Controller, *regionEconomyApi, true, false);
		localEconomyDispatcher->SetRegionDescription(regionEconomyDescription);
		auto endpoint = std::make_unique<economy_host::LocalEconomyPipeEndpoint>(localEconomyPipeName);
		std::string error;
		if (!endpoint->Start(error))
		{
			uiNotice = "Trusted-local controller endpoint could not start: " + error;
			localEconomyDispatcher.reset();
		}
		else
		{
			localEconomyPipeEndpoint = std::move(endpoint);
			uiNotice = "Native economy ready; trusted-local controller endpoint is listening. Choose a building to place, or select haul endpoints.";
		}
		wilog("[OpenRepublic] %s", uiNotice.c_str());
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
		const auto snapshot = QuerySimulationSnapshot();
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

	std::string OpenRepublicApplication::SelectedUtilityStatus(bool expanded) const
	{
		const auto* world = ActiveOwnedEconomyWorld();
		if (!world || !regionEconomyApi || !selectedRegionBuildingId ||
			std::none_of(world->GetCatalog().Resources().begin(), world->GetCatalog().Resources().end(),
				[](const auto& resource) { return resource.utility; })) return {};
		const auto page = regionEconomyApi->QueryUtilityDiagnostics(world->Revision(), 0, 128,
			static_cast<economy::BuildingId>(*selectedRegionBuildingId));
		if (page.error != economy_api::ErrorCode::None) return "Utility measurements unavailable for this selection.";
		if (!page.available) return "Utility measurements unavailable; advance one tick to measure supply and demand.";
		if (page.networks.empty()) return {};
		const auto* chosenResource = world->GetCatalog().Resource(nativeEconomyTools.SelectedUtilityResource());
		const auto chosen = std::find_if(page.networks.begin(), page.networks.end(), [&](const auto& network) {
			return chosenResource && network.resource == chosenResource->name;
		});
		std::ostringstream text;
		const auto append = [&](const auto& network) {
			text << "Utility " << network.resource << " at tick " << page.tick
				<< ": generated " << network.generated << ", used " << network.consumed
				<< ", eligible demand " << network.requested << ", unused " << network.unused
				<< ", short recipes " << network.starvedRecipes;
			if (expanded) text << " (grid OR-" << network.component << ", " << network.memberCount << " buildings)";
		};
		if (expanded)
		{
			for (const auto& network : page.networks)
			{
				if (text.tellp() > 0) text << "\n";
				append(network);
			}
			if (page.hasMore) text << "\nMore measured networks are available through utility_diagnostics paging.";
		}
		else append(chosen != page.networks.end() ? *chosen : page.networks.front());
		return text.str();
	}

	void OpenRepublicApplication::UpdateRegionStatus()
	{
		if (!mapRegionPreview || (!economyOverlay && !regionEconomyApi))
		{
			return;
		}
		std::ostringstream text;
		if (nativeToolsReady)
		{
			LayoutNativeControls();
			RefreshNativeHaulChoices();
		}
		text << regionWorldStatus;
		if (nativeGameMenuExpanded)
		{
			text << "\nGame menu: new sandbox, coal demo, construction game; autosave every 10 simulation minutes (3 rotating slots).";
			text << "\nSelect a listed save slot to load it.";
			if (nativeSaveSlots.empty()) text << "\nNo valid save slots found in the user-data save directory.";
		}
		const auto* overviewWorld = ActiveOwnedEconomyWorld();
		std::optional<openrepublic::runtime::EconomyOverview> overview;
		if (nativeToolsReady && overviewWorld)
		{
			const auto current = overviewWorld->Snapshot();
			if (!nativeOverviewHistory.empty() && current.tick < nativeOverviewHistory.back().tick)
				nativeOverviewHistory.clear();
			if (nativeOverviewHistory.empty() || current.tick > nativeOverviewHistory.back().tick)
				nativeOverviewHistory.push_back(current);
			else if (current.tick == nativeOverviewHistory.back().tick)
				nativeOverviewHistory.back() = current;
			while (nativeOverviewHistory.size() > 21) nativeOverviewHistory.pop_front();
			const std::optional<economy::WorldSnapshot> baseline = nativeOverviewHistory.size() > 1
				? std::optional<economy::WorldSnapshot>(nativeOverviewHistory.front()) : std::nullopt;
			overview = openrepublic::runtime::BuildEconomyOverview(current, overviewWorld->GetCatalog(), baseline);
		}
		if (nativeEconomyCheckpointMode)
		{
			text << (nativeEconomyPresentationValid ? "\nNative buildings shown: " : "\nScene is stale; selection disabled. Frame world can rebase. Buildings shown: ")
				<< regionInstances.size();
			if (nativeConstructionMode == NativeConstructionMode::Building)
			{
				const auto* type = nativeEconomyCheckpoint.world->GetCatalog().BuildingType(nativePlacementType);
				text << "\nPlace " << (type ? type->name : "building") << ": click its minimum X/Z footprint cell; Esc cancels.";
			}
			else if (nativeConstructionMode == NativeConstructionMode::Road)
			{
				text << "\nRoad: choose two cells on one row or column (up to 64 segments); cost unavailable in this economy model. Esc cancels.";
				if (nativeRoadStart) {
					text << " Start: (" << nativeRoadStart->x << ", " << nativeRoadStart->z << ").";
					if (nativeHoverCell) text << " Preview length: " << std::abs(nativeHoverCell->x - nativeRoadStart->x) + std::abs(nativeHoverCell->z - nativeRoadStart->z) << " segments.";
				}
			}
			text << "\nB building | R road | X bulldoze | Esc cancel | Space pause/run | 1-4 speed (" << nativeEconomySpeed << " ticks/step)";
			const auto orders = nativeEconomyCheckpoint.world->Snapshot().orders;
			const auto chosen = nativeEconomyTools.SelectedOrder();
			const auto order = std::find_if(orders.begin(), orders.end(),
				[chosen](const auto& value) { return value.id == chosen; });
			if (order != orders.end())
			{
				const char* issue = "none";
				switch (order->failure) {
				case economy::HaulFailure::NoRoute: issue = "no route"; break;
				case economy::HaulFailure::SourceEmpty: issue = "source empty"; break;
				case economy::HaulFailure::DestinationFull: issue = "destination full"; break;
				case economy::HaulFailure::RoadDisabled: issue = "road disabled"; break;
				default: break;
				}
				text << "\nHaul #" << order->id << ": OR-" << order->source << " -> OR-" << order->destination
					<< "; truck #" << order->vehicle << "; cargo " << order->cargo << "; issue: " << issue
					<< "\nTruck cell: (" << order->vehicleCell.x << ", " << order->vehicleCell.z << "). Select a building to retarget.";
			}
			if (nativeHoverCell) text << "\nGround cell: (" << nativeHoverCell->x << ", " << nativeHoverCell->z << "). Preview is a draft; placement validates on step.";
		}
		if (selectedRegionBuildingEntity != wi::ecs::INVALID_ENTITY)
		{
			const auto selected = GetRegionBuildingForInstance(selectedRegionBuildingEntity);
			if (selected.has_value())
			{
				selectedRegionBuildingId = selected->id;
				text << "\nSelected: OR-" << selected->id << " " << selected->type
					<< " (" << (selected->enabled ? "enabled" : "disabled") << ")";
				// A synthetic construction site cannot produce yet: show its site state, not the stall text.
				const economy::BuildingSnapshot* siteSnapshot = nullptr;
				economy::WorldSnapshot siteWorld;
				if (nativeEconomyCheckpointMode)
				{
					siteWorld = nativeEconomyCheckpoint.world->Snapshot();
					const auto foundSite = std::find_if(siteWorld.buildings.begin(), siteWorld.buildings.end(),
						[&](const auto& b) { return b.id == selected->id; });
					if (foundSite != siteWorld.buildings.end() && foundSite->constructionSite && foundSite->constructionState < 2)
						siteSnapshot = &*foundSite;
				}
				if (siteSnapshot)
				{
					static const char* const kStatusStates[] = { "construction site: waiting for materials", "construction site: building" };
					text << "\nStatus: " << kStatusStates[std::min<std::size_t>(siteSnapshot->constructionState, 1)];
				}
				else
				{
					text << "\nStatus: " << selected->stallReason;
					if (!selected->stallExplanation.empty()) text << " — " << selected->stallExplanation;
				}
				if (!selected->inventory.empty())
				{
					const auto formatUnits = [this](std::uint64_t amount)
					{
						if (syntheticRegionFixtureMode || nativeEconomyCheckpointMode)
						{
							return std::to_string(amount);
						}
						std::string formatted = std::to_string(amount / 1000000);
						const std::uint64_t fraction = amount % 1000000;
						if (fraction != 0)
						{
							std::string decimal = std::to_string(fraction + 1000000).substr(1);
							while (!decimal.empty() && decimal.back() == '0') decimal.pop_back();
							formatted += "." + decimal;
						}
						return formatted;
					};
					text << (nativeEconomyCheckpointMode ? "\nInventory (native amounts):" : "\nInventory (units):");
					for (std::size_t i = 0; i < selected->inventory.size(); ++i)
					{
						text << "\n  " << selected->inventory[i].resource << " " << formatUnits(selected->inventory[i].amount)
							<< "/" << formatUnits(selected->inventory[i].capacity);
					}
				}
				if (nativeEconomyCheckpointMode)
				{
					const auto native = nativeEconomyCheckpoint.world->Snapshot();
					const auto found = std::find_if(native.buildings.begin(), native.buildings.end(),
						[&](const auto& b) { return b.id == selected->id; });
					if (found != native.buildings.end())
					{
						const auto* type = nativeEconomyCheckpoint.world->GetCatalog().BuildingType(found->type);
						if (found->constructionSite)
						{
							static const char* const kSiteStates[] = { "waiting for materials", "building", "done" };
							const std::size_t stateIndex = std::min<std::size_t>(found->constructionState, 2);
							text << "\nConstruction site (OpenRepublic synthetic rules): " << kSiteStates[stateIndex]
								<< ", progress " << static_cast<int>(found->constructionProgress * 100.0 + 0.5) << "%";
							for (const auto& material : found->constructionMaterials)
							{
								const auto* resource = nativeEconomyCheckpoint.world->GetCatalog().Resource(material.resource);
								text << "\n  " << (resource ? resource->name : "resource") << " " << material.held << "/" << material.required;
							}
						}
						for (std::size_t i = 0; type && !(found->constructionSite && found->constructionState < 2) && i < found->recipes.size() && i < type->recipes.size(); ++i)
							text << "\n" << type->recipes[i].name << ": " << found->recipes[i].completedCycles
								<< " cycles, " << found->recipes[i].remainingTicks << " ticks remaining";
					}
				}
			}
			else
			{
				text << "\nSelected imported building is not present in the current economy snapshot.";
			}
		}
		if (regionHaulSourceId.has_value())
		{
			text << "\nHaul source: OR-" << *regionHaulSourceId << " (select a destination and choose Haul to selected).";
		}
		if (nativeCoalDemo && nativeEconomyCheckpoint.world) {
			const auto snapshot = nativeEconomyCheckpoint.world->Snapshot();
			const auto legName = [](economy::VehicleLeg leg) {
				switch (leg) { case economy::VehicleLeg::ToSource: return "to source"; case economy::VehicleLeg::Loading: return "loading";
				case economy::VehicleLeg::ToDestination: return "to destination"; case economy::VehicleLeg::Unloading: return "unloading";
				case economy::VehicleLeg::Returning: return "returning"; default: return "parked"; }
			};
			text << "\nDepot trucks:";
			for (const auto& truck : snapshot.vehicles) if (truck.ownerDepot) {
				text << "\n  Truck #" << truck.id << " | owner depot OR-" << truck.ownerDepot
					<< " | leg " << legName(truck.leg) << " | cargo " << truck.cargo;
			}
		}
		if (!uiNotice.empty() && !(nativeToolsReady && !nativeDetailsExpanded)) text << "\n" << uiNotice;
		if (economyOverlay)
		{
			const std::string action = economyOverlay->GetActionStatus();
			if (!action.empty()) text << "\n" << action;
		}
		const std::string utilityStatus = SelectedUtilityStatus(nativeDetailsExpanded);
		if (!utilityStatus.empty()) text << "\n" << utilityStatus;
		std::string statusText = text.str();
		if (nativeOverviewExpanded && overview)
		{
			std::ostringstream panel;
			panel << "Economy overview | tick " << overview->tick << " | treasury " << overview->treasury;
			panel << "\nHeat produced: " << overview->heatProduced << " (OpenRepublic synthetic output)";
			if (overview->populationAvailable)
				panel << "\nPopulation " << overview->population << " | housing " << overview->housingCapacity
					<< " | workers " << overview->workersAssigned << "/" << overview->workersRequired;
			else panel << "\nPopulation/workforce: unavailable (catalog has no population declarations)";
			panel << "\nStored resources:";
			for (const auto& resource : overview->resources)
			{
				panel << "\n  " << resource.name << ": " << resource.stored;
				if (resource.perTickDelta)
					panel << " (" << std::showpos << std::fixed << std::setprecision(2)
						<< *resource.perTickDelta << std::noshowpos << std::defaultfloat << " / tick)";
				else panel << " (rate warming up)";
			}
			panel << "\nConstruction sites: " << overview->constructionSites.size();
			static const char* const constructionStates[] = { "waiting materials", "building", "done" };
			for (const auto& site : overview->constructionSites)
				panel << "\n  OR-" << site.id << " " << site.type << " | "
					<< constructionStates[std::min<std::size_t>(site.state, 2)] << " | "
					<< static_cast<int>(std::clamp(site.progress, 0.0, 1.0) * 100.0 + 0.5) << "%";
			panel << "\nActive haul orders: " << overview->activeHauls.size() << " | trucks: " << overview->truckCount;
			const auto haulStateName = [](economy::HaulState state) {
				switch (state) {
				case economy::HaulState::Idle: return "idle";
				case economy::HaulState::ToDestination: return "to destination";
				case economy::HaulState::ToSource: return "to source";
				case economy::HaulState::Waiting: return "waiting";
				case economy::HaulState::NoRoute: return "no route";
				}
				return "unknown";
			};
			for (const auto& haul : overview->activeHauls)
				panel << "\n  #" << haul.id << " OR-" << haul.source << " -> OR-" << haul.destination
					<< " | " << haul.resource << " cargo " << haul.cargo << " | truck #" << haul.vehicle
					<< " | " << haulStateName(haul.state);
			statusText = panel.str();
		}
		if (nativeToolsReady && !nativeDetailsExpanded && !nativeOverviewExpanded && ActiveOwnedEconomyWorld())
		{
			const auto snapshot = ActiveOwnedEconomyWorld()->Snapshot();
			std::ostringstream compact;
			if (!uiNotice.empty()) compact << uiNotice << "\n";
			compact << "Tick " << snapshot.tick << " | " << snapshot.buildings.size() << " buildings | "
				<< snapshot.roads.size() << " roads | " << snapshot.orders.size() << " hauls";
			if (!snapshot.utilityConnections.empty()) compact << " | " << snapshot.utilityConnections.size() << " utility links";
			if (nativeEconomyCheckpointMode) compact << (nativeEconomyRunning ? " | running" : " | paused");
			if (selectedRegionBuildingEntity != wi::ecs::INVALID_ENTITY)
			{
				const auto selected = GetRegionBuildingForInstance(selectedRegionBuildingEntity);
				if (selected) compact << "\nSelected OR-" << selected->id << " " << selected->type << ": " << selected->stallReason;
			}
			if (!utilityStatus.empty()) compact << "\n" << utilityStatus;
			statusText = compact.str();
		}
		simulationStatus.SetText(statusText);
		if (renderPath)
		{
			const float viewportWidth = static_cast<float>(renderPath->GetLogicalWidth());
			const float viewportHeight = static_cast<float>(renderPath->GetLogicalHeight());
			const float statusWidth = std::max(240.0f, std::min(900.0f, viewportWidth - 36.0f));
			const bool ownedStatus = nativeToolsReady && ActiveOwnedEconomyWorld();
			const float statusHeightCap = ownedStatus ? (nativeOverviewExpanded ? 420.0f : (nativeDetailsExpanded ? 230.0f : (utilityStatus.empty() ? 80.0f : 120.0f))) : 420.0f;
			const float availableHeight = std::max(60.0f, viewportHeight - (nativeToolsReady ? nativeStatusTop : 54.0f) - 24.0f);
			const float targetStatusHeight = ownedStatus
				? std::min(statusHeightCap, std::max(60.0f, viewportHeight - 36.0f))
				: std::min(statusHeightCap, availableHeight);
			const float statusTop = ownedStatus ? std::max(18.0f, viewportHeight - targetStatusHeight - 18.0f) : (nativeToolsReady ? nativeStatusTop : 54.0f);
			simulationStatus.SetPos(XMFLOAT2(18, statusTop));
			simulationStatus.font.params.h_wrap = statusWidth - simulationStatus.margin_left - simulationStatus.margin_right;
			const float measuredTextHeight = wi::font::TextHeight(statusText, simulationStatus.font.params);
			const float desiredHeight = measuredTextHeight + 4.0f + simulationStatus.margin_top + simulationStatus.margin_bottom;
			const float statusHeight = std::max(60.0f, std::min(statusHeightCap, desiredHeight));
			simulationStatus.SetSize(XMFLOAT2(statusWidth, statusHeight));
		}
	}

	std::optional<openrepublic::preview::EconomyBuildingStatus>
	OpenRepublicApplication::GetRegionBuildingForInstance(wi::ecs::Entity instance) const
	{
		if (economyOverlay)
		{
			return economyOverlay->GetBuildingForInstance(instance);
		}
		if (!regionEconomySnapshot.has_value())
		{
			return std::nullopt;
		}
		if (nativeEconomyCheckpointMode && !nativeEconomyPresentationValid) return std::nullopt;
		const auto rendered = std::find_if(regionInstances.begin(), regionInstances.end(),
			[&](const auto& candidate) { return candidate.object == instance; });
		if (rendered == regionInstances.end())
		{
			return std::nullopt;
		}
		std::optional<economy::BuildingId> exactId;
		if (nativeEconomyCheckpointMode)
		{
			exactId = nativeEconomyScene.BuildingForEntity(instance);
		}
		else
		{
			const auto identity = std::find_if(regionEconomyImport.placedSourceBuildings.begin(),
			regionEconomyImport.placedSourceBuildings.end(), [&](const auto& candidate) {
				return candidate.sourcePlacementKey == rendered->sourcePlacementIndex;
			});
			if (identity != regionEconomyImport.placedSourceBuildings.end())
				exactId = identity->buildingId;
		}
		if (!exactId) return std::nullopt;
		const auto building = std::find_if(regionEconomySnapshot->buildings.begin(),
			regionEconomySnapshot->buildings.end(), [&](const auto& candidate) {
				return candidate.id == *exactId;
			});
		if (building == regionEconomySnapshot->buildings.end())
		{
			return std::nullopt;
		}
		preview::EconomyBuildingStatus status;
		status.id = building->id;
		status.type = building->type;
		status.enabled = building->enabled;
		switch (building->stallReason)
		{
		case economy::StallReason::Running: status.stallReason = "running"; break;
		case economy::StallReason::MissingInput: status.stallReason = "missing_input"; break;
		case economy::StallReason::OutputFull: status.stallReason = "output_full"; break;
		case economy::StallReason::Disabled: status.stallReason = "disabled"; break;
		}
		status.stallExplanation = building->stallExplanation;
		status.inventory.reserve(building->inventory.size());
		for (const auto& item : building->inventory)
		{
			status.inventory.push_back({ item.resource, item.amount, item.capacity });
		}
		return status;
	}

	void OpenRepublicApplication::RefreshRegionEconomy()
	{
		if (!regionEconomyApi)
		{
			return;
		}
		const auto queried = regionEconomyApi->QueryWorld();
		if (queried.error != openrepublic::economy_api::ErrorCode::None)
		{
			regionWorldStatus = "App-owned economy API could not read the current world.";
			return;
		}
		regionEconomySnapshot = queried.world;
		lastPresentedRegionRevision.reset();
		std::size_t applied = 0, rejected = 0, unavailable = 0;
		std::string lastFailure;

		for (auto it = regionPendingCommandHandles.begin(); it != regionPendingCommandHandles.end();)
		{
			const auto polled = regionEconomyApi->PollCommand(*it);
			if (polled.error != openrepublic::economy_api::ErrorCode::None)
			{
				++unavailable;
				lastFailure = "Economy command poll failed with API error " +
					std::to_string(static_cast<unsigned>(polled.error)) + ".";
				it = regionPendingCommandHandles.erase(it);
				continue;
			}
			if (polled.result.state == economy::CommandState::Queued)
			{
				++it;
				continue;
			}
			if (polled.result.state == economy::CommandState::Accepted)
			{
				++applied;
			}
			else
			{
				++rejected;
				lastFailure = polled.result.message;
			}
			it = regionPendingCommandHandles.erase(it);
		}
		if (applied || rejected || unavailable)
		{
			uiNotice = "Commands at tick " + std::to_string(regionEconomySnapshot->tick) + ": " +
				std::to_string(applied) + " applied, " + std::to_string(rejected) + " rejected, " +
				std::to_string(unavailable) + " unavailable.";
			if (!lastFailure.empty()) uiNotice += " Last issue: " + lastFailure;
		}

		const auto running = std::count_if(regionEconomySnapshot->buildings.begin(),
			regionEconomySnapshot->buildings.end(), [](const auto& building) {
				return building.stallReason == economy::StallReason::Running;
			});
		std::ostringstream status;
		status << regionEconomyDescription << "\nApp-owned economy: tick "
			<< regionEconomySnapshot->tick << ", " << running << "/"
			<< regionEconomySnapshot->buildings.size() << " buildings running, "
			<< regionEconomySnapshot->vehicles.size() << " trucks, "
			<< regionEconomySnapshot->utilityConnections.size() << " utility links";
		const auto ownedFleet = std::count_if(regionEconomySnapshot->vehicles.begin(),
			regionEconomySnapshot->vehicles.end(), [](const auto& vehicle) { return vehicle.ownerDepot != 0; });
		if (ownedFleet > 0)
		{
			const auto parkedFleet = std::count_if(regionEconomySnapshot->vehicles.begin(),
				regionEconomySnapshot->vehicles.end(), [](const auto& vehicle) {
					return vehicle.ownerDepot != 0 && vehicle.order == 0;
				});
			status << "; synthetic fleet " << parkedFleet << "/" << ownedFleet << " parked";
		}
		if (!nativeEconomyCheckpointMode)
			status << ", " << regionEconomyImport.placedSourceBuildings.size() << " exact placement bindings";
		regionWorldStatus = status.str();
		if (nativeObjectiveSet && nativeEconomyCheckpoint.world && nativeEconomyCheckpoint.catalog)
		{
			const auto progress = runtime::EvaluateNativeObjectives(*nativeObjectiveSet,
				nativeEconomyCheckpoint.world->Snapshot(), *nativeEconomyCheckpoint.catalog);
			std::ostringstream checklist;
			checklist << nativeObjectiveSet->title << "  " << progress.completedCount << "/"
				<< nativeObjectiveSet->objectives.size() << " complete\n[";
			constexpr int barWidth = 20;
			const int filled = static_cast<int>(progress.fraction * barWidth + 0.5);
			for (int i = 0; i < barWidth; ++i) checklist << (i < filled ? '#' : '-');
			checklist << "] " << static_cast<int>(progress.fraction * 100.0 + 0.5) << "%\n";
			for (std::size_t i = 0; i < nativeObjectiveSet->objectives.size(); ++i)
				checklist << (progress.complete[i] ? "[x] " : "[ ] ") << nativeObjectiveSet->objectives[i].title << '\n';
			checklist << (progress.finished ? "Scenario complete!" : "Hint: " + progress.currentHint);
			nativeObjectiveLabel.SetText(checklist.str());
		}
	}

	void OpenRepublicApplication::UpdateRegionEconomyPresentation()
	{
		if ((!mapPlacement && !syntheticRegionFixtureMode && !nativeEconomyCheckpointMode) || !regionEconomySnapshot.has_value() ||
			(lastPresentedRegionRevision.has_value() &&
				*lastPresentedRegionRevision == regionEconomySnapshot->revision))
		{
			return;
		}
		lastPresentedRegionRevision = regionEconomySnapshot->revision;
		if (nativeEconomyCheckpointMode)
		{
			std::string error;
			if (!nativeEconomyScene.Sync(previewWorld, nativeEconomyCheckpoint.world->Snapshot(),
				*nativeEconomyCheckpoint.catalog, regionInstances, error))
			{
				uiNotice = "Native economy scene update failed: " + error;
				nativeEconomyPresentationValid = false;
				return;
			}
			nativeEconomyPresentationValid = true;
		}
		else if (mapPlacement && regionEconomyWorld &&
			regionCatalogImportOptions.syntheticRegionFleetCount > 0)
		{
			runtime::NativeEconomyTransform transform;
			const double localOriginX = regionEconomyImport.originX - mapPlacement->placement.X();
			const double localOriginZ = regionEconomyImport.originZ - mapPlacement->placement.Z();
			if (!runtime::BuildImportedRegionVehicleTransform(localOriginX, localOriginZ,
				regionEconomyImport.cellSizeMeters, transform))
			{
				uiNotice = "Imported fleet vehicle transform could not match the region importer cell grid.";
				regionVehicleTransform.reset();
				return;
			}
			std::string vehicleError;
			if (!nativeEconomyScene.SyncVehiclesOnly(previewWorld, regionEconomyWorld->Snapshot(),
				regionEconomyWorld->GetCatalog(), transform, vehicleError))
			{
				uiNotice = "Imported region vehicle presentation failed: " + vehicleError;
				regionVehicleTransform.reset();
				return;
			}
			regionVehicleTransform = transform;
		}

		std::unordered_map<std::uint64_t, openrepublic::economy::BuildingId> buildingBySource;
		buildingBySource.reserve(regionEconomyImport.placedSourceBuildings.size());
		for (const auto& identity : regionEconomyImport.placedSourceBuildings)
		{
			buildingBySource.emplace(identity.sourcePlacementKey, identity.buildingId);
		}
		std::unordered_map<openrepublic::economy::BuildingId,
			const openrepublic::economy_api::BuildingView*> stateByBuilding;
		stateByBuilding.reserve(regionEconomySnapshot->buildings.size());
		for (const auto& building : regionEconomySnapshot->buildings)
		{
			stateByBuilding.emplace(building.id, &building);
		}
		for (const auto& instance : regionInstances)
		{
			wi::scene::ObjectComponent* object = previewWorld.objects.GetComponent(instance.object);
			if (object == nullptr)
			{
				continue;
			}
			object->color = XMFLOAT4(1, 1, 1, 1);
			std::optional<economy::BuildingId> id;
			if (nativeEconomyCheckpointMode) id = nativeEconomyScene.BuildingForEntity(instance.object);
			else
			{
				const auto source = buildingBySource.find(instance.sourcePlacementIndex);
				if (source != buildingBySource.end()) id = source->second;
			}
			if (!id) continue;
			const auto state = stateByBuilding.find(*id);
			if (state == stateByBuilding.end())
			{
				continue;
			}
			switch (state->second->stallReason)
			{
			case openrepublic::economy::StallReason::Running:
				object->color = XMFLOAT4(0.55f, 1.0f, 0.55f, 1.0f); break;
			case openrepublic::economy::StallReason::MissingInput:
				object->color = XMFLOAT4(1.0f, 0.85f, 0.25f, 1.0f); break;
			case openrepublic::economy::StallReason::OutputFull:
				object->color = XMFLOAT4(1.0f, 0.5f, 0.2f, 1.0f); break;
			case openrepublic::economy::StallReason::Disabled:
				object->color = XMFLOAT4(0.45f, 0.45f, 0.5f, 1.0f); break;
			}
		}

		openrepublic::content::TerrainTransform terrainTransform{};
		double centerX = syntheticRegionCenterX;
		double centerZ = syntheticRegionCenterZ;
		if (mapPlacement)
		{
			terrainTransform = openrepublic::content::MakeMapTerrainTransform(mapPlacement->metadata);
			centerX = mapPlacement->placement.X();
			centerZ = mapPlacement->placement.Z();
		}
		if (nativeEconomyCheckpointMode ||
			(mapPlacement && regionCatalogImportOptions.syntheticRegionFleetCount > 0)) return;
		while (regionTruckEntities.size() < regionEconomySnapshot->vehicles.size())
		{
			wi::ecs::Entity truck = wi::ecs::INVALID_ENTITY;
			if (mapShowcaseEnhancedLook && mapPlacement)
			{
				// Modern look: a real (procedural) truck model instead of a debug cube.
				if (regionTruckMesh == wi::ecs::INVALID_ENTITY)
				{
					regionTruckMesh = preview::CreateProceduralMeshEntity(previewWorld, "app-owned economy truck mesh",
						openrepublic::procgeo::MakeVehicle(openrepublic::procgeo::VehicleKind::CargoTruck, 5), 0.5f, 0.1f);
				}
				if (regionTruckMesh != wi::ecs::INVALID_ENTITY)
				{
					truck = previewWorld.Entity_CreateObject("app-owned economy truck marker");
					previewWorld.objects.GetComponent(truck)->meshID = regionTruckMesh;
				}
			}
			if (truck == wi::ecs::INVALID_ENTITY)
			{
				truck = previewWorld.Entity_CreateCube("app-owned economy truck marker");
				if (wi::scene::MaterialComponent* material = previewWorld.materials.GetComponent(truck))
				{
					material->SetBaseColor(XMFLOAT4(1.0f, 0.2f, 0.1f, 1.0f));
					material->CreateRenderData(true);
				}
			}
			regionTruckEntities.push_back(truck);
			regionTruckPose.push_back({ 0.0, 0.0, 0.0 });
		}
		for (std::size_t i = 0; i < regionTruckEntities.size(); ++i)
		{
			wi::scene::TransformComponent* marker = previewWorld.transforms.GetComponent(regionTruckEntities[i]);
			if (marker == nullptr)
			{
				continue;
			}
			if (i >= regionEconomySnapshot->vehicles.size())
			{
				marker->ClearTransform();
				marker->Translate(XMFLOAT3(0, -10000, 0));
				marker->UpdateTransform();
				continue;
			}
			const auto& vehicle = regionEconomySnapshot->vehicles[i];
			if (nativeEconomyCheckpointMode)
			{
				const auto position = nativeEconomyScene.CellPosition(vehicle.cell);
				marker->ClearTransform();
				marker->Scale(XMFLOAT3(0.65f, 0.45f, 0.9f));
				marker->Translate(XMFLOAT3(position.x, position.y + 0.45f, position.z));
				marker->UpdateTransform();
				continue;
			}
			const auto cellCenter = [](double origin, std::int32_t cell)
			{
				constexpr double cellSizeMeters = 4.0;
				return origin + (static_cast<double>(cell) + 0.5) * cellSizeMeters;
			};
			const double worldX = cellCenter(regionEconomyImport.originX, vehicle.cell.x);
			// The synthetic fixture road is centered at Z=-4 m. Its vehicles use
			// road-grid cells whose generic half-cell anchor falls beside the surface;
			// keep that fixture marker on the authored road while preserving the
			// existing cell-center policy for imported map regions.
			const double worldZ = syntheticRegionFixtureMode
				? -4.0
				: cellCenter(regionEconomyImport.originZ, vehicle.cell.z);
			double height = 0.0;
			if (mapPlacement)
			{
				openrepublic::content::SampleTerrainHeight(mapPlacement->heightmap, terrainTransform,
					std::min(std::max(worldX, terrainTransform.originX), terrainTransform.originX + terrainTransform.sizeX),
					std::min(std::max(worldZ, terrainTransform.originZ), terrainTransform.originZ + terrainTransform.sizeZ), height, nullptr);
			}
			marker->ClearTransform();
			if (regionTruckMesh != wi::ecs::INVALID_ENTITY && previewWorld.objects.GetComponent(regionTruckEntities[i]) != nullptr &&
				previewWorld.objects.GetComponent(regionTruckEntities[i])->meshID == regionTruckMesh)
			{
				auto& last = regionTruckPose[i];
				if ((last[0] != 0.0 || last[1] != 0.0) && std::hypot(worldX - last[0], worldZ - last[1]) > 0.5)
				{
					last[2] = std::atan2(worldX - last[0], worldZ - last[1]);
				}
				last[0] = worldX;
				last[1] = worldZ;
				marker->RotateRollPitchYaw(XMFLOAT3(0.0f, static_cast<float>(last[2]), 0.0f));
				marker->Translate(XMFLOAT3(static_cast<float>(worldX - centerX), static_cast<float>(height) + 0.35f,
					static_cast<float>(worldZ - centerZ)));
				marker->UpdateTransform();
				continue;
			}
			constexpr float truckHalfHeight = 0.45f;
			marker->Scale(XMFLOAT3(0.65f, truckHalfHeight, 0.9f));
			marker->Translate(XMFLOAT3(static_cast<float>(worldX - centerX),
				static_cast<float>(height) + truckHalfHeight + (syntheticRegionFixtureMode ? 0.1f : 0.0f),
				static_cast<float>(worldZ - centerZ)));
			marker->UpdateTransform();
		}
	}

	void OpenRepublicApplication::LayoutNativeControls()
	{
		if (!nativeToolsReady || !renderPath) return;
		const float width = static_cast<float>(renderPath->GetLogicalWidth());
		float x = 18.0f, y = 18.0f;
		for (auto* button : { &regionStepOneButton, &regionStepButton, &regionToggleButton,
			&regionHaulSourceButton, &regionHaulButton, &nativeFrameButton, &nativeCameraFollowButton, &nativeStepManyButton, &nativeRunPauseButton,
			&nativeDetailsButton, &nativeOverviewButton, &nativeGameMenuButton, &nativeObjectiveRestartButton })
		{
			if (!nativeObjectiveSet && button == &nativeObjectiveRestartButton) continue;
			if (!nativeEconomyCheckpointMode) continue;
			if (x > 18.0f && x + 132.0f > width - 18.0f) { x = 18.0f; y += 34.0f; }
			button->SetPos(XMFLOAT2(x, y));
			x += 138.0f;
		}
		nativeStatusTop = nativeEconomyTools.Layout(width, y + 36.0f);
		if (nativeObjectiveSet)
		{
			nativeObjectiveLabel.SetPos(XMFLOAT2(18.0f, nativeStatusTop + 6.0f));
			nativeObjectiveLabel.SetVisible(!nativeGameMenuExpanded);
		}
		float menuY = nativeStatusTop + 8.0f;
		for (auto& button : nativeNewGameButtons)
		{
			button.SetVisible(nativeGameMenuExpanded);
			button.SetPos(XMFLOAT2(18.0f, menuY));
			menuY += 32.0f;
		}
		nativeSaveButton.SetVisible(nativeGameMenuExpanded);
		nativeSaveButton.SetPos(XMFLOAT2(18.0f, menuY));
		nativeQuitButton.SetVisible(nativeGameMenuExpanded);
		nativeQuitButton.SetPos(XMFLOAT2(18.0f, menuY + 32.0f));
		menuY += 64.0f;
		for (auto& button : nativeSaveSlotButtons)
		{
			button.SetVisible(nativeGameMenuExpanded && !nativeSaveSlots.empty());
			button.SetPos(XMFLOAT2(218.0f, menuY));
			menuY += 32.0f;
		}
		for (std::size_t i = 0; i < nativeSavePageButtons.size(); ++i)
		{
			nativeSavePageButtons[i].SetVisible(nativeGameMenuExpanded &&
				(i == 0 ? nativeSaveSlotOffset > 0 : nativeSaveSlotOffset + nativeSaveSlotButtons.size() < nativeSaveSlots.size()));
			nativeSavePageButtons[i].SetPos(XMFLOAT2(18.0f + 138.0f * static_cast<float>(i), menuY));
		}
	}

	void OpenRepublicApplication::RestartNativeObjectives()
	{
		if (nativeObjectiveBaseline.empty() || !nativeEconomyCheckpoint.world || !nativeEconomyCheckpoint.catalog)
		{
			uiNotice = "Objective restart is unavailable for this world.";
			UpdateRegionStatus();
			return;
		}
		nativeEconomyRunning = false;
		nativeEconomyRunClock.Reset();
		nativeRunPauseButton.SetText("Run");
		openrepublic::economy::ParseError error;
		if (!openrepublic::economy::LoadWorld(nativeObjectiveBaseline, *nativeEconomyCheckpoint.catalog,
			*nativeEconomyCheckpoint.world, error))
		{
			uiNotice = "Could not restart objectives: " + error.message;
			UpdateRegionStatus();
			return;
		}
		regionPendingCommandHandles.clear();
		nativeOverviewHistory.clear();
		uiNotice = "Objective scenario restarted from its authored starting world.";
		RefreshRegionEconomy();
		UpdateRegionEconomyPresentation();
		UpdateRegionStatus();
	}

	openrepublic::economy::World* OpenRepublicApplication::ActiveOwnedEconomyWorld() noexcept
	{
		if (nativeEconomyCheckpointMode) return nativeEconomyCheckpoint.world.get();
		if (regionEconomyApi && (mapPlacement || syntheticRegionFixtureMode)) return regionEconomyWorld.get();
		return nullptr;
	}

	const openrepublic::economy::World* OpenRepublicApplication::ActiveOwnedEconomyWorld() const noexcept
	{
		if (nativeEconomyCheckpointMode) return nativeEconomyCheckpoint.world.get();
		if (regionEconomyApi && (mapPlacement || syntheticRegionFixtureMode)) return regionEconomyWorld.get();
		return nullptr;
	}

	void OpenRepublicApplication::ToggleNativeRunPause()
	{
		if (!nativeEconomyCheckpointMode) return;
		if (nativeEconomyRunning)
		{
			nativeEconomyRunning = false;
			nativeEconomyRunClock.Reset();
			nativeRunPauseButton.SetText("Run");
			uiNotice = "Native economy paused.";
		}
		else if (!nativeEconomyPresentationValid || !regionEconomyApi)
		{
			nativeEconomyRunning = false;
			nativeEconomyRunClock.Reset();
			nativeRunPauseButton.SetText("Run");
			uiNotice = "Cannot run while the native scene or EconomyApi is unavailable; Frame world first.";
		}
		else
		{
			nativeEconomyRunning = true;
			nativeEconomyRunClock.Reset();
			nativeRunPauseButton.SetText("Pause");
			uiNotice = "Native economy running at four ticks per second.";
		}
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::RefreshNativeHaulChoices()
	{
		auto* world = ActiveOwnedEconomyWorld();
		if (!nativeToolsReady || !world) return;
		const auto snapshot = world->Snapshot();
		nativeEconomyTools.RefreshOrders(snapshot, world->GetCatalog());
		nativeEconomyTools.RefreshRoads(snapshot);
		std::vector<economy::BuildingId> importedBuildings;
		const std::vector<economy::BuildingId>* dispatchBuildings = nullptr;
		if (mapPlacement && regionCatalogImportOptions.syntheticRegionFleetCount > 0)
		{
			importedBuildings.reserve(regionEconomyImport.placedSourceBuildings.size());
			for (const auto& identity : regionEconomyImport.placedSourceBuildings)
				importedBuildings.push_back(identity.buildingId);
			dispatchBuildings = &importedBuildings;
		}
		nativeEconomyTools.RefreshDispatchChoices(snapshot, world->GetCatalog(), dispatchBuildings);
		if (regionHaulSourceId && std::none_of(snapshot.buildings.begin(), snapshot.buildings.end(),
			[&](const auto& b) { return b.id == *regionHaulSourceId; })) regionHaulSourceId.reset();
		nativeEconomyTools.RefreshHaulChoices(snapshot, world->GetCatalog(),
			regionHaulSourceId ? std::optional<economy::BuildingId>(static_cast<economy::BuildingId>(*regionHaulSourceId)) : std::nullopt,
			selectedRegionBuildingId ? std::optional<economy::BuildingId>(static_cast<economy::BuildingId>(*selectedRegionBuildingId)) : std::nullopt);
	}

	void OpenRepublicApplication::SaveNativeCheckpoint()
	{
		if (!nativeEconomyCheckpointMode || !regionEconomyApi) return;
		// Pause before opening the modal dialog; a cancelled save stays paused.
		nativeEconomyRunning = false;
		nativeEconomyRunClock.Reset();
		nativeRunPauseButton.SetText("Run");
		const auto current = regionEconomyApi->QueryWorld();
		if (current.error != economy_api::ErrorCode::None)
		{
			uiNotice = "Cannot query the native world for saving.";
			UpdateRegionStatus();
			return;
		}
		const auto checkpoint = regionEconomyApi->ExportCheckpoint(current.world.revision);
		if (checkpoint.error != economy_api::ErrorCode::None)
		{
			uiNotice = "Checkpoint export failed: " + checkpoint.message;
			UpdateRegionStatus();
			return;
		}
		std::error_code directoryError;
		std::filesystem::create_directories(nativeSaveDirectory, directoryError);
		if (directoryError) { uiNotice = "Could not create save directory: " + directoryError.message(); UpdateRegionStatus(); return; }
		std::array<wchar_t, 32768> path{};
		const std::wstring initial = L"republic-" + std::to_wstring(checkpoint.tick) + L".orsave";
		std::copy(initial.begin(), initial.end(), path.begin());
		OPENFILENAMEW dialog{};
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = window;
		dialog.lpstrFilter = L"OpenRepublic save slots\0*.orsave\0All files\0*.*\0\0";
		dialog.lpstrFile = path.data();
		dialog.nMaxFile = static_cast<DWORD>(path.size());
		dialog.lpstrInitialDir = nativeSaveDirectory.c_str();
		dialog.lpstrDefExt = L"orsave";
		dialog.lpstrTitle = L"Save game slot";
		dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
		if (!GetSaveFileNameW(&dialog))
		{
			const auto error = CommDlgExtendedError();
			uiNotice = error == 0 ? "Checkpoint save cancelled; native economy remains paused."
				: "Checkpoint dialog failed with code " + std::to_string(error) + ".";
		}
		else
		{
			std::string error;
			const auto chosen = std::filesystem::path(path.data());
			std::wstring chosenParent = chosen.parent_path().lexically_normal().native();
			std::wstring saveParent = nativeSaveDirectory.lexically_normal().native();
			if (_wcsicmp(chosenParent.c_str(), saveParent.c_str()) != 0)
			{
				uiNotice = "Save slots must stay in the OpenRepublic user-data save directory.";
			}
			else if (!runtime::WriteSaveSlot(nativeSaveDirectory, chosen.stem().string(), checkpoint.text,
				static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()),
				checkpoint.tick, current.world.buildings.size(), error))
			{
				uiNotice = "Game slot was not saved: " + error;
			}
			else
			{
				nativeSavedRevision = current.world.revision;
				uiNotice = "Saved game at tick " + std::to_string(checkpoint.tick) + " in slot " + chosen.stem().string();
				RefreshNativeSaveSlots();
			}
		}
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::RefreshNativeSaveSlots()
	{
		std::string error;
		nativeSaveSlots.clear();
		if (!runtime::ListSaveSlots(nativeSaveDirectory, nativeSaveSlots, error))
		{
			uiNotice = "Could not list save slots: " + error;
			return;
		}
		if (nativeSaveSlotOffset >= nativeSaveSlots.size())
			nativeSaveSlotOffset = nativeSaveSlots.empty() ? 0 : ((nativeSaveSlots.size() - 1) / nativeSaveSlotButtons.size()) * nativeSaveSlotButtons.size();
		for (std::size_t i = 0; i < nativeSaveSlotButtons.size(); ++i)
		{
			auto& button = nativeSaveSlotButtons[i];
			const auto slotIndex = nativeSaveSlotOffset + i;
			if (slotIndex >= nativeSaveSlots.size()) { button.SetVisible(false); continue; }
			const auto& slot = nativeSaveSlots[slotIndex];
			std::string label = "Load: " + slot.name;
			if (slot.savedAtUnixSeconds != 0 && slot.savedAtUnixSeconds <= static_cast<std::uint64_t>(std::numeric_limits<std::time_t>::max()))
			{
				const std::time_t stamp = static_cast<std::time_t>(slot.savedAtUnixSeconds);
				std::tm local{};
				localtime_s(&local, &stamp);
				char date[32]{};
				std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &local);
				label += " | " + std::string(date);
				if (slot.hasMetadata) label += " | tick " + std::to_string(slot.tick) +
					" | buildings " + std::to_string(slot.buildingCount);
			}
			button.SetText(label);
			button.SetVisible(true);
		}
		nativeSavePageButtons[0].SetVisible(nativeGameMenuExpanded && nativeSaveSlotOffset > 0);
		nativeSavePageButtons[1].SetVisible(nativeGameMenuExpanded && nativeSaveSlotOffset + nativeSaveSlotButtons.size() < nativeSaveSlots.size());
	}

	bool OpenRepublicApplication::LaunchNativeSession(const std::wstring& arguments)
	{
		std::array<wchar_t, 32768> executable{};
		const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
		if (length == 0 || length >= executable.size()) { uiNotice = "Could not locate OpenRepublic.exe."; return false; }
		const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", executable.data(),
			arguments.c_str(), std::filesystem::path(executable.data()).parent_path().c_str(), SW_SHOWNORMAL));
		if (result <= 32) { uiNotice = "Could not start the requested game session (Windows error " + std::to_string(result) + ")."; return false; }
		nativeAllowClose = true;
		PostMessageW(window, WM_CLOSE, 0, 0);
		return true;
	}

	void OpenRepublicApplication::StartNativeNewGame(const wchar_t* arguments)
	{
		if (!ConfirmQuit()) return;
		if (!LaunchNativeSession(arguments ? arguments : L"--native-economy")) UpdateRegionStatus();
	}

	void OpenRepublicApplication::LoadNativeSaveSlot(std::size_t index)
	{
		index += nativeSaveSlotOffset;
		if (index >= nativeSaveSlots.size()) return;
		std::string checkpoint;
		std::string error;
		if (!runtime::ReadSaveSlot(nativeSaveSlots[index].path, checkpoint, nullptr, error))
		{
			uiNotice = "Cannot load selected save slot: " + error;
			UpdateRegionStatus();
			return;
		}
		economy::RestoredCheckpoint candidate;
		economy::ParseError parseError;
		runtime::NativeEconomyScenePlan plan;
		if (!economy::RestoreCheckpoint(checkpoint, candidate, parseError) ||
			!runtime::BuildNativeEconomyScenePlan(candidate.world->Snapshot(), *candidate.catalog, plan, error) ||
			!runtime::NativeEconomyPlanFitsFloatPrecision(plan, error))
		{
			uiNotice = "Selected slot cannot be restored in the desktop view: " +
				(!parseError.message.empty() ? parseError.message : error);
			UpdateRegionStatus();
			return;
		}
		if (!ConfirmQuit()) return;
		const auto path = nativeSaveSlots[index].path.wstring();
		if (!LaunchNativeSession(L"--economy-checkpoint \"" + path + L"\"")) UpdateRegionStatus();
	}

	bool OpenRepublicApplication::ConfirmQuit()
	{
		if (nativeAllowClose || !nativeEconomyCheckpointMode || !nativeEconomyCheckpoint.world) return true;
		const bool unsaved = nativeEconomyCheckpoint.world->Snapshot().revision != nativeSavedRevision;
		if (!unsaved) return true;
		return MessageBoxW(window, L"This game has unsaved changes. Quit without saving?", L"OpenRepublic",
			MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES;
	}

	void OpenRepublicApplication::UpdateNativeAutosave()
	{
		if (!nativeEconomyCheckpointMode || !nativeEconomyCheckpoint.world) return;
		const auto snapshot = nativeEconomyCheckpoint.world->Snapshot();
		if (snapshot.tick < nextNativeAutosaveTick) return;
		while (nextNativeAutosaveTick <= snapshot.tick) nextNativeAutosaveTick += 2400; // 10 simulation minutes at 4 ticks/second.
		std::string checkpoint;
		economy::ParseError parseError;
		std::string error;
		if (!economy::SaveCheckpoint(*nativeEconomyCheckpoint.world, checkpoint, parseError))
		{
			uiNotice = "Autosave failed: " + parseError.message;
			UpdateRegionStatus();
			return;
		}
		const auto now = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
		if (runtime::WriteAutosave(nativeSaveDirectory, checkpoint, now, snapshot.tick, snapshot.buildings.size(), nextNativeAutosaveSlot, error))
		{
			nativeSavedRevision = snapshot.revision;
			uiNotice = "Autosaved game at tick " + std::to_string(snapshot.tick) + " (10 simulation minutes; three rotating slots).";
		}
		else uiNotice = "Autosave failed: " + error;
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::QueueNativeRoadToggle()
	{
		if (!nativeEconomyCheckpointMode || !CanQueueNativeCommand()) return;
		const auto id = nativeEconomyTools.SelectedRoad();
		const auto snapshot = nativeEconomyCheckpoint.world->Snapshot();
		const auto road = std::find_if(snapshot.roads.begin(), snapshot.roads.end(),
			[id](const auto& candidate) { return candidate.id == id; });
		if (id == 0 || road == snapshot.roads.end())
		{
			uiNotice = "Choose a current native road first.";
			RefreshNativeHaulChoices();
			UpdateRegionStatus();
			return;
		}
		TrackNativeSubmission(regionEconomyApi->SetRoadEnabled(snapshot.revision, id, !road->enabled),
			std::string(road->enabled ? "disable road #" : "enable road #") + std::to_string(id));
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::QueueNativeUtilityConnection(bool connect)
	{
		auto* world = ActiveOwnedEconomyWorld();
		if (!world || !CanQueueNativeCommand()) return;
		const auto* resource = world->GetCatalog().Resource(nativeEconomyTools.SelectedUtilityResource());
		if (!regionHaulSourceId || !selectedRegionBuildingId ||
			*regionHaulSourceId == *selectedRegionBuildingId || !resource || !resource->utility)
		{
			uiNotice = "Set a source, select a different building, and choose a utility resource.";
			UpdateRegionStatus();
			return;
		}
		const auto snapshot = world->Snapshot();
		const auto source = static_cast<economy::BuildingId>(*regionHaulSourceId);
		const auto destination = static_cast<economy::BuildingId>(*selectedRegionBuildingId);
		TrackNativeSubmission(connect
			? regionEconomyApi->ConnectUtility(snapshot.revision, resource->name, source, destination)
			: regionEconomyApi->DisconnectUtility(snapshot.revision, resource->name, source, destination),
			std::string(connect ? "connect " : "disconnect ") + resource->name + " utility");
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::QueueNativeOrderChange(bool retarget)
	{
		auto* world = ActiveOwnedEconomyWorld();
		if (!world || !CanQueueNativeCommand()) return;
		const auto id = nativeEconomyTools.SelectedOrder();
		const auto snapshot = world->Snapshot();
		const auto order = std::find_if(snapshot.orders.begin(), snapshot.orders.end(),
			[id](const auto& candidate) { return candidate.id == id; });
		if (id == 0 || order == snapshot.orders.end())
		{
			uiNotice = "Choose a current native haul order first.";
			RefreshNativeHaulChoices();
			UpdateRegionStatus();
			return;
		}
		if (retarget && (!selectedRegionBuildingId || std::none_of(snapshot.buildings.begin(), snapshot.buildings.end(),
			[this](const auto& building) { return building.id == *selectedRegionBuildingId; })))
		{
			uiNotice = "Select the new destination building in the world before retargeting.";
			UpdateRegionStatus();
			return;
		}
		// Core validation on stepping preserves cargo and rejects unsafe cancellation.
		// Use the exact chosen order ID, never a dropdown index or rendered vehicle ID.
		const auto submitted = retarget
			? regionEconomyApi->RetargetHaulOrder(snapshot.revision, id,
				static_cast<economy::BuildingId>(*selectedRegionBuildingId))
			: regionEconomyApi->CancelHaulOrder(snapshot.revision, id);
		TrackNativeSubmission(submitted, std::string(retarget ? "retarget of haul " : "cancellation of haul ") + std::to_string(id));
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::QueueNativeDispatch()
	{
		auto* world = ActiveOwnedEconomyWorld();
		if (!world || !regionEconomyApi || !CanQueueNativeCommand()) return;
		const auto vehicle = nativeEconomyTools.SelectedDispatchVehicle();
		const auto source = nativeEconomyTools.SelectedDispatchSource();
		const auto destination = nativeEconomyTools.SelectedDispatchDestination();
		const auto* resource = world->GetCatalog().Resource(nativeEconomyTools.SelectedDispatchResource());
		const auto amount = nativeEconomyTools.SelectedDispatchAmount();
		if (!vehicle || !source || !destination || source == destination || !resource || resource->utility || !amount) { uiNotice = "Choose an owned truck, two buildings, a storable resource, and an amount."; UpdateRegionStatus(); return; }
		const auto snapshot = world->Snapshot();
		TrackNativeSubmission(regionEconomyApi->DispatchTruck(snapshot.revision, vehicle, source, destination, resource->name, amount),
			"dispatch truck #" + std::to_string(vehicle) + " with " + resource->name);
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::QueueNativeCoalImport()
	{
		auto* world = ActiveOwnedEconomyWorld();
		if (!nativeCoalDemo || !world || !regionEconomyApi || !CanQueueNativeCommand() || !coalDemoBorder || coalDemoResource == economy::InvalidResourceId) return;
		const auto snapshot = world->Snapshot();
		TrackNativeSubmission(regionEconomyApi->TradeImport(snapshot.revision, coalDemoBorder, "coal", 4, 4), "import 4 coal");
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::UpdateNativeCoalAutomation()
	{
		if (!nativeCoalDemo || !nativeCoalAutoRun || !regionEconomyApi || !nativeEconomyCheckpoint.world || !regionPendingCommandHandles.empty()) return;
		const auto snapshot = nativeEconomyCheckpoint.world->Snapshot();
		const auto action = openrepublic::runtime::NextCoalDemoAction(snapshot, coalDemoBorder, coalDemoDepot,
			coalDemoStorage, coalDemoPlant, coalDemoGrain, coalDemoResource, 4);
		if (!action) return;
		if (action->kind == openrepublic::runtime::CoalDemoActionKind::ExportGrain)
			TrackNativeSubmission(regionEconomyApi->TradeExport(snapshot.revision, action->source, "grain", action->amount, action->amount), "auto export grain to fund coal");
		else if (action->kind == openrepublic::runtime::CoalDemoActionKind::ImportCoal)
			TrackNativeSubmission(regionEconomyApi->TradeImport(snapshot.revision, action->source, "coal", action->amount, action->amount), "auto import coal");
		else
			TrackNativeSubmission(regionEconomyApi->DispatchTruck(snapshot.revision, action->vehicle, action->source,
				action->destination, "coal", action->amount), "auto dispatch truck #" + std::to_string(action->vehicle));
	}

	bool OpenRepublicApplication::CanQueueNativeCommand()
	{
		if (!nativeToolsReady || !regionEconomyApi || (nativeEconomyCheckpointMode && !nativeEconomyPresentationValid))
		{
			uiNotice = nativeEconomyCheckpointMode
				? "Native controls need a current scene. Frame world before continuing."
				: "Imported-region logistics controls are unavailable.";
			UpdateRegionStatus();
			return false;
		}
		if (regionPendingCommandHandles.size() >= kMaxPendingRegionCommands)
		{
			uiNotice = "Pending command limit reached. Step the world before queuing more.";
			UpdateRegionStatus();
			return false;
		}
		return true;
	}

	bool OpenRepublicApplication::TrackNativeSubmission(const economy_api::Submission& submitted, const std::string& action)
	{
		if (submitted.error != economy_api::ErrorCode::None)
		{
			uiNotice = "Could not queue " + action + " (API error " + std::to_string(static_cast<unsigned>(submitted.error)) + ").";
			return false;
		}
		regionPendingCommandHandles.push_back(submitted.handle);
		uiNotice = "Queued " + action + "; step the world to validate and apply it.";
		return true;
	}

	void OpenRepublicApplication::BeginNativeBuildingPlacement()
	{
		if (!CanQueueNativeCommand()) return;
		const auto type = nativeEconomyTools.SelectedBuildingType();
		if (!nativeEconomyCheckpoint.world->GetCatalog().BuildingType(type)) return;
		ClearNativeConstructionPreview();
		nativePlacementType = type;
		nativeConstructionMode = NativeConstructionMode::Building;
		nativeRoadStart.reset();
		uiNotice = "Choose a ground cell for the building footprint. Placement is queued and validated on the next step.";
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::BeginNativeRoadPlacement()
	{
		if (!CanQueueNativeCommand()) return;
		ClearNativeConstructionPreview();
		nativeConstructionMode = NativeConstructionMode::Road;
		nativeRoadStart.reset();
		uiNotice = "Click the first road cell, then another on the same row or column. Maximum 64 unit segments.";
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::CancelNativeConstruction()
	{
		nativeConstructionMode = NativeConstructionMode::None;
		nativePlacementType = economy::InvalidBuildingTypeId;
		nativeRoadStart.reset();
		ClearNativeConstructionPreview();
		uiNotice = "Construction cancelled; click a building to inspect it.";
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::QueueNativeRemoval()
	{
		if (!CanQueueNativeCommand()) return;
		if (!selectedRegionBuildingId || !GetRegionBuildingForInstance(selectedRegionBuildingEntity))
		{
			uiNotice = "Select a native building before removing it.";
			UpdateRegionStatus();
			return;
		}
		CancelNativeConstruction();
		RefreshRegionEconomy();
		TrackNativeSubmission(regionEconomyApi->RemoveBuilding(regionEconomySnapshot->revision,
			static_cast<economy::BuildingId>(*selectedRegionBuildingId)), "removal of OR-" + std::to_string(*selectedRegionBuildingId) + " and its inventory");
		UpdateRegionStatus();
	}

	std::optional<economy::Cell> OpenRepublicApplication::NativeCellAtPointer() const
	{
		if (!nativeEconomyCheckpointMode || !nativeEconomyPresentationValid || !renderPath || !renderPath->camera) return std::nullopt;
		const auto pointer = wi::input::GetPointer();
		if (pointer.x < 0 || pointer.y < 0 || pointer.x >= renderPath->GetLogicalWidth() || pointer.y >= renderPath->GetLogicalHeight()) return std::nullopt;
		const auto ray = wi::renderer::GetPickRay(static_cast<long>(pointer.x), static_cast<long>(pointer.y), *renderPath, *renderPath->camera);
		if (!std::isfinite(ray.origin.y) || !std::isfinite(ray.direction.y) || std::abs(ray.direction.y) < 1e-6f) return std::nullopt;
		double targetY = 0.0;
		double distance = -static_cast<double>(ray.origin.y) / ray.direction.y;
		for (int iteration = 0; iteration < (nativeTerrainEnabled ? 5 : 1); ++iteration)
		{
			const double x = ray.origin.x + static_cast<double>(ray.direction.x) * distance;
			const double z = ray.origin.z + static_cast<double>(ray.direction.z) * distance;
			targetY = nativeEconomyScene.GroundHeight(x, z);
			distance = (targetY - static_cast<double>(ray.origin.y)) / ray.direction.y;
		}
		if (!std::isfinite(distance) || distance < ray.TMin || distance > ray.TMax) return std::nullopt;
		const double worldX = ray.origin.x + static_cast<double>(ray.direction.x) * distance;
		const double worldZ = ray.origin.z + static_cast<double>(ray.direction.z) * distance;
		economy::Cell cell;
		const auto snapshot = nativeEconomyCheckpoint.world->Snapshot();
		if (!nativeEconomyScene.CellFromGroundPoint(worldX, worldZ, snapshot.gridWidth, snapshot.gridDepth, cell)) return std::nullopt;
		return cell;
	}

	void OpenRepublicApplication::QueueNativeConstruction(economy::Cell cell)
	{
		if (!CanQueueNativeCommand()) return;
		RefreshRegionEconomy();
		if (nativeConstructionMode == NativeConstructionMode::Building)
		{
			const auto* type = nativeEconomyCheckpoint.world->GetCatalog().BuildingType(nativePlacementType);
			if (!type) return;
			if (nativeTerrainEnabled && !nativeEconomyScene.CanPlaceFootprint(cell, type->width, type->depth))
			{
				uiNotice = "Placement rejected: terrain gradient exceeds the synthetic 0.22 rise/run limit or the footprint intersects the lake.";
				UpdateRegionStatus(); return;
			}
			// A type with a synthetic `build ... work N` declaration starts as a construction site that
			// needs material hauls; other types keep the instant placement path (OpenRepublic policy).
			const bool asSite = type->constructionWorkTicks > 0;
			const auto submission = asSite
				? regionEconomyApi->PlaceConstructionSite(regionEconomySnapshot->revision, type->name, cell)
				: regionEconomyApi->PlaceBuilding(regionEconomySnapshot->revision, type->name, cell);
			if (TrackNativeSubmission(submission,
				std::string(asSite ? "construction site " : "") + type->name + " at (" + std::to_string(cell.x) + ", " + std::to_string(cell.z) + ")"))
			{
				nativeConstructionMode = NativeConstructionMode::None;
				ClearNativeConstructionPreview();
			}
		}
		else if (nativeConstructionMode == NativeConstructionMode::Road)
		{
			if (!nativeRoadStart)
			{
				nativeRoadStart = cell;
				uiNotice = "Road start set; choose an endpoint on the same row or column.";
			}
			else
			{
				const auto start = *nativeRoadStart;
				const auto dx = static_cast<std::int64_t>(cell.x) - start.x;
				const auto dz = static_cast<std::int64_t>(cell.z) - start.z;
				const auto length = std::abs(dx) + std::abs(dz);
				if ((dx != 0 && dz != 0) || length == 0 || length > 64)
				{
					uiNotice = "Choose a distinct endpoint on the same row or column, at most 64 cells away.";
				}
				else if (static_cast<std::size_t>(length) > kMaxPendingRegionCommands - regionPendingCommandHandles.size())
				{
					uiNotice = "This road exceeds the remaining pending-command budget. Step first or choose a shorter road.";
				}
				else
				{
					economy::Cell a = start;
					std::size_t queued = 0;
					std::string submissionIssue;
					if (nativeTerrainEnabled)
					{
						economy::Cell probe = start;
						for (std::int64_t i = 0; i <= length; ++i)
						{
							if (!nativeEconomyScene.CanPlaceFootprint(probe, 1, 1))
							{
								submissionIssue = "road rejected: terrain gradient exceeds the synthetic 0.22 rise/run limit or intersects the lake";
								break;
							}
							probe.x += dx > 0 ? 1 : dx < 0 ? -1 : 0;
							probe.z += dz > 0 ? 1 : dz < 0 ? -1 : 0;
						}
					}
					for (std::int64_t i = 0; i < length; ++i)
					{
						if (!submissionIssue.empty()) break;
						const economy::Cell b{ a.x + (dx > 0 ? 1 : dx < 0 ? -1 : 0), a.z + (dz > 0 ? 1 : dz < 0 ? -1 : 0) };
						if (!TrackNativeSubmission(regionEconomyApi->BuildRoad(regionEconomySnapshot->revision, a, b), "road segment"))
						{
							submissionIssue = uiNotice;
							break;
						}
						++queued;
						a = b;
					}
					uiNotice = "Queued " + std::to_string(queued) + "/" + std::to_string(length) +
						" road segments; each validates independently on step.";
					if (!submissionIssue.empty())
					{
						nativeRoadStart = a;
						uiNotice += " " + submissionIssue + " Remaining draft starts at (" +
							std::to_string(a.x) + ", " + std::to_string(a.z) + ").";
					}
					if (queued == static_cast<std::size_t>(length))
					{
						nativeConstructionMode = NativeConstructionMode::None;
						nativeRoadStart.reset();
						ClearNativeConstructionPreview();
					}
				}
			}
		}
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::ClearNativeConstructionPreview()
	{
		if (nativeConstructionPreview != wi::ecs::INVALID_ENTITY) previewWorld.Entity_Remove(nativeConstructionPreview);
		nativeConstructionPreview = wi::ecs::INVALID_ENTITY;
		nativeHoverCell.reset();
	}

	void OpenRepublicApplication::UpdateNativeConstructionPreview(bool pointerAvailable)
	{
		if (!pointerAvailable || nativeConstructionMode == NativeConstructionMode::None)
		{
			ClearNativeConstructionPreview();
			return;
		}
		const auto cell = NativeCellAtPointer();
		if (!cell) { ClearNativeConstructionPreview(); return; }
		nativeHoverCell = cell;
		float width = 4.0f, depth = 4.0f;
		const auto center = nativeEconomyScene.CellPosition(*cell);
		float x = center.x, z = center.z;
		bool roadDraftValid = false;
		if (nativeConstructionMode == NativeConstructionMode::Road && nativeRoadStart) {
			const auto dx = static_cast<std::int64_t>(cell->x) - nativeRoadStart->x;
			const auto dz = static_cast<std::int64_t>(cell->z) - nativeRoadStart->z;
			const auto length = std::abs(dx) + std::abs(dz);
			roadDraftValid = (dx == 0 || dz == 0) && length > 0 && length <= 64;
			width = 4.0f * static_cast<float>(std::max<std::int64_t>(1, std::abs(dx) + (dx != 0 ? 1 : 0)));
			depth = 4.0f * static_cast<float>(std::max<std::int64_t>(1, std::abs(dz) + (dz != 0 ? 1 : 0)));
			const auto startCenter = nativeEconomyScene.CellPosition(*nativeRoadStart);
			x = (center.x + startCenter.x) * 0.5f;
			z = (center.z + startCenter.z) * 0.5f;
		}
		if (nativeConstructionMode == NativeConstructionMode::Building)
		{
			const auto* type = nativeEconomyCheckpoint.world->GetCatalog().BuildingType(nativePlacementType);
			if (!type) { ClearNativeConstructionPreview(); return; }
			width *= type->width; depth *= type->depth;
			x += (width - 4.0f) * 0.5f; z += (depth - 4.0f) * 0.5f;
		}
		const std::uint32_t footprintWidth = nativeConstructionMode == NativeConstructionMode::Building
			? nativeEconomyCheckpoint.world->GetCatalog().BuildingType(nativePlacementType)->width : 1u;
		const std::uint32_t footprintDepth = nativeConstructionMode == NativeConstructionMode::Building
			? nativeEconomyCheckpoint.world->GetCatalog().BuildingType(nativePlacementType)->depth : 1u;
		const bool terrainValid = nativeEconomyScene.CanPlaceFootprint(*cell, footprintWidth, footprintDepth);
		if (nativeConstructionPreview == wi::ecs::INVALID_ENTITY)
		{
			nativeConstructionPreview = previewWorld.Entity_CreateCube("Native construction draft");
		}
		if (auto* material = previewWorld.materials.GetComponent(nativeConstructionPreview))
		{
			bool valid = terrainValid;
			if (nativeConstructionMode == NativeConstructionMode::Building)
				valid = valid && runtime::ValidateNativeBuildingPlacement(nativeEconomyCheckpoint.world->Snapshot(),
					nativeEconomyCheckpoint.world->GetCatalog(), nativePlacementType, *cell).Valid();
			else if (nativeConstructionMode == NativeConstructionMode::Road) valid = valid && (roadDraftValid || !nativeRoadStart);
			material->SetBaseColor(valid ? XMFLOAT4(0.10f, 0.90f, 0.25f, 0.42f) : XMFLOAT4(0.95f, 0.12f, 0.08f, 0.48f));
			material->CreateRenderData(true);
		}
		if (auto* transform = previewWorld.transforms.GetComponent(nativeConstructionPreview))
		{
			transform->scale_local = XMFLOAT3(width * 0.5f, 0.08f, depth * 0.5f);
			transform->translation_local = XMFLOAT3(x,
				static_cast<float>(nativeEconomyScene.GroundHeight(x, z)) + 0.12f, z);
			transform->UpdateTransform();
		}
	}

	void OpenRepublicApplication::FrameNativeEconomyWorld()
	{
		ClearNativeConstructionPreview();
		if (!nativeEconomyCheckpointMode || !regionEconomyApi || !renderPath) return;
		const auto selectedId = selectedRegionBuildingId;
		RefreshRegionEconomy();
		std::string error;
		if (!nativeEconomyScene.Rebase(previewWorld, nativeEconomyCheckpoint.world->Snapshot(),
			*nativeEconomyCheckpoint.catalog, regionInstances, error))
		{
			nativeEconomyPresentationValid = false;
			uiNotice = "Could not frame native world: " + error;
			UpdateRegionStatus();
			return;
		}
		nativeEconomyPresentationValid = true;
		lastPresentedRegionRevision.reset();
		UpdateRegionEconomyPresentation();
		selectedRegionBuildingEntity = wi::ecs::INVALID_ENTITY;
		selectedRegionBuildingId.reset();
		for (const auto& instance : regionInstances)
		{
			if (selectedId && nativeEconomyScene.BuildingForEntity(instance.object) == selectedId)
			{
				selectedRegionBuildingEntity = instance.object;
				selectedRegionBuildingId = selectedId;
				break;
			}
		}
		const auto target = nativeEconomyScene.CameraTarget();
		const float distance = nativeEconomyScene.CameraDistance() * (nativeCoalDemo ? 0.68f : 1.0f);
		wi::scene::TransformComponent cameraTransform;
		cameraTransform.Translate(XMFLOAT3(target.x, target.y + distance * 0.55f, target.z - distance * 0.85f));
		cameraTransform.RotateRollPitchYaw(XMFLOAT3(0.6f, 0, 0));
		cameraTransform.UpdateTransform();
		renderPath->camera->TransformCamera(cameraTransform);
		renderPath->camera->zNearP = std::max(0.01f, distance / 100000.0f);
		renderPath->camera->zFarP = std::max(1000.0f, distance * 4.0f);
		renderPath->camera->UpdateCamera();
		cameraSpeedScale = std::max(1.0f, distance / 15.0f);
		uiNotice = "Framed the current native world without advancing the simulation.";
		UpdateRegionStatus();
	}

	bool OpenRepublicApplication::QueueRegionStep(std::uint32_t ticks, bool automatic)
	{
		const std::string tickLabel = std::to_string(ticks) + (ticks == 1 ? " tick" : " ticks");
		if (economyOverlay)
		{
			const bool queued = economyOverlay->QueueStep(ticks);
			uiNotice = queued
				? "Queued a " + tickLabel + " advance for the shared economy world."
				: "Could not queue the economy step; check the bridge connection and controller role.";
			UpdateRegionStatus();
			return queued;
		}
		else if (regionEconomyApi)
		{
			const bool hadPending = !regionPendingCommandHandles.empty();
			const auto error = regionEconomyApi->Step(ticks);
			if (error == openrepublic::economy_api::ErrorCode::None)
			{
				RefreshRegionEconomy();
				UpdateRegionEconomyPresentation();
				// Automatic ticks retain outcome feedback until the next human action.
				if (!automatic)
					uiNotice = "Advanced the app-owned economy by " + tickLabel + "." +
						(hadPending ? " " + uiNotice : "");
				UpdateRegionStatus();
				return true;
			}
			else
			{
				uiNotice = "App-owned EconomyApi rejected the " + tickLabel + " step with error " +
					std::to_string(static_cast<unsigned>(error)) + ".";
				UpdateRegionStatus();
				return false;
			}
		}
		UpdateRegionStatus();
		return false;
	}

	void OpenRepublicApplication::QueueRegionBuildingToggle()
	{
		if ((!economyOverlay && !regionEconomyApi) || !selectedRegionBuildingId.has_value())
		{
			uiNotice = "Select an imported building before toggling it.";
			UpdateRegionStatus();
			return;
		}
		if (economyOverlay)
		{
			uiNotice = economyOverlay->QueueToggleBuilding(*selectedRegionBuildingId)
				? "Queued enable/disable for the selected building. Advance the shared economy world to apply it."
				: "Could not queue the building change; refresh the world and check the controller role.";
		}
		else
		{
			if (regionPendingCommandHandles.size() >= kMaxPendingRegionCommands)
			{
				uiNotice = "Pending economy command limit reached; advance the world before queuing more.";
				UpdateRegionStatus();
				return;
			}
			const auto selected = GetRegionBuildingForInstance(selectedRegionBuildingEntity);
			const auto submitted = selected.has_value() && regionEconomySnapshot.has_value()
				? regionEconomyApi->SetBuildingEnabled(regionEconomySnapshot->revision,
					static_cast<openrepublic::economy::BuildingId>(*selectedRegionBuildingId), !selected->enabled)
				: openrepublic::economy_api::Submission{ openrepublic::economy_api::ErrorCode::InvalidArgument };
			if (submitted.error == openrepublic::economy_api::ErrorCode::None)
			{
				regionPendingCommandHandles.push_back(submitted.handle);
				uiNotice = "Queued the selected building state change; step the world to apply it.";
			}
			else
			{
				uiNotice = "Could not queue the selected building state change.";
			}
		}
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::SetRegionHaulSource()
	{
		if (!selectedRegionBuildingId.has_value())
		{
			uiNotice = "Select a source building before setting a haul source.";
			UpdateRegionStatus();
			return;
		}
		regionHaulSourceId = selectedRegionBuildingId;
		uiNotice = "Selected building saved as the haul source.";
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::QueueRegionHaul()
	{
		if (nativeToolsReady && ActiveOwnedEconomyWorld())
		{
			RefreshNativeHaulChoices();
			auto* world = ActiveOwnedEconomyWorld();
			const auto& catalog = world->GetCatalog();
			const auto* resource = catalog.Resource(nativeEconomyTools.SelectedResource());
			const auto* vehicle = catalog.VehicleType(nativeEconomyTools.SelectedVehicleType());
			if (!regionHaulSourceId || !selectedRegionBuildingId || !resource || !vehicle)
			{
				uiNotice = "Set a source, select a different destination, then choose a shared resource and vehicle.";
				UpdateRegionStatus();
				return;
			}
			if (!CanQueueNativeCommand()) return;
			RefreshRegionEconomy();
			TrackNativeSubmission(regionEconomyApi->CreateHaulOrder(regionEconomySnapshot->revision,
				static_cast<economy::BuildingId>(*regionHaulSourceId),
				static_cast<economy::BuildingId>(*selectedRegionBuildingId), resource->name, vehicle->name),
				resource->name + " haul using " + vehicle->name);
			UpdateRegionStatus();
			return;
		}
		if ((!economyOverlay && !regionEconomyApi) || !regionHaulSourceId.has_value() ||
			!selectedRegionBuildingId.has_value())
		{
			uiNotice = "Choose a haul source and a destination building first.";
			UpdateRegionStatus();
			return;
		}
		if (*regionHaulSourceId == *selectedRegionBuildingId)
		{
			uiNotice = "The haul source and destination must be different buildings.";
			UpdateRegionStatus();
			return;
		}
		if (economyOverlay)
		{
			uiNotice = economyOverlay->QueueHaulOrder(*regionHaulSourceId, *selectedRegionBuildingId)
				? "Queued a haul order using the first compatible resource and vehicle. Advance the shared economy world to apply it."
				: "No compatible resource/vehicle was found or the bridge rejected the haul request.";
		}
		else if (regionEconomySnapshot.has_value())
		{
			const auto findBuilding = [&](std::uint64_t id) -> const openrepublic::economy_api::BuildingView* {
				const auto found = std::find_if(regionEconomySnapshot->buildings.begin(),
					regionEconomySnapshot->buildings.end(), [&](const auto& building) { return building.id == id; });
				return found == regionEconomySnapshot->buildings.end() ? nullptr : &*found;
			};
			const auto* source = findBuilding(*regionHaulSourceId);
			const auto* destination = findBuilding(*selectedRegionBuildingId);
			const auto catalog = regionEconomyApi->DescribeCatalog();
			const auto findType = [&](const std::string& name) -> const openrepublic::economy_api::BuildingTypeInfo* {
				const auto found = std::find_if(catalog.buildingTypes.begin(), catalog.buildingTypes.end(),
					[&](const auto& type) { return type.name == name; });
				return found == catalog.buildingTypes.end() ? nullptr : &*found;
			};
			const auto* sourceType = source ? findType(source->type) : nullptr;
			const auto* destinationType = destination ? findType(destination->type) : nullptr;
			std::string compatibleResource;
			if (sourceType != nullptr && destinationType != nullptr)
			{
				for (const auto& sourceRecipe : sourceType->recipes)
				{
					for (const auto& output : sourceRecipe.outputs)
					{
						const bool destinationConsumes = std::any_of(destinationType->recipes.begin(),
							destinationType->recipes.end(), [&](const auto& destinationRecipe) {
								return std::any_of(destinationRecipe.inputs.begin(), destinationRecipe.inputs.end(),
									[&](const auto& input) { return input.resource == output.resource; });
							});
						if (destinationConsumes)
						{
							compatibleResource = output.resource;
							break;
						}
					}
					if (!compatibleResource.empty()) break;
				}
			}
			if (!compatibleResource.empty() && !catalog.vehicleTypes.empty())
			{
				if (regionPendingCommandHandles.size() >= kMaxPendingRegionCommands)
				{
					uiNotice = "Pending economy command limit reached; advance the world before queuing more.";
					UpdateRegionStatus();
					return;
				}
				const auto& vehicle = catalog.vehicleTypes.front();
				const auto submitted = regionEconomyApi->CreateHaulOrder(regionEconomySnapshot->revision,
					static_cast<openrepublic::economy::BuildingId>(*regionHaulSourceId),
					static_cast<openrepublic::economy::BuildingId>(*selectedRegionBuildingId),
					compatibleResource, vehicle.name);
				if (submitted.error == openrepublic::economy_api::ErrorCode::None)
				{
					regionPendingCommandHandles.push_back(submitted.handle);
					uiNotice = "Queued " + compatibleResource + " haul using " + vehicle.name +
						"; step the world to apply it.";
				}
				else
				{
					uiNotice = "EconomyApi rejected the haul command.";
				}
			}
			else
			{
				uiNotice = "The selected building types have no producer-to-consumer recipe or no vehicle type.";
			}
		}
		UpdateRegionStatus();
	}

	void OpenRepublicApplication::AssignTruckMillEndpoint()
	{
		const auto id = GetSelectedSimulationId();
		const auto snapshot = QuerySimulationSnapshot();
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
		const auto snapshot = QuerySimulationSnapshot();
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

	void OpenRepublicApplication::SelectNextStorage()
	{
		std::vector<wi::ecs::Entity> storageEntities;
		storageEntities.reserve(prototypeStorageEntities.size() + 1);
		if (warehouseBuilding != wi::ecs::INVALID_ENTITY && !GetOpenRepublicId(warehouseBuilding).empty())
		{
			storageEntities.push_back(warehouseBuilding);
		}
		for (const wi::ecs::Entity entity : prototypeStorageEntities)
		{
			if (entity != wi::ecs::INVALID_ENTITY && !GetOpenRepublicId(entity).empty())
			{
				storageEntities.push_back(entity);
			}
		}
		if (storageEntities.empty())
		{
			uiNotice = "No storage entities are available to select.";
			UpdateStatusLabel();
			return;
		}

		const auto current = std::find(storageEntities.begin(), storageEntities.end(), selectedBuilding);
		const auto next = current == storageEntities.end() || std::next(current) == storageEntities.end()
			? storageEntities.begin() : std::next(current);
		if (*next == wi::ecs::INVALID_ENTITY)
		{
			uiNotice = "The next storage entity is not available in the scene.";
			UpdateStatusLabel();
			return;
		}

		uiNotice = "Selected storage " + GetOpenRepublicId(*next) + " from the storage list.";
		SelectBuilding(*next);
	}

	void OpenRepublicApplication::QueueSelectedTruckPlan()
	{
		const auto snapshot = QuerySimulationSnapshot();
		if (!snapshot.pendingTruckPlans.empty())
		{
			uiNotice = "Truck plan " + PendingTruckPlanCommandLabel(agentApi,
				snapshot.pendingTruckPlans.front().commandId) +
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
		const auto submission = agentApi.AssignTransportPlan(QuerySimulationSnapshot().stateRevision, truckPlan);
		if (submission.error != openrepublic::agent_api::ErrorCode::None)
		{
			uiNotice = "Truck plan could not be queued; the bounded command queue may be full or the simulation may be at its terminal tick.";
		}
		else
		{
			uiNotice = "Queued truck plan API command " + std::to_string(submission.commandId) + " (Farm OR-" +
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
		if (agentApi.SetRoadEnabled(QuerySimulationSnapshot().stateRevision, 202, requestedState) ==
			openrepublic::agent_api::ErrorCode::None)
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
		uiNotice = "Place synthetic storage: click a valid construction grid cell. One placement will queue for validation at the next fixed simulation tick, then return to selection mode; Esc or Cancel build exits placement.";
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
		if (nativeEconomyCheckpointMode && nativeConstructionMode != NativeConstructionMode::None)
		{
			const auto cell = NativeCellAtPointer();
			if (cell) QueueNativeConstruction(*cell);
			else { uiNotice = "No displayable native ground cell at this pointer. Adjust the camera or Frame world."; UpdateRegionStatus(); }
			return;
		}
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
			const auto submission = agentApi.AddRoadSegment(QuerySimulationSnapshot().stateRevision,
				startX, startZ, gridX, gridZ);
			if (submission.error != openrepublic::agent_api::ErrorCode::None)
			{
				uiNotice = "Road segment could not be queued; the bounded construction queue may be full.";
			}
			else
			{
				uiNotice = "Queued synthetic road segment command " + std::to_string(submission.commandId) + " from (" +
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
			const auto submission = agentApi.PlaceFacility(QuerySimulationSnapshot().stateRevision,
				*activeFacilityPlacement, gridX, gridZ);
			if (submission.error != openrepublic::agent_api::ErrorCode::None)
			{
				uiNotice = "Could not queue construction; the simulation queue may be full or at its terminal tick.";
			}
			else
			{
				uiNotice = "Queued synthetic facility placement command " + std::to_string(submission.commandId) +
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
			const auto submission = agentApi.PlaceStorage(QuerySimulationSnapshot().stateRevision, gridX, gridZ);
			if (submission.error != openrepublic::agent_api::ErrorCode::None)
			{
				uiNotice = "Could not queue storage placement; the bounded construction queue may be full or at its terminal tick.";
			}
			else
			{
				uiNotice = "Queued storage placement command " + std::to_string(submission.commandId) + " at grid (" +
					std::to_string(gridX) + ", " + std::to_string(gridZ) + "); occupancy and capacity are validated at the next fixed tick.";
				// Storage placement is a single-cell action. Return to selection mode
				// after one queued placement so the next world click cannot accidentally
				// place another storage where the player intended to select this one.
				activeStoragePlacement = false;
				placeStorageButton.SetText("Place storage");
			}
			UpdateStatusLabel();
			return;
		}
		const wi::scene::Scene::RayIntersectionResult hit = previewWorld.Intersects(ray);
		if (mapRegionPreview && (economyOverlay || regionEconomyApi))
		{
			selectedRegionBuildingEntity = hit.entity;
			const auto selected = GetRegionBuildingForInstance(selectedRegionBuildingEntity);
			selectedRegionBuildingId = selected.has_value()
				? std::optional<std::uint64_t>(selected->id) : std::nullopt;
			if (selected.has_value())
			{
				uiNotice = std::string(nativeEconomyCheckpointMode ? "Selected native building OR-" : "Selected imported building OR-") +
					std::to_string(selected->id) + " " + selected->type + ".";
			}
			else
			{
				selectedRegionBuildingEntity = wi::ecs::INVALID_ENTITY;
				uiNotice = "No economy building is bound to the clicked map object.";
			}
			UpdateRegionStatus();
			return;
		}
		SelectBuilding(hit.entity);
	}

	void OpenRepublicApplication::UpdateSimulationPresentation()
	{
		const openrepublic::sim::Snapshot snapshot = QuerySimulationSnapshot();
		SyncPrototypeStorages(snapshot);
		SyncFacilities(snapshot);
		SyncRoads(snapshot);
		std::uint64_t eventCursor = lastAgentEventSequence;
		while (true)
		{
			const std::uint64_t previousCursor = eventCursor;
			const auto eventPage = agentApi.ReadEvents(eventCursor, openrepublic::agent_api::MaxEventPageSize);
			if (eventPage.error != openrepublic::agent_api::ErrorCode::None) break;
			for (const auto& event : eventPage.page.events)
			{
				eventCursor = event.sequence;
				if (event.type != openrepublic::agent_api::EventType::CommandApplied &&
					event.type != openrepublic::agent_api::EventType::CommandRejected)
				{
					continue;
				}

				const auto result = agentApi.GetCommandResult(event.commandId);
				if (result.error == openrepublic::agent_api::ErrorCode::UnknownCommand) continue;
				const bool accepted = result.state == openrepublic::agent_api::CommandState::Applied;
				if (result.type == openrepublic::sim::ConstructionCommandType::RoadSegment)
				{
					uiNotice = accepted
						? "Road construction command " + std::to_string(result.commandId) + " accepted as edge " + std::to_string(result.entityId) + "."
						: "Road construction command " + std::to_string(result.commandId) + " rejected by road graph validation.";
				}
				else if (result.type == openrepublic::sim::ConstructionCommandType::StoragePlacement)
				{
					uiNotice = accepted
						? "Storage placement command " + std::to_string(result.commandId) + " accepted as OR-" + std::to_string(result.entityId) + "."
						: "Storage placement command " + std::to_string(result.commandId) + " rejected: occupied cell or storage limit.";
				}
				else if (result.type == openrepublic::sim::ConstructionCommandType::TruckPlan)
				{
					uiNotice = accepted
						? "Truck plan command " + std::to_string(result.commandId) + " accepted; the selected route is now active."
						: "Truck plan command " + std::to_string(result.commandId) + " rejected: invalid endpoint, disconnected graph, or truck still active on its previous plan.";
				}
				else
				{
					uiNotice = accepted
						? "Construction command " + std::to_string(result.commandId) + " accepted as OR-" + std::to_string(result.entityId) + "."
						: "Construction command " + std::to_string(result.commandId) + " rejected: occupied cell or facility limit.";
				}
				agentApi.AcknowledgeCommandResult(result.commandId);
			}
			if (!eventPage.page.hasMore || eventPage.page.nextCursor == previousCursor) break;
			eventCursor = eventPage.page.nextCursor;
		}
		lastAgentEventSequence = eventCursor;
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
		std::vector<std::string> snapshotIds;
		snapshotIds.reserve(snapshot.prototypeStorages.size());
		for (const auto& storage : snapshot.prototypeStorages)
		{
			snapshotIds.push_back("OR-" + std::to_string(storage.id));
		}

		if (prototypeStorageIds != snapshotIds)
		{
			const std::string previouslySelectedId = GetOpenRepublicId(selectedBuilding);
			const bool selectedStorage = std::find(prototypeStorageIds.begin(), prototypeStorageIds.end(), previouslySelectedId) !=
				prototypeStorageIds.end();
			const bool preserveSelection = selectedStorage &&
				std::find(snapshotIds.begin(), snapshotIds.end(), previouslySelectedId) != snapshotIds.end();
			if (selectedStorage)
			{
				SelectBuilding(wi::ecs::INVALID_ENTITY);
			}

			for (const wi::ecs::Entity entity : prototypeStorageEntities)
			{
				previewWorld.Entity_Remove(entity);
			}
			prototypeStorageEntities.clear();
			prototypeStorageIds.clear();

			for (const auto& storage : snapshot.prototypeStorages)
			{
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

			if (preserveSelection)
			{
				const auto restored = std::find(prototypeStorageIds.begin(), prototypeStorageIds.end(), previouslySelectedId);
				if (restored != prototypeStorageIds.end())
				{
					const std::size_t index = static_cast<std::size_t>(std::distance(prototypeStorageIds.begin(), restored));
					SelectBuilding(prototypeStorageEntities[index]);
				}
			}
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
		std::string previouslySelectedId;
		bool preserveSelection = false;
		for (std::size_t i = 0; !identityChanged && i < snapshot.facilities.size(); ++i)
		{
			if (i < firstDynamicFacility) continue;
			identityChanged = facilityIds[i - firstDynamicFacility] != "OR-" + std::to_string(snapshot.facilities[i].id);
		}
		if (identityChanged)
		{
			previouslySelectedId = GetOpenRepublicId(selectedBuilding);
			const bool selectedDynamicFacility = std::find(facilityIds.begin(), facilityIds.end(), previouslySelectedId) !=
				facilityIds.end();
			preserveSelection = selectedDynamicFacility &&
				std::find_if(snapshot.facilities.begin() + firstDynamicFacility, snapshot.facilities.end(),
					[&](const auto& facility) { return "OR-" + std::to_string(facility.id) == previouslySelectedId; }) != snapshot.facilities.end();
			if (selectedDynamicFacility)
			{
				SelectBuilding(wi::ecs::INVALID_ENTITY);
			}
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
		if (identityChanged && preserveSelection)
		{
			const auto match = std::find(facilityIds.begin(), facilityIds.end(), previouslySelectedId);
			if (match != facilityIds.end())
			{
				const std::size_t index = static_cast<std::size_t>(std::distance(facilityIds.begin(), match));
				SelectBuilding(facilityEntities[index]);
			}
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

		const auto saveQuery = agentApi.SaveWorld();
		if (saveQuery.error != openrepublic::agent_api::ErrorCode::None)
		{
			output.close();
			std::error_code removeError;
			std::filesystem::remove(temporaryPath, removeError);
			uiNotice = "The Agent API denied the simulation save request.";
			UpdateStatusLabel();
			return;
		}
		const std::string& saveData = saveQuery.data;
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
		const auto loadError = agentApi.LoadWorld(QuerySimulationSnapshot().stateRevision, data.str(), &error);
		if (loadError != openrepublic::agent_api::ErrorCode::None)
		{
			uiNotice = loadError == openrepublic::agent_api::ErrorCode::StaleRevision
				? "Save rejected because the simulation changed while loading."
				: "Save rejected: " + error;
			UpdateStatusLabel();
			return;
		}

		pendingRules.reset();
		lastAgentEventSequence = 0;
		pendingMillApproachRoadState.reset();
		truckPlan = QuerySimulationSnapshot().truckPlan;
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

	openrepublic::sim::Snapshot OpenRepublicApplication::QuerySimulationSnapshot() const
	{
		return agentApi.QuerySnapshot().snapshot;
	}

	void OpenRepublicApplication::ReloadLuaRules()
	{
		#ifdef OPENREPUBLIC_WITH_LUA_RULES
		// A reload request supersedes any candidate queued by an earlier request.
		// Clear it before parsing so an invalid replacement cannot later apply a
		// stale candidate after the user has been told the active rules were kept.
		pendingRules.reset();
		openrepublic::sim::SimulationRules candidate = QuerySimulationSnapshot().rules;
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
			if (agentApi.SetSimulationRules(QuerySimulationSnapshot().stateRevision, candidate, &error) ==
				openrepublic::agent_api::ErrorCode::None)
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
		if (agentApi.SetSimulationRules(QuerySimulationSnapshot().stateRevision, *pendingRules, &error) ==
			openrepublic::agent_api::ErrorCode::None)
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
		if (mapRegionPreview)
		{
			return;
		}
		const openrepublic::sim::Snapshot snapshot = QuerySimulationSnapshot();
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
			text << "\nPending truck plan " << PendingTruckPlanCommandLabel(agentApi, pending.commandId)
				<< ": Farm OR-" << pending.plan.farmId
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
