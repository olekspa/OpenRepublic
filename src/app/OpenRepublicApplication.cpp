#include "OpenRepublicApplication.h"
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

#include <algorithm>
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
			if (!compat::ImportCatalogFromDefinitions(definitions, {}, catalog, catalogReport))
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
					static_cast<std::uint64_t>(index) });
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

			std::ostringstream summary;
			summary << "App-owned region economy (experimental): "
				<< compat::DescribeImport(catalogReport) << " "
				<< compat::DescribeRegionImport(importReport) << " "
				<< compat::DescribeRegionRoads(roadReport);
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
		bool regionPreview, std::filesystem::path contentRoot, int regionModelLevel)
	{
		mapPlacement = std::move(request);
		mapRadiusMeters = radiusMeters;
		mapRegionPreview = regionPreview;
		mapRegionModelLevel = regionModelLevel;
		mapContentRoot = std::move(contentRoot);
	}

	void OpenRepublicApplication::SetSyntheticRegionFixture()
	{
		syntheticRegionFixtureMode = true;
		mapRegionPreview = true;
	}

	void OpenRepublicApplication::SetMapShowcaseCapture(std::filesystem::path outputPath, bool enhancedLook,
		float cameraYawDegrees, float cameraElevationDegrees, float cameraDistanceScale)
	{
		mapShowcaseCaptureMode = !outputPath.empty();
		mapShowcaseCapturePath = std::move(outputPath);
		mapShowcaseEnhancedLook = enhancedLook;
		mapShowcaseCameraYawDegrees = cameraYawDegrees;
		mapShowcaseCameraElevationDegrees = cameraElevationDegrees;
		mapShowcaseCameraDistanceScale = cameraDistanceScale;
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

	void OpenRepublicApplication::SetMapShowcaseSun(float elevationDegrees, float yawOffsetDegrees)
	{
		mapShowcaseSunElevationDegrees = elevationDegrees;
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
		if (syntheticRegionFixtureMode)
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
				light->SetCastShadow(true);
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
						description, error, &regionInstances, &showcaseEnvironment);
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
							if (BuildAppOwnedRegionEconomy(*mapPlacement, mapContentRoot, effectiveRadius,
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
						weather.sunColor = previewSunColor;
						if (showcaseEnvironment.hasWater)
						{
							weather.SetOceanEnabled(true);
							weather.oceanParameters.waterHeight = showcaseEnvironment.waterLevel;
							weather.oceanParameters.wave_amplitude = 2.0f;
							weather.oceanParameters.wind_speed = 60.0f;
							weather.oceanParameters.patch_length = 140.0f;
							weather.oceanParameters.choppy_scale = 0.8f;
							weather.oceanParameters.waterColor = XMFLOAT4(0.02f, 0.10f, 0.12f, 0.85f);
							weather.oceanParameters.extinctionColor = XMFLOAT4(0.10f, 0.55f, 0.65f, 1.0f);
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
		if (mapShowcaseCaptureMode && mapShowcaseEnhancedLook)
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
			renderPath->setExposure(1.5f);
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
				cameraTransform.Translate(XMFLOAT3(nmfPreviewCameraTarget.x,
					nmfPreviewCameraTarget.y + nmfPreviewCameraDistance * 0.55f,
					nmfPreviewCameraTarget.z - nmfPreviewCameraDistance * 0.85f));
				cameraTransform.RotateRollPitchYaw(XMFLOAT3(0.6f, 0, 0));
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
				regionStepButton.Create("RegionEconomyStepButton");
				regionStepButton.SetText("Step 24 ticks");
				StyleButton(regionStepButton);
				regionStepButton.SetSize(XMFLOAT2(132, 28));
				regionStepButton.SetPos(XMFLOAT2(18, 18));
				regionStepButton.OnClick([this](const wi::gui::EventArgs&) { QueueRegionStep(); });
				gui.AddWidget(&regionStepButton);

				regionToggleButton.Create("RegionEconomyToggleBuildingButton");
				regionToggleButton.SetText("Toggle selected");
				StyleButton(regionToggleButton);
				regionToggleButton.SetSize(XMFLOAT2(132, 28));
				regionToggleButton.SetPos(XMFLOAT2(156, 18));
				regionToggleButton.OnClick([this](const wi::gui::EventArgs&) { QueueRegionBuildingToggle(); });
				gui.AddWidget(&regionToggleButton);

				regionHaulSourceButton.Create("RegionEconomyHaulSourceButton");
				regionHaulSourceButton.SetText("Set haul source");
				StyleButton(regionHaulSourceButton);
				regionHaulSourceButton.SetSize(XMFLOAT2(132, 28));
				regionHaulSourceButton.SetPos(XMFLOAT2(294, 18));
				regionHaulSourceButton.OnClick([this](const wi::gui::EventArgs&) { SetRegionHaulSource(); });
				gui.AddWidget(&regionHaulSourceButton);

				regionHaulButton.Create("RegionEconomyHaulToSelectedButton");
				regionHaulButton.SetText("Haul to selected");
				StyleButton(regionHaulButton);
				regionHaulButton.SetSize(XMFLOAT2(132, 28));
				regionHaulButton.SetPos(XMFLOAT2(432, 18));
				regionHaulButton.OnClick([this](const wi::gui::EventArgs&) { QueueRegionHaul(); });
				gui.AddWidget(&regionHaulButton);
			}
			simulationStatus.SetPos(XMFLOAT2(18, 54));
			simulationStatus.SetSize(XMFLOAT2(1100, 96));
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
		wi::Application::Update(dt);
		if (nmfCaptureMode || mapShowcaseCaptureMode)
		{
			return;
		}
		PumpLocalEconomyRequests();
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
		else if (regionEconomyApi && (mapPlacement || syntheticRegionFixtureMode))
		{
			UpdateRegionEconomyPresentation();
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
		// normal input guard for the actual buttons above it.
		const bool pointerOnStatusLabel = simulationStatus.IsVisible() &&
			pointerHitbox.intersects(simulationStatus.hitBox);
		const bool guiHasFocus = renderPath->GetGUI().HasFocus() && !pointerOnStatusLabel;
		if (!guiHasFocus && wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
		{
			if (mapRegionPreview)
			{
				selectedRegionBuildingEntity = wi::ecs::INVALID_ENTITY;
				selectedRegionBuildingId.reset();
				regionHaulSourceId.reset();
				UpdateRegionStatus();
			}
			else if (activeRoadPlacement) CancelRoadPlacement();
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

	void OpenRepublicApplication::UpdateRegionStatus()
	{
		if (!mapRegionPreview || (!economyOverlay && !regionEconomyApi))
		{
			return;
		}
		std::ostringstream text;
		text << regionWorldStatus;
		if (selectedRegionBuildingEntity != wi::ecs::INVALID_ENTITY)
		{
			const auto selected = GetRegionBuildingForInstance(selectedRegionBuildingEntity);
			if (selected.has_value())
			{
				selectedRegionBuildingId = selected->id;
				text << "\nSelected OR-" << selected->id << " " << selected->type
					<< " | " << (selected->enabled ? "enabled" : "disabled");
				text << " | " << selected->stallReason;
				if (!selected->stallExplanation.empty()) text << ": " << selected->stallExplanation;
				if (!selected->inventory.empty())
				{
					const auto formatUnits = [](std::uint64_t microUnits)
					{
						std::string formatted = std::to_string(microUnits / 1000000);
						const std::uint64_t fraction = microUnits % 1000000;
						if (fraction != 0)
						{
							std::string decimal = std::to_string(fraction + 1000000).substr(1);
							while (!decimal.empty() && decimal.back() == '0') decimal.pop_back();
							formatted += "." + decimal;
						}
						return formatted;
					};
					text << " | inventory (units) ";
					for (std::size_t i = 0; i < selected->inventory.size(); ++i)
					{
						if (i != 0) text << ", ";
						text << selected->inventory[i].resource << " " << formatUnits(selected->inventory[i].amount)
							<< "/" << formatUnits(selected->inventory[i].capacity);
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
		if (!uiNotice.empty()) text << "\n" << uiNotice;
		if (economyOverlay)
		{
			const std::string action = economyOverlay->GetActionStatus();
			if (!action.empty()) text << "\n" << action;
		}
		simulationStatus.SetText(text.str());
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
		const auto rendered = std::find_if(regionInstances.begin(), regionInstances.end(),
			[&](const auto& candidate) { return candidate.object == instance; });
		if (rendered == regionInstances.end())
		{
			return std::nullopt;
		}
		const auto identity = std::find_if(regionEconomyImport.placedSourceBuildings.begin(),
			regionEconomyImport.placedSourceBuildings.end(), [&](const auto& candidate) {
				return candidate.sourcePlacementKey == rendered->sourcePlacementIndex;
			});
		if (identity == regionEconomyImport.placedSourceBuildings.end())
		{
			return std::nullopt;
		}
		const auto building = std::find_if(regionEconomySnapshot->buildings.begin(),
			regionEconomySnapshot->buildings.end(), [&](const auto& candidate) {
				return candidate.id == identity->buildingId;
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

		for (auto it = regionPendingCommandHandles.begin(); it != regionPendingCommandHandles.end();)
		{
			const auto polled = regionEconomyApi->PollCommand(*it);
			if (polled.error != openrepublic::economy_api::ErrorCode::None)
			{
				uiNotice = "Economy command poll failed with API error " +
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
				uiNotice = "Economy command applied at tick " + std::to_string(regionEconomySnapshot->tick) + ".";
			}
			else
			{
				uiNotice = "Economy command rejected: " + polled.result.message;
			}
			it = regionPendingCommandHandles.erase(it);
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
			<< regionEconomyImport.placedSourceBuildings.size() << " exact placement bindings";
		regionWorldStatus = status.str();
	}

	void OpenRepublicApplication::UpdateRegionEconomyPresentation()
	{
		if ((!mapPlacement && !syntheticRegionFixtureMode) || !regionEconomySnapshot.has_value() ||
			(lastPresentedRegionRevision.has_value() &&
				*lastPresentedRegionRevision == regionEconomySnapshot->revision))
		{
			return;
		}
		lastPresentedRegionRevision = regionEconomySnapshot->revision;

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
			const auto source = buildingBySource.find(instance.sourcePlacementIndex);
			if (source == buildingBySource.end())
			{
				continue;
			}
			const auto state = stateByBuilding.find(source->second);
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
		while (regionTruckEntities.size() < regionEconomySnapshot->vehicles.size())
		{
			const wi::ecs::Entity truck = previewWorld.Entity_CreateCube("app-owned economy truck marker");
			if (wi::scene::MaterialComponent* material = previewWorld.materials.GetComponent(truck))
			{
				material->SetBaseColor(XMFLOAT4(1.0f, 0.2f, 0.1f, 1.0f));
				material->CreateRenderData(true);
			}
			regionTruckEntities.push_back(truck);
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
			const double worldX = regionEconomyImport.originX + (vehicle.cell.x + 0.5) * 4.0;
			const double worldZ = regionEconomyImport.originZ + (vehicle.cell.z + 0.5) * 4.0;
			double height = 0.0;
			if (mapPlacement)
			{
				openrepublic::content::SampleTerrainHeight(mapPlacement->heightmap, terrainTransform,
					std::min(std::max(worldX, terrainTransform.originX), terrainTransform.originX + terrainTransform.sizeX),
					std::min(std::max(worldZ, terrainTransform.originZ), terrainTransform.originZ + terrainTransform.sizeZ), height, nullptr);
			}
			marker->ClearTransform();
			marker->Scale(XMFLOAT3(3.0f, 3.0f, 3.0f));
			marker->Translate(XMFLOAT3(static_cast<float>(worldX - centerX),
				static_cast<float>(height) + 3.0f, static_cast<float>(worldZ - centerZ)));
			marker->UpdateTransform();
		}
	}

	void OpenRepublicApplication::QueueRegionStep()
	{
		if (economyOverlay)
		{
			uiNotice = economyOverlay->QueueStep(24)
				? "Queued a 24-tick advance for the shared economy world."
				: "Could not queue the economy step; check the bridge connection and controller role.";
		}
		else if (regionEconomyApi)
		{
			const auto error = regionEconomyApi->Step(24);
			if (error == openrepublic::economy_api::ErrorCode::None)
			{
				RefreshRegionEconomy();
				UpdateRegionEconomyPresentation();
				uiNotice = "Advanced the app-owned economy by 24 ticks.";
			}
			else
			{
				uiNotice = "App-owned EconomyApi rejected the 24-tick step with error " +
					std::to_string(static_cast<unsigned>(error)) + ".";
			}
		}
		UpdateRegionStatus();
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
				uiNotice = "Selected imported building OR-" + std::to_string(selected->id) + " " + selected->type + ".";
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
