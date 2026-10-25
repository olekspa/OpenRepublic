#include "OpenRepublicApplication.h"
#include "wiHelper.h"
#include "openrepublic/content/building_types.h"
#include "openrepublic/content/mtl_dependencies.h"
#include "openrepublic/runtime/graphics_settings.h"
#include "openrepublic/runtime/save_slots.h"
#include "openrepublic/economy/checkpoint.h"
#include "openrepublic/compat/storage_profile.h"
#include "openrepublic/compat/catalog_import.h"
#include "openrepublic/runtime/native_objectives.h"
#include "openrepublic/runtime/play_options.h"
#include "openrepublic/runtime/campaign.h"
#include "openrepublic/runtime/ui_script.h"
#include "json.hpp"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <memory>
#include <cstdio>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

// windows.h aliases FindResource to FindResourceW; preserve the economy API method name.
#ifdef FindResource
#undef FindResource
#endif

namespace
{
	openrepublic::OpenRepublicApplication application;

	// Run in the CRT's XI initializer range, before C++ global constructors.
	// Wicked Engine performs cwd-relative startup work before wWinMain is called.
	int SetStartupWorkingDirectory()
	{
		std::array<wchar_t, 32768> executablePath{};
		const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
		if (length == 0 || static_cast<std::size_t>(length) >= executablePath.size()) return 1;
		const auto directory = openrepublic::runtime::ResolveEngineWorkingDirectory(
			std::filesystem::path(executablePath.data(), executablePath.data() + length));
		return !directory.empty() && SetCurrentDirectoryW(directory.c_str()) ? 0 : 1;
	}

	using StartupInitializer = int (__cdecl*)();
	#pragma section(".CRT$XIAA", long, read)
	__declspec(allocate(".CRT$XIAA")) StartupInitializer setStartupWorkingDirectory = &SetStartupWorkingDirectory;

	bool IsStandaloneEconomyIncompatibleOption(std::wstring_view argument)
	{
		static constexpr std::wstring_view incompatible[] = {
			L"--scene-probe", L"--glb-probe", L"--nmf-probe", L"--content-root", L"--economy-url",
			L"--map-package", L"--map-building", L"--map-region", L"--synthetic-region-fixture",
			L"--map-radius", L"--map-lod", L"--map-lod-distances", L"--showcase-capture", L"--showcase-look",
			L"--showcase-camera", L"--showcase-wind", L"--showcase-grass", L"--scene-pipe-name", L"--showcase-sun", L"--building-type",
			L"--building-lod", L"--nmf-texture-root", L"--nmf-unlit-preview",
			L"--nmf-material-unlit-preview", L"--capture-preview", L"--capture-preview-zoom",
			L"--capture-preview-elevation", L"--terrain-probe", L"--terrain-transform", L"--storage-profile", L"--utility-resource" };
		return std::find(std::begin(incompatible), std::end(incompatible), argument) != std::end(incompatible);
	}

	// Pre-initialization failures use stderr only. Starting the engine's async
	// logger immediately before CRT teardown can race its worker initialization.
	int LogCheckpointStartupError(const std::string& error)
	{
		std::fprintf(stderr, "OpenRepublic economy checkpoint startup failed: %s\n", error.c_str());
		return 2;
	}

	int LogNativeEconomyStartupError(const std::string& error)
	{
		std::fprintf(stderr, "OpenRepublic native economy startup failed: %s\n", error.c_str());
		return 2;
	}

	bool ParseFiniteDouble(const wchar_t* text, double& value)
	{
		if (text == nullptr || *text == L'\0') return false;
		wchar_t* end = nullptr;
		errno = 0;
		const double parsed = std::wcstod(text, &end);
		if (end == text || *end != L'\0' || errno == ERANGE || !std::isfinite(parsed)) return false;
		value = parsed;
		return true;
	}

	std::optional<openrepublic::content::RasterOrientation> ParseRasterOrientation(std::wstring_view name)
	{
		using openrepublic::content::RasterOrientation;
		if (name == L"identity" || name == L"first-row-at-min-z") return RasterOrientation::Identity;
		if (name == L"flip-x") return RasterOrientation::FlipX;
		if (name == L"flip-z" || name == L"first-row-at-max-z") return RasterOrientation::FlipZ;
		if (name == L"rotate-180") return RasterOrientation::Rotate180;
		if (name == L"transpose") return RasterOrientation::Transpose;
		if (name == L"transpose-flip-x") return RasterOrientation::TransposeFlipX;
		if (name == L"transpose-flip-z") return RasterOrientation::TransposeFlipZ;
		if (name == L"transpose-rotate-180") return RasterOrientation::TransposeRotate180;
		return std::nullopt;
	}

	std::filesystem::path ResolveApplicationDirectory(const std::filesystem::path& requested,
		const std::filesystem::path& workingDirectory, const std::filesystem::path& executablePath)
	{
		std::error_code error;
		const auto workingCandidate = workingDirectory / requested;
		if (std::filesystem::is_directory(workingCandidate, error) && !error) return workingCandidate;
		auto directory = executablePath.parent_path();
		while (!directory.empty())
		{
			error.clear();
			const auto candidate = directory / requested;
			if (std::filesystem::is_directory(candidate, error) && !error) return candidate;
			const auto parent = directory.parent_path();
			if (parent == directory) break;
			directory = parent;
		}
		return workingCandidate;
	}

	bool PathIsSameOrWithinDirectory(const std::filesystem::path& candidate,
		const std::filesystem::path& directory, bool& isWithin, std::string& error)
	{
		std::error_code pathError;
		std::filesystem::path canonicalCandidate = std::filesystem::weakly_canonical(candidate, pathError);
		if (pathError)
		{
			error = "could not resolve showcase output path: " + pathError.message();
			return false;
		}
		std::filesystem::path canonicalDirectory = std::filesystem::weakly_canonical(directory, pathError);
		if (pathError)
		{
			error = "could not resolve input directory for showcase safety check: " + pathError.message();
			return false;
		}

		std::wstring candidateText = canonicalCandidate.lexically_normal().native();
		std::wstring directoryText = canonicalDirectory.lexically_normal().native();
		const auto normalizeSeparators = [](std::wstring& value) {
			for (wchar_t& character : value)
			{
				if (character == L'/') character = L'\\';
			}
		};
		normalizeSeparators(candidateText);
		normalizeSeparators(directoryText);
		const auto trimTrailingSeparators = [](std::wstring& value) {
			while (value.size() > 1 && value.back() == L'\\' &&
				!(value.size() == 3 && value[1] == L':'))
			{
				value.pop_back();
			}
		};
		trimTrailingSeparators(candidateText);
		trimTrailingSeparators(directoryText);

		if (directoryText.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
			candidateText.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			error = "showcase path is too long for the Windows path safety check";
			return false;
		}
		const std::size_t comparedLength = std::min(candidateText.size(), directoryText.size());
		const int prefixComparison = CompareStringOrdinal(candidateText.c_str(),
			static_cast<int>(comparedLength), directoryText.c_str(), static_cast<int>(comparedLength), TRUE);
		if (prefixComparison == 0)
		{
			error = "Windows could not compare the showcase output path prefix";
			return false;
		}
		const bool exact = candidateText.size() == directoryText.size() && prefixComparison == CSTR_EQUAL;
		const bool directoryHasTrailingSeparator = !directoryText.empty() && directoryText.back() == L'\\';
		const bool child = candidateText.size() > directoryText.size() && prefixComparison == CSTR_EQUAL &&
			(directoryHasTrailingSeparator || candidateText[directoryText.size()] == L'\\');
		isWithin = exact || child;
		return true;
	}

	LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_SIZE:
		case WM_DPICHANGED:
			if (application.is_window_active)
			{
				application.SetWindow(window);
			}
			return 0;
		case WM_INPUT:
			wi::input::rawinput::ParseMessage(reinterpret_cast<void*>(lParam));
			return 0;
		case WM_CLOSE:
			if (!application.ConfirmQuit()) return 0;
			DestroyWindow(window);
			return 0;
		case WM_SETFOCUS:
			application.is_window_active = true;
			return 0;
		case WM_KILLFOCUS:
			application.is_window_active = false;
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProcW(window, message, wParam, lParam);
		}
	}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	int argumentCount = 0;
	LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	std::vector<std::wstring> rawArguments;
	for (int i = 1; arguments != nullptr && i < argumentCount; ++i) rawArguments.emplace_back(arguments[i]);
	std::optional<openrepublic::runtime::CampaignDefinition> campaignDefinition;
	std::optional<openrepublic::runtime::CampaignProgress> campaignProgress;
	std::optional<openrepublic::runtime::CampaignMission> campaignMission;
	std::filesystem::path campaignProgressPath;
	std::string campaignArgumentError;
	std::wstring campaignIdArgument;
	std::size_t campaignArgumentCount = 0;
	for (std::size_t i = 0; i < rawArguments.size(); ++i)
	{
		if (rawArguments[i] != L"--campaign") continue;
		++campaignArgumentCount;
		if (campaignArgumentCount > 1 || i + 1 >= rawArguments.size() ||
			rawArguments[i + 1].empty() || rawArguments[i + 1].rfind(L"--", 0) == 0)
		{
			campaignArgumentError = "--campaign requires exactly one campaign ID or list";
			break;
		}
		campaignIdArgument = rawArguments[++i];
	}
	if (!campaignArgumentError.empty())
	{
		LocalFree(arguments);
		return LogNativeEconomyStartupError(campaignArgumentError);
	}
	if (!campaignIdArgument.empty())
	{
		if (rawArguments.size() != 2)
		{
			LocalFree(arguments);
			return LogNativeEconomyStartupError("--campaign cannot be combined with other startup options");
		}
		std::vector<wchar_t> executablePathBuffer(32768);
		const DWORD executablePathLength = GetModuleFileNameW(nullptr, executablePathBuffer.data(),
			static_cast<DWORD>(executablePathBuffer.size()));
		const std::filesystem::path executablePath = executablePathLength > 0 &&
			executablePathLength < executablePathBuffer.size()
			? std::filesystem::path(std::wstring(executablePathBuffer.data(), executablePathLength))
			: std::filesystem::path{};
		std::error_code currentPathError;
		const auto workingDirectory = std::filesystem::current_path(currentPathError);
		const auto campaignRoot = currentPathError || executablePath.empty()
			? std::filesystem::path("data/campaign")
			: ResolveApplicationDirectory("data/campaign", workingDirectory, executablePath);
		if (campaignIdArgument == L"list")
		{
			std::vector<std::string> ids;
			std::string error;
			if (!openrepublic::runtime::ListCampaigns(campaignRoot, ids, error))
			{
				LocalFree(arguments);
				return LogNativeEconomyStartupError(error);
			}
			for (const auto& id : ids) std::fprintf(stdout, "%s\n", id.c_str());
			LocalFree(arguments);
			return 0;
		}
		std::string campaignId;
		for (const wchar_t character : campaignIdArgument)
		{
			if (character > 127) { campaignArgumentError = "campaign ID must be ASCII"; break; }
			campaignId.push_back(static_cast<char>(character));
		}
		if (campaignArgumentError.empty())
		{
			const auto manifestPath = campaignRoot / campaignId / "campaign.txt";
			campaignDefinition.emplace();
			std::string error;
			if (!openrepublic::runtime::LoadCampaign(manifestPath, *campaignDefinition, error))
				campaignArgumentError = error;
			else if (campaignDefinition->id != campaignId)
				campaignArgumentError = "campaign directory ID does not match its manifest";
			else
			{
				campaignProgressPath = openrepublic::runtime::DefaultCampaignProgressPath(campaignDefinition->id);
				campaignProgress.emplace();
				if (!openrepublic::runtime::LoadCampaignProgress(campaignProgressPath,
					*campaignDefinition, *campaignProgress, error)) campaignArgumentError = error;
				else
				{
					for (const auto& mission : campaignDefinition->missions)
					{
						const auto found = std::find_if(campaignProgress->missions.begin(), campaignProgress->missions.end(),
							[&mission](const auto& item) { return item.first == mission.id; });
						if (found == campaignProgress->missions.end() || !found->second.completed)
						{
							campaignMission = mission;
							break;
						}
					}
					if (!campaignMission) campaignArgumentError = "all campaign missions are already complete";
				}
			}
		}
		if (!campaignArgumentError.empty())
		{
			LocalFree(arguments);
			return LogNativeEconomyStartupError(campaignArgumentError);
		}
	}
	const auto playOptions = openrepublic::runtime::ResolvePlayOptions(rawArguments);
	if (!playOptions.error.empty())
	{
		LocalFree(arguments);
		return LogNativeEconomyStartupError(playOptions.error);
	}
	std::filesystem::path sceneProbePath;
	std::filesystem::path glbProbePath;
	std::filesystem::path nmfProbePath;
	std::filesystem::path nmfMaterialPath;
	std::filesystem::path contentRoot;
	std::filesystem::path storageProfilePath;
	bool storageProfileGiven = false;
	std::string storageProfileError;
	bool utilityResourceGiven = false;
	std::string utilityResourceError;
	bool syntheticRegionFleetGiven = false;
	std::string syntheticRegionFleetError;
	openrepublic::compat::CatalogImportOptions regionCatalogOptions;
	std::filesystem::path mapPackage;
	std::size_t mapBuildingIndex = 0;
	bool mapBuildingIndexGiven = false;
	bool mapRegion = false;
	bool syntheticRegionFixture = false;
	double mapRadiusMeters = 0.0;
	int mapBuildingModelLevel = 0;
	bool mapBuildingModelLevelGiven = false;
	bool mapBuildingDynamicLod = false;
	float mapLodNearDistance = 250.0f;
	float mapLodFarDistance = 700.0f;
	bool mapLodDistancesGiven = false;
	std::string buildingTypeName;
	std::string economyHost;
	int economyPort = 0;
	std::wstring localEconomyPipeName;
	bool localEconomyPipeNameGiven = false;
	bool economyUrlArgumentSeen = false;
	std::optional<openrepublic::runtime::GraphicsSettings> graphicsSettings;
	int buildingModelLevel = 0;
	std::vector<std::filesystem::path> nmfTextureRoots;
	bool nmfUnlitPreview = false;
	bool nmfMaterialUnlitPreview = false;
	std::vector<std::filesystem::path> nmfCapturePaths;
	std::filesystem::path mapShowcaseCapturePath;
	bool mapShowcaseEnhancedLook = false;
	bool mapShowcaseLookArgumentSeen = false;
	float mapShowcaseCameraYawDegrees = 45.0f;
	float mapShowcaseCameraElevationDegrees = 24.5f;
	float mapShowcaseCameraDistanceScale = 1.5f;
	bool mapShowcaseCameraArgumentSeen = false;
	bool showcaseVegetationWind = true;
	bool showcaseGrassLayer = true;
	float mapShowcaseSunElevationDegrees = std::numeric_limits<float>::quiet_NaN();
	std::wstring scenePipeName;
	std::filesystem::path sceneScriptPath;
	float mapShowcaseSunYawOffsetDegrees = 0.0f;
	double nmfCaptureZoom = 1.0;
	bool nmfCaptureZoomArgumentSeen = false;
	std::optional<double> nmfCaptureElevationDegrees;
	bool nmfCaptureElevationArgumentSeen = false;
	std::filesystem::path terrainProbePath;
	std::filesystem::path economyCheckpointPath;
	std::unique_ptr<openrepublic::economy::RestoredCheckpoint> economyCheckpoint;
	bool economyCheckpointArgumentSeen = false;
	bool nativeEconomyArgumentSeen = false;
	bool nativeConstructionSitesArgumentSeen = false;
	bool nativeRailArgumentSeen = false;
	bool nativeCoalDemoArgumentSeen = false;
	bool nativeTownDemoArgumentSeen = false;
	std::wstring nativeObjectivesArgument;
	std::optional<openrepublic::runtime::NativeObjectiveSet> nativeObjectiveSet;
	std::optional<nlohmann::json> campaignScenario;
	std::string campaignBriefing;
	bool ambientTrafficArgumentSeen = false;
	bool nativeTrafficPolishArgumentSeen = false;
	bool nativeTerrainArgumentSeen = false;
	bool nativeContentCatalogArgumentSeen = false;
	std::filesystem::path nativeContentCatalogPath;
	std::string nativeEconomyArgumentError;
	std::vector<std::wstring> economyCheckpointIncompatibleOptions;
	std::string economyCheckpointArgumentError;
	std::optional<openrepublic::content::TerrainTransform> terrainProbeTransform;
	std::string terrainProbeTransformError;
	std::string nmfCaptureArgumentError;
	bool terrainTransformArgumentSeen = false;
	bool autoplayArgumentSeen = false;
	std::uint32_t autoplaySpeed = 1;
	std::filesystem::path uiScriptPath, screenshotPath;
	std::uint32_t screenshotAfterFrames = 1;
	if (arguments != nullptr)
	{
		for (int i = 1; i < argumentCount; ++i)
		{
			const std::wstring_view argument = arguments[i];
			if (IsStandaloneEconomyIncompatibleOption(argument))
			{
				if (std::find(economyCheckpointIncompatibleOptions.begin(),
					economyCheckpointIncompatibleOptions.end(), argument) ==
					economyCheckpointIncompatibleOptions.end())
				{
					economyCheckpointIncompatibleOptions.emplace_back(argument);
				}
			}
			if (argument == L"--economy-checkpoint")
			{
				if (economyCheckpointArgumentSeen)
				{
					economyCheckpointArgumentError = "--economy-checkpoint may only be supplied once";
					if (i + 1 < argumentCount && std::wstring_view(arguments[i + 1]).rfind(L"--", 0) != 0)
						++i;
					continue;
				}
				economyCheckpointArgumentSeen = true;
				if (i + 1 >= argumentCount || std::wstring_view(arguments[i + 1]).empty() ||
					std::wstring_view(arguments[i + 1]).rfind(L"--", 0) == 0)
				{
					economyCheckpointArgumentError = "--economy-checkpoint requires a non-empty file path";
					continue;
				}
				economyCheckpointPath = arguments[++i];
			}
			else if (argument == L"--native-economy")
			{
				if (nativeEconomyArgumentSeen)
				{
					nativeEconomyArgumentError = "--native-economy may only be supplied once";
					continue;
				}
				nativeEconomyArgumentSeen = true;
			}
			else if (argument == L"--autoplay") autoplayArgumentSeen = true;
			else if (argument == L"--autoplay-speed")
			{
				if (i + 1 >= argumentCount) nativeEconomyArgumentError = "--autoplay-speed requires an integer from 1 to 10000";
				else { wchar_t* end=nullptr; const auto value=std::wcstoul(arguments[++i], &end, 10); if(!end || *end || value<1 || value>10000) nativeEconomyArgumentError="--autoplay-speed requires an integer from 1 to 10000"; else autoplaySpeed=static_cast<std::uint32_t>(value); }
			}
			else if (argument == L"--ui-script")
			{
				if (i + 1 >= argumentCount || std::wstring_view(arguments[i+1]).rfind(L"--",0)==0) nativeEconomyArgumentError="--ui-script requires a JSON file";
				else uiScriptPath=arguments[++i];
			}
			else if (argument == L"--screenshot")
			{
				if (i + 1 >= argumentCount || std::wstring_view(arguments[i+1]).rfind(L"--",0)==0) nativeEconomyArgumentError="--screenshot requires a PNG path";
				else screenshotPath=arguments[++i];
			}
			else if (argument == L"--screenshot-after-frames")
			{
				if (i + 1 >= argumentCount) nativeEconomyArgumentError="--screenshot-after-frames requires a positive frame count";
				else { wchar_t* end=nullptr; const auto value=std::wcstoul(arguments[++i], &end, 10); if(!end || *end || value<1 || value>1000000) nativeEconomyArgumentError="--screenshot-after-frames requires a positive frame count"; else screenshotAfterFrames=static_cast<std::uint32_t>(value); }
			}
			else if (argument == L"--native-construction-sites")
			{
				// Opt-in synthetic catalog with planks/bricks and `build` requirements (requires --native-economy).
				nativeConstructionSitesArgumentSeen = true;
			}
			else if (argument == L"--native-rail") nativeRailArgumentSeen = true;
			else if (argument == L"--native-terrain") nativeTerrainArgumentSeen = true;
			else if (argument == L"--native-coal-demo") nativeCoalDemoArgumentSeen = true;
			else if (argument == L"--native-town-demo") nativeTownDemoArgumentSeen = true;
			else if (argument == L"--native-catalog-from-content")
			{
				if (nativeContentCatalogArgumentSeen)
				{
					nativeEconomyArgumentError = "--native-catalog-from-content may only be supplied once";
					if (i + 1 < argumentCount && std::wstring_view(arguments[i + 1]).rfind(L"--", 0) != 0) ++i;
					continue;
				}
				nativeContentCatalogArgumentSeen = true;
				if (i + 1 >= argumentCount || std::wstring_view(arguments[i + 1]).empty() ||
					std::wstring_view(arguments[i + 1]).rfind(L"--", 0) == 0)
				{
					nativeEconomyArgumentError = "--native-catalog-from-content requires a media_soviet or buildings_types directory";
					continue;
				}
				nativeContentCatalogPath = arguments[++i];
			}
			else if (argument == L"--native-objectives")
			{
				if (!nativeObjectivesArgument.empty() || i + 1 >= argumentCount ||
					std::wstring_view(arguments[i + 1]).empty() || std::wstring_view(arguments[i + 1]).rfind(L"--", 0) == 0)
					nativeEconomyArgumentError = "--native-objectives requires one objective JSON path or builtin:coal_demo";
				else nativeObjectivesArgument = arguments[++i];
			}
			else if (argument == L"--ambient-traffic") ambientTrafficArgumentSeen = true;
			else if (argument == L"--native-traffic-polish") nativeTrafficPolishArgumentSeen = true;
			else if (argument == L"--scene-probe" && i + 1 < argumentCount)
			{
				sceneProbePath = arguments[++i];
			}
			else if (argument == L"--glb-probe" && i + 1 < argumentCount)
			{
				glbProbePath = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--nmf-probe" && i + 1 < argumentCount)
			{
				nmfProbePath = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--content-root" && i + 1 < argumentCount)
			{
				contentRoot = arguments[++i];
			}
			else if (argument == L"--utility-resource")
			{
				utilityResourceGiven = true;
				if (i + 1 >= argumentCount || std::wstring_view(arguments[i + 1]).empty() ||
					std::wstring_view(arguments[i + 1]).rfind(L"--", 0) == 0)
				{
					utilityResourceError = "--utility-resource requires a resource name";
					continue;
				}
				const std::wstring_view name = arguments[++i];
				if (name.size() > 96 || std::any_of(name.begin(), name.end(), [](wchar_t c) {
					return c <= 32 || c > 126 || c == L'#'; }))
					utilityResourceError = "--utility-resource requires a valid ASCII resource name";
				else
				{
					std::string narrowName;
					for (wchar_t c : name) narrowName.push_back(static_cast<char>(c));
					if (!regionCatalogOptions.utilityResources.emplace(std::move(narrowName)).second)
						utilityResourceError = "--utility-resource names must be distinct";
				}
			}
			else if (argument == L"--region-synthetic-fleet")
			{
				if (syntheticRegionFleetGiven)
					syntheticRegionFleetError = "--region-synthetic-fleet may only be supplied once";
				syntheticRegionFleetGiven = true;
				if (i + 1 >= argumentCount || std::wstring_view(arguments[i + 1]).empty() ||
					std::wstring_view(arguments[i + 1]).rfind(L"--", 0) == 0)
				{
					syntheticRegionFleetError = "--region-synthetic-fleet requires a truck count from 1 to 16";
					continue;
				}
				const std::wstring value = arguments[++i];
				wchar_t* end = nullptr;
				const unsigned long count = std::wcstoul(value.c_str(), &end, 10);
				const bool decimalOnly = std::all_of(value.begin(), value.end(), [](wchar_t c) {
					return c >= L'0' && c <= L'9';
				});
				if (!decimalOnly || end == value.c_str() || *end != L'\0' || count < 1 || count > 16)
					syntheticRegionFleetError = "--region-synthetic-fleet requires a truck count from 1 to 16";
				else
					regionCatalogOptions.syntheticRegionFleetCount = static_cast<std::uint32_t>(count);
			}
			else if (argument == L"--storage-profile")
			{
				if (storageProfileGiven)
					storageProfileError = "--storage-profile may only be supplied once";
				storageProfileGiven = true;
				if (i + 1 >= argumentCount || std::wstring_view(arguments[i + 1]).empty() ||
					std::wstring_view(arguments[i + 1]).rfind(L"--", 0) == 0)
					storageProfileError = "--storage-profile requires a non-empty file path";
				else
					storageProfilePath = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--economy-url")
			{
				economyUrlArgumentSeen = true;
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--economy-url requires http://127.0.0.1:<port>";
					continue;
				}
				// http://127.0.0.1:PORT (loopback only)
				const std::wstring url = arguments[++i];
				const std::wstring prefix = L"http://127.0.0.1:";
				if (url.rfind(prefix, 0) == 0 && url.size() > prefix.size() && url.size() <= prefix.size() + 5)
				{
					const int port = std::wcstol(url.c_str() + prefix.size(), nullptr, 10);
					if (port > 0 && port < 65536)
					{
						economyHost = "127.0.0.1";
						economyPort = port;
					}
				}
				if (economyPort == 0)
				{
					nmfCaptureArgumentError = "--economy-url requires http://127.0.0.1:<port>";
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--economy-pipe-name")
			{
				if (localEconomyPipeNameGiven)
				{
					nmfCaptureArgumentError = "--economy-pipe-name may only be supplied once";
					if (i + 1 < argumentCount) ++i;
					continue;
				}
				localEconomyPipeNameGiven = true;
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--economy-pipe-name requires 1-64 ASCII letters, digits, '.', '_' or '-'";
					continue;
				}
				localEconomyPipeName = arguments[++i];
				const bool validName = !localEconomyPipeName.empty() && localEconomyPipeName.size() <= 64 &&
					std::all_of(localEconomyPipeName.begin(), localEconomyPipeName.end(), [](wchar_t value) {
						return (value >= L'a' && value <= L'z') || (value >= L'A' && value <= L'Z') ||
							(value >= L'0' && value <= L'9') || value == L'.' || value == L'_' || value == L'-';
					});
				if (!validName)
				{
					localEconomyPipeName.clear();
					nmfCaptureArgumentError = "--economy-pipe-name requires 1-64 ASCII letters, digits, '.', '_' or '-'";
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--graphics" && i + 1 < argumentCount)
			{
				const std::wstring wide = arguments[++i];
				std::string narrow;
				for (const wchar_t value : wide)
				{
					narrow.push_back(value < 0x80 ? static_cast<char>(value) : '?');
				}
				openrepublic::runtime::GraphicsSettings parsed;
				std::string graphicsError;
				if (openrepublic::runtime::ParseGraphicsSettings(narrow, parsed, &graphicsError))
				{
					graphicsSettings = parsed;
				}
				else
				{
					nmfCaptureArgumentError = "--graphics: " + graphicsError;
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--map-package" && i + 1 < argumentCount)
			{
				mapPackage = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--map-building" && i + 1 < argumentCount)
			{
				const std::wstring_view value = arguments[++i];
				double parsed = 0.0;
				if (!ParseFiniteDouble(arguments[i], parsed) || parsed < 0.0 || parsed > 65535.0 ||
					parsed != static_cast<double>(static_cast<std::size_t>(parsed)))
				{
					nmfCaptureArgumentError = "--map-building requires a placement index from 0 through 65535";
				}
				else
				{
					mapBuildingIndex = static_cast<std::size_t>(parsed);
					mapBuildingIndexGiven = true;
				}
				static_cast<void>(value);
			}
			else if (std::wstring_view(arguments[i]) == L"--map-region")
			{
				mapRegion = true;
			}
			else if (std::wstring_view(arguments[i]) == L"--synthetic-region-fixture")
			{
				if (syntheticRegionFixture)
				{
					nmfCaptureArgumentError = "--synthetic-region-fixture may only be supplied once";
				}
				syntheticRegionFixture = true;
			}
			else if (std::wstring_view(arguments[i]) == L"--map-radius" && i + 1 < argumentCount)
			{
				double parsed = 0.0;
				if (!ParseFiniteDouble(arguments[++i], parsed) || parsed < 10.0 || parsed > 5000.0)
				{
					nmfCaptureArgumentError = "--map-radius requires meters from 10 through 5000";
				}
				else
				{
					mapRadiusMeters = parsed;
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--map-lod")
			{
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--map-lod requires 0, 1, 2 or auto";
				}
				else
				{
					const std::wstring_view level = arguments[++i];
					if (level == L"auto")
					{
						mapBuildingModelLevel = 2;
						mapBuildingDynamicLod = true;
						mapBuildingModelLevelGiven = true;
					}
					else if (level == L"0" || level == L"1" || level == L"2")
					{
						mapBuildingModelLevel = level[0] - L'0';
						mapBuildingModelLevelGiven = true;
					}
					else
					{
						nmfCaptureArgumentError = "--map-lod requires 0, 1, 2 or auto";
					}
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--map-lod-distances")
			{
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--map-lod-distances requires near,far distances in meters";
				}
				else
				{
					const std::wstring_view pair(arguments[++i]);
					const std::size_t comma = pair.find(L',');
					double nearDistance = 0.0, farDistance = 0.0;
					const std::wstring nearText(pair.substr(0, comma));
					const std::wstring farText = comma == std::wstring_view::npos ? L"" : std::wstring(pair.substr(comma + 1));
					if (comma == std::wstring_view::npos || pair.find(L',', comma + 1) != std::wstring_view::npos ||
						!ParseFiniteDouble(nearText.c_str(), nearDistance) || !ParseFiniteDouble(farText.c_str(), farDistance) ||
						nearDistance <= 0.0 || farDistance <= nearDistance || farDistance > 100000.0)
					{
						nmfCaptureArgumentError = "--map-lod-distances requires 0 < near < far <= 100000 meters, formatted near,far";
					}
					else
					{
						mapLodNearDistance = static_cast<float>(nearDistance);
						mapLodFarDistance = static_cast<float>(farDistance);
						if (!std::isfinite(mapLodNearDistance) || !std::isfinite(mapLodFarDistance) ||
							mapLodNearDistance <= 0.0f || mapLodFarDistance <= mapLodNearDistance)
						{
							nmfCaptureArgumentError = "--map-lod-distances are not representable as distinct positive float meters";
						}
						else
						{
							mapLodDistancesGiven = true;
						}
					}
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--showcase-capture")
			{
				if (i + 1 >= argumentCount || !mapShowcaseCapturePath.empty())
				{
					nmfCaptureArgumentError = "--showcase-capture requires one absolute output .png path";
					continue;
				}
				mapShowcaseCapturePath = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--showcase-look")
			{
				mapShowcaseLookArgumentSeen = true;
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--showcase-look requires baseline or enhanced";
					continue;
				}
				const std::wstring_view look = arguments[++i];
				if (look == L"baseline")
				{
					mapShowcaseEnhancedLook = false;
				}
				else if (look == L"enhanced")
				{
					mapShowcaseEnhancedLook = true;
				}
				else
				{
					nmfCaptureArgumentError = "--showcase-look requires baseline or enhanced";
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--showcase-camera")
			{
				if (mapShowcaseCameraArgumentSeen)
				{
					nmfCaptureArgumentError = "--showcase-camera may only be supplied once";
					if (i + 1 < argumentCount) ++i;
					if (i + 1 < argumentCount) ++i;
					if (i + 1 < argumentCount) ++i;
					continue;
				}
				mapShowcaseCameraArgumentSeen = true;
				if (i + 3 >= argumentCount)
				{
					nmfCaptureArgumentError = "--showcase-camera requires yaw-degrees elevation-degrees distance-radius-factor";
					continue;
				}
				std::array<double, 3> camera{};
				bool valuesValid = true;
				for (std::size_t valueIndex = 0; valueIndex < camera.size(); ++valueIndex)
				{
					valuesValid = ParseFiniteDouble(arguments[i + 1 + static_cast<int>(valueIndex)], camera[valueIndex]) && valuesValid;
				}
				i += 3;
				if (!valuesValid || camera[0] < -180.0 || camera[0] > 180.0 ||
					camera[1] < 5.0 || camera[1] > 75.0 || camera[2] < 0.5 || camera[2] > 2.5)
				{
					nmfCaptureArgumentError = "--showcase-camera requires finite yaw -180..180, elevation 5..75, and distance factor 0.5..2.5";
					continue;
				}
				mapShowcaseCameraYawDegrees = static_cast<float>(camera[0]);
				mapShowcaseCameraElevationDegrees = static_cast<float>(camera[1]);
				mapShowcaseCameraDistanceScale = static_cast<float>(camera[2]);
			}
			else if (std::wstring_view(arguments[i]) == L"--showcase-wind" || std::wstring_view(arguments[i]) == L"--showcase-grass")
			{
				const bool isWind = std::wstring_view(arguments[i]) == L"--showcase-wind";
				if (i + 1 >= argumentCount || (std::wstring_view(arguments[i + 1]) != L"on" && std::wstring_view(arguments[i + 1]) != L"off"))
				{
					nmfCaptureArgumentError = isWind ? "--showcase-wind requires on or off" : "--showcase-grass requires on or off";
					continue;
				}
				const bool enabled = std::wstring_view(arguments[++i]) == L"on";
				if (isWind) showcaseVegetationWind = enabled;
				else showcaseGrassLayer = enabled;
			}
			else if (std::wstring_view(arguments[i]) == L"--scene-pipe-name" && i + 1 < argumentCount)
			{
				// Runtime look/camera control pipe (requires --modern or an enhanced region view).
				scenePipeName = arguments[++i];
				const bool validName = !scenePipeName.empty() && scenePipeName.size() <= 64 &&
					std::all_of(scenePipeName.begin(), scenePipeName.end(), [](wchar_t value) {
						return (value >= L'a' && value <= L'z') || (value >= L'A' && value <= L'Z') ||
							(value >= L'0' && value <= L'9') || value == L'.' || value == L'_' || value == L'-';
					});
				if (!validName)
				{
					scenePipeName.clear();
					nmfCaptureArgumentError = "--scene-pipe-name requires 1-64 ASCII letters, digits, '.', '_' or '-'";
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--scene-script" && i + 1 < argumentCount)
			{
				sceneScriptPath = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--modern")
			{
				// Interactive next-generation look for the region view (same preset as the showcase
				// captures: physically based sky, lake, relief, trees, props, post effects).
				mapShowcaseEnhancedLook = true;
				if (!std::isfinite(mapShowcaseSunElevationDegrees))
				{
					mapShowcaseSunElevationDegrees = 17.0f;
					mapShowcaseSunYawOffsetDegrees = 45.0f;
					if (!mapShowcaseCameraArgumentSeen)
					{
						// Hero pose so the sun sits behind-left of the camera like the showcase captures.
						mapShowcaseCameraYawDegrees = -110.0f;
						mapShowcaseCameraElevationDegrees = 16.0f;
					}
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--showcase-sun")
			{
				// --showcase-sun <elevation 2..80> <yaw offset from the camera direction -180..180>
				std::array<double, 2> sun{};
				if (i + 2 >= argumentCount ||
					!ParseFiniteDouble(arguments[i + 1], sun[0]) || !ParseFiniteDouble(arguments[i + 2], sun[1]) ||
					sun[0] < 2.0 || sun[0] > 80.0 || sun[1] < -180.0 || sun[1] > 180.0)
				{
					nmfCaptureArgumentError = "--showcase-sun requires elevation-degrees 2..80 and yaw-offset-degrees -180..180";
					i = std::min(i + 2, argumentCount - 1);
					continue;
				}
				i += 2;
				mapShowcaseSunElevationDegrees = static_cast<float>(sun[0]);
				mapShowcaseSunYawOffsetDegrees = static_cast<float>(sun[1]);
			}
			else if (std::wstring_view(arguments[i]) == L"--building-type" && i + 1 < argumentCount)
			{
				const std::wstring wide = arguments[++i];
				if (wide.empty() || std::any_of(wide.begin(), wide.end(), [](wchar_t value) { return value <= 0x20 || value > 0x7e; }))
				{
					nmfCaptureArgumentError = "--building-type requires a printable ASCII type name without spaces";
				}
				else
				{
					for (const wchar_t value : wide)
					{
						buildingTypeName.push_back(static_cast<char>(value));
					}
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--building-lod" && i + 1 < argumentCount)
			{
				const std::wstring_view level = arguments[++i];
				if (level == L"0" || level == L"1" || level == L"2")
				{
					buildingModelLevel = level[0] - L'0';
				}
				else
				{
					nmfCaptureArgumentError = "--building-lod requires 0, 1 or 2";
				}
			}
			else if (std::wstring_view(arguments[i]) == L"--nmf-texture-root")
			{
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--nmf-texture-root requires an absolute local content directory";
					continue;
				}
				if (nmfTextureRoots.size() >= openrepublic::content::MaxMtlDependencyRoots)
				{
					nmfCaptureArgumentError = "--nmf-texture-root may be supplied at most 32 times";
					++i;
					continue;
				}
				nmfTextureRoots.emplace_back(arguments[++i]);
			}
			else if (std::wstring_view(arguments[i]) == L"--nmf-unlit-preview")
			{
				nmfUnlitPreview = true;
			}
			else if (std::wstring_view(arguments[i]) == L"--nmf-material-unlit-preview")
			{
				nmfMaterialUnlitPreview = true;
			}
			else if (std::wstring_view(arguments[i]) == L"--capture-preview")
			{
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--capture-preview requires an absolute output .png path";
					continue;
				}
				if (nmfCapturePaths.size() >= 11)
				{
					nmfCaptureArgumentError = "--capture-preview may be supplied at most eleven times";
					++i;
					continue;
				}
				nmfCapturePaths.emplace_back(arguments[++i]);
			}
			else if (std::wstring_view(arguments[i]) == L"--capture-preview-zoom")
			{
				if (nmfCaptureZoomArgumentSeen)
				{
					nmfCaptureArgumentError = "--capture-preview-zoom may only be supplied once";
					if (i + 1 < argumentCount)
					{
						++i;
					}
					continue;
				}
				nmfCaptureZoomArgumentSeen = true;
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--capture-preview-zoom requires a finite factor from 0.5 through 2.5";
					continue;
				}
				double zoom = 0.0;
				if (!ParseFiniteDouble(arguments[++i], zoom) || zoom < 0.5 || zoom > 2.5)
				{
					nmfCaptureArgumentError = "--capture-preview-zoom requires a finite factor from 0.5 through 2.5";
					continue;
				}
				nmfCaptureZoom = zoom;
			}
			else if (std::wstring_view(arguments[i]) == L"--capture-preview-elevation")
			{
				if (nmfCaptureElevationArgumentSeen)
				{
					nmfCaptureArgumentError = "--capture-preview-elevation may only be supplied once";
					if (i + 1 < argumentCount)
					{
						++i;
					}
					continue;
				}
				nmfCaptureElevationArgumentSeen = true;
				if (i + 1 >= argumentCount)
				{
					nmfCaptureArgumentError = "--capture-preview-elevation requires finite degrees from 0 through 60";
					continue;
				}
				double elevation = 0.0;
				if (!ParseFiniteDouble(arguments[++i], elevation) || elevation < 0.0 || elevation > 60.0)
				{
					nmfCaptureArgumentError = "--capture-preview-elevation requires finite degrees from 0 through 60";
					continue;
				}
				nmfCaptureElevationDegrees = elevation;
			}
			else if (std::wstring_view(arguments[i]) == L"--terrain-probe" && i + 1 < argumentCount)
			{
				terrainProbePath = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--terrain-transform")
			{
				if (terrainTransformArgumentSeen)
				{
					terrainProbeTransformError = "--terrain-transform may only be supplied once";
				}
				terrainTransformArgumentSeen = true;
				if (i + 7 >= argumentCount)
				{
					terrainProbeTransformError =
						"--terrain-transform requires originX originZ baseHeight sizeX sizeZ heightScale orientation";
					continue;
				}

				std::array<double, 6> values{};
				bool valuesValid = true;
				for (std::size_t valueIndex = 0; valueIndex < values.size(); ++valueIndex)
				{
					valuesValid = ParseFiniteDouble(arguments[i + 1 + static_cast<int>(valueIndex)], values[valueIndex]) && valuesValid;
				}
				const std::wstring_view orientationName(arguments[i + 7]);
				i += 7;
				if (!terrainProbeTransformError.empty())
				{
					continue;
				}
				if (!valuesValid)
				{
					terrainProbeTransformError = "--terrain-transform requires six finite numeric values";
					continue;
				}

				openrepublic::content::TerrainTransform transform;
				transform.originX = values[0];
				transform.originZ = values[1];
				transform.baseHeight = values[2];
				transform.sizeX = values[3];
				transform.sizeZ = values[4];
				transform.heightScale = values[5];
				if (const auto orientation = ParseRasterOrientation(orientationName))
				{
					transform.orientation = *orientation;
				}
				else
				{
					terrainProbeTransformError =
						"orientation must be identity, flip-x, flip-z, rotate-180, transpose, transpose-flip-x, transpose-flip-z, or transpose-rotate-180";
					continue;
				}
				terrainProbeTransform = transform;
			}
		}
		LocalFree(arguments);
	}
	if (playOptions.selected)
	{
		// The resolver prepends the selected preset's existing switches; the
		// original parser has already handled caller-supplied unrelated options.
		nativeEconomyArgumentSeen = true;
		nativeTerrainArgumentSeen = true;
		ambientTrafficArgumentSeen = true;
		for (const auto& option : playOptions.arguments)
		{
			if (option == L"--native-coal-demo") nativeCoalDemoArgumentSeen = true;
			else if (option == L"--native-construction-sites") nativeConstructionSitesArgumentSeen = true;
			else if (option == L"--native-town-demo") nativeTownDemoArgumentSeen = true;
			else if (option == L"--native-objectives") continue;
			else if (option.rfind(L"builtin:", 0) == 0) nativeObjectivesArgument = option;
		}
	}
	if (campaignMission)
	{
		if (campaignMission->startingTreasury != 0)
			return LogNativeEconomyStartupError("campaign missions currently require a zero starting treasury");
		nativeEconomyArgumentSeen = true;
		nativeTerrainArgumentSeen = true;
		ambientTrafficArgumentSeen = true;
		nativeObjectivesArgument = (campaignDefinition->directory / campaignMission->objectivesFile).wstring();
		const auto scenarioPath = campaignDefinition->directory / campaignMission->scenarioFile;
		std::error_code fileError;
		const auto scenarioSize = std::filesystem::file_size(scenarioPath, fileError);
		if (fileError || scenarioSize == 0 || scenarioSize > 256 * 1024)
			return LogNativeEconomyStartupError("campaign scenario JSON is unavailable or exceeds 256 KiB");
		try
		{
			std::ifstream scenarioFile(scenarioPath, std::ios::binary);
			if (!scenarioFile) throw std::runtime_error("could not open campaign scenario JSON");
			campaignScenario = nlohmann::json::parse(scenarioFile);
			if (!campaignScenario->is_object() || !campaignScenario->contains("catalog") ||
				!campaignScenario->at("catalog").is_string() || !campaignScenario->contains("roads") ||
				!campaignScenario->at("roads").is_array() || !campaignScenario->contains("buildings") ||
				!campaignScenario->at("buildings").is_array())
				throw std::runtime_error("scenario must contain catalog, roads, and buildings");
			const auto briefingPath = campaignDefinition->directory / campaignMission->briefingFile;
			const auto briefingSize = std::filesystem::file_size(briefingPath, fileError);
			if (fileError || briefingSize == 0 || briefingSize > 16 * 1024)
				throw std::runtime_error("campaign briefing is unavailable or exceeds 16 KiB");
			std::ifstream briefingFile(briefingPath, std::ios::binary);
			if (!briefingFile) throw std::runtime_error("could not open campaign briefing");
			campaignBriefing.assign(std::istreambuf_iterator<char>(briefingFile), {});
			if (briefingFile.bad()) throw std::runtime_error("could not read campaign briefing completely");
		}
		catch (const std::exception& error)
		{
			return LogNativeEconomyStartupError(std::string("invalid campaign mission data: ") + error.what());
		}
	}
	if (!nativeObjectivesArgument.empty() && !economyCheckpointArgumentSeen) nativeEconomyArgumentSeen = true;
	if (!nativeEconomyArgumentError.empty())
	{
		return LogNativeEconomyStartupError(nativeEconomyArgumentError);
	}
	if (autoplayArgumentSeen && !nativeEconomyArgumentSeen && !economyCheckpointArgumentSeen)
		return LogNativeEconomyStartupError("--autoplay requires --native-economy, --economy-checkpoint, or --play");
	if (!screenshotPath.empty() && screenshotPath.extension() != L".png")
		return LogNativeEconomyStartupError("--screenshot currently requires a .png path");
	openrepublic::runtime::UiScript uiScript;
	if (!uiScriptPath.empty())
	{
		std::error_code pathError;
		const auto size = std::filesystem::file_size(uiScriptPath, pathError);
		if (pathError || size > 1024 * 1024) return LogNativeEconomyStartupError("--ui-script is unavailable or exceeds 1 MiB");
		std::ifstream scriptFile(uiScriptPath, std::ios::binary);
		std::string scriptText((std::istreambuf_iterator<char>(scriptFile)), {});
		std::string scriptError;
		if (!scriptFile || !openrepublic::runtime::ParseUiScript(scriptText, uiScript, scriptError))
			return LogNativeEconomyStartupError("invalid --ui-script: " + (scriptError.empty() ? std::string("could not read file") : scriptError));
	}
	if (!screenshotPath.empty())
		uiScript.actions.push_back({ screenshotAfterFrames, "screenshot", {}, screenshotPath.string() });
	std::stable_sort(uiScript.actions.begin(), uiScript.actions.end(), [](const auto& a, const auto& b) {
		return a.frame < b.frame;
	});
	std::vector<std::filesystem::path> screenshotOutputs;
	for (const auto& action : uiScript.actions)
	{
		if (action.action != "screenshot") continue;
		const std::filesystem::path output(action.path);
		const auto normalized = output.lexically_normal();
		if (std::find(screenshotOutputs.begin(), screenshotOutputs.end(), normalized) != screenshotOutputs.end())
			return LogNativeEconomyStartupError("UI screenshot output paths must be distinct");
		screenshotOutputs.push_back(normalized);
		std::error_code outputError;
		if (std::filesystem::exists(output, outputError) || outputError)
			return LogNativeEconomyStartupError("UI screenshot refuses to overwrite an existing or inaccessible output");
		if (!output.parent_path().empty())
		{
			std::filesystem::create_directories(output.parent_path(), outputError);
			if (outputError) return LogNativeEconomyStartupError("could not create UI screenshot output directory");
		}
	}
	if (autoplayArgumentSeen && !playOptions.selected) nativeEconomyArgumentSeen = true;
	if (nativeEconomyArgumentSeen && economyCheckpointArgumentSeen)
	{
		return LogNativeEconomyStartupError("--native-economy cannot be combined with --economy-checkpoint");
	}
	if (nativeCoalDemoArgumentSeen && !nativeEconomyArgumentSeen && !economyCheckpointArgumentSeen)
		return LogNativeEconomyStartupError("--native-coal-demo requires --native-economy");
	if (nativeContentCatalogArgumentSeen && !nativeEconomyArgumentSeen)
		return LogNativeEconomyStartupError("--native-catalog-from-content requires --native-economy");
	if (nativeRailArgumentSeen && !nativeEconomyArgumentSeen)
		return LogNativeEconomyStartupError("--native-rail requires --native-economy");
	if (!nativeObjectivesArgument.empty())
	{
		std::wstring fileArgument = nativeObjectivesArgument;
		if (fileArgument.rfind(L"builtin:", 0) == 0)
			fileArgument = L"data/objectives/" + fileArgument.substr(8) + L".json";
		const std::filesystem::path requestedObjectivePath(fileArgument);
		std::vector<wchar_t> executablePathBuffer(32768);
		const DWORD executablePathLength = GetModuleFileNameW(nullptr, executablePathBuffer.data(),
			static_cast<DWORD>(executablePathBuffer.size()));
		const std::filesystem::path executablePath = executablePathLength > 0 &&
			executablePathLength < executablePathBuffer.size()
			? std::filesystem::path(std::wstring(executablePathBuffer.data(), executablePathLength))
			: std::filesystem::path{};
		std::error_code currentPathError;
		const auto workingDirectory = std::filesystem::current_path(currentPathError);
		const std::filesystem::path objectivePath = currentPathError || executablePath.empty()
			? requestedObjectivePath
			: openrepublic::runtime::ResolveApplicationDataPath(requestedObjectivePath, workingDirectory, executablePath);
		std::error_code objectiveFileError;
		const auto objectiveFileSize = std::filesystem::file_size(objectivePath, objectiveFileError);
		if (objectiveFileError || objectiveFileSize > 64 * 1024)
			return LogNativeEconomyStartupError("objective scenario JSON is unavailable or exceeds the 64 KiB limit");
		std::ifstream objectiveFile(objectivePath, std::ios::binary);
		if (!objectiveFile) return LogNativeEconomyStartupError("could not open objective scenario JSON");
		try
		{
			const auto json = nlohmann::json::parse(objectiveFile);
			if (!json.is_object() || !json.contains("objectives") || !json.at("objectives").is_array() ||
				json.at("objectives").empty() || json.at("objectives").size() > 128)
				throw std::runtime_error("objectives must be a non-empty array with at most 128 entries");
			openrepublic::runtime::NativeObjectiveSet set;
			set.id = json.at("id").get<std::string>();
			set.title = json.at("title").get<std::string>();
			for (const auto& item : json.at("objectives"))
			{
				openrepublic::runtime::NativeObjective objective;
				objective.id = item.at("id").get<std::string>();
				objective.title = item.at("title").get<std::string>();
				objective.hint = item.at("hint").get<std::string>();
				const auto metric = item.at("metric").get<std::string>();
				if (metric == "inventory_at_least") objective.metric = openrepublic::runtime::ObjectiveMetric::InventoryAtLeast;
				else if (metric == "delivered_at_least") objective.metric = openrepublic::runtime::ObjectiveMetric::DeliveredAtLeast;
				else if (metric == "construction_sites_complete") objective.metric = openrepublic::runtime::ObjectiveMetric::ConstructionSitesComplete;
				else if (metric == "construction_sites_count") objective.metric = openrepublic::runtime::ObjectiveMetric::ConstructionSitesCount;
				else if (metric == "population_at_least") objective.metric = openrepublic::runtime::ObjectiveMetric::PopulationAtLeast;
				else if (metric == "building_count_at_least") objective.metric = openrepublic::runtime::ObjectiveMetric::BuildingCountAtLeast;
				else throw std::runtime_error("unknown objective metric");
				objective.buildingType = item.value("building_type", std::string{});
				objective.resource = item.value("resource", std::string{});
				objective.target = item.at("target").get<std::uint64_t>();
				set.objectives.push_back(std::move(objective));
			}
			nativeObjectiveSet = std::move(set);
		}
		catch (const std::exception& error)
		{
			return LogNativeEconomyStartupError(std::string("invalid objective scenario JSON: ") + error.what());
		}
		if (nativeObjectiveSet->id == "coal_demo") nativeCoalDemoArgumentSeen = true;
		else if (nativeObjectiveSet->id == "construction_sites_demo") nativeConstructionSitesArgumentSeen = true;
		if (campaignMission)
			nativeObjectiveSet->title = campaignMission->title + "\n" + campaignBriefing;
	}
	if (nativeContentCatalogArgumentSeen && (nativeCoalDemoArgumentSeen || nativeConstructionSitesArgumentSeen || nativeTownDemoArgumentSeen || nativeRailArgumentSeen))
		return LogNativeEconomyStartupError("--native-catalog-from-content cannot be combined with synthetic demo catalogs");
	if (nativeRailArgumentSeen && (nativeCoalDemoArgumentSeen || nativeConstructionSitesArgumentSeen || nativeTownDemoArgumentSeen))
		return LogNativeEconomyStartupError("--native-rail cannot be combined with other synthetic demo catalogs");
	if (ambientTrafficArgumentSeen && !nativeEconomyArgumentSeen)
		return LogNativeEconomyStartupError("--ambient-traffic requires --native-economy");
	if (nativeTrafficPolishArgumentSeen && !nativeEconomyArgumentSeen && !economyCheckpointArgumentSeen)
		return LogNativeEconomyStartupError("--native-traffic-polish requires --native-economy or --economy-checkpoint");
	if (nativeTerrainArgumentSeen && !nativeEconomyArgumentSeen && !economyCheckpointArgumentSeen)
		return LogNativeEconomyStartupError("--native-terrain requires --native-economy or --economy-checkpoint");
	if ((nativeEconomyArgumentSeen || economyCheckpointArgumentSeen) && !economyCheckpointIncompatibleOptions.empty())
	{
		std::string conflicts = nativeEconomyArgumentSeen
			? "--native-economy cannot be combined with"
			: "--economy-checkpoint cannot be combined with";
		for (const auto& option : economyCheckpointIncompatibleOptions)
		{
			conflicts += " --";
			for (std::size_t character = 2; character < option.size(); ++character)
				conflicts.push_back(static_cast<char>(option[character]));
		}
		return nativeEconomyArgumentSeen
			? LogNativeEconomyStartupError(conflicts)
			: LogCheckpointStartupError(conflicts);
	}
	if (!economyCheckpointArgumentError.empty())
	{
		return LogCheckpointStartupError(economyCheckpointArgumentError);
	}
	if (economyCheckpointArgumentSeen)
	{
		std::string checkpointText;
		std::string readError;
		openrepublic::runtime::SaveSlotInfo slotMetadata;
		if (!openrepublic::runtime::ReadSaveSlot(economyCheckpointPath, checkpointText, &slotMetadata, readError))
		{
			return LogCheckpointStartupError(readError);
		}
		economyCheckpoint = std::make_unique<openrepublic::economy::RestoredCheckpoint>();
		openrepublic::economy::ParseError parseError;
		if (!openrepublic::economy::RestoreCheckpoint(checkpointText, *economyCheckpoint, parseError) ||
			!economyCheckpoint->catalog || !economyCheckpoint->world)
		{
			return LogCheckpointStartupError(parseError.message.empty()
				? "checkpoint contents are invalid"
				: parseError.message);
		}
		openrepublic::runtime::NativeEconomyScenePlan plan;
		std::string layoutError;
		if (!openrepublic::runtime::BuildNativeEconomyScenePlan(economyCheckpoint->world->Snapshot(),
			*economyCheckpoint->catalog, plan, layoutError) ||
			!openrepublic::runtime::NativeEconomyPlanFitsFloatPrecision(plan, layoutError))
		{
			return LogCheckpointStartupError(layoutError);
		}
	}
	if (nativeEconomyArgumentSeen)
	{
		economyCheckpoint = std::make_unique<openrepublic::economy::RestoredCheckpoint>();
		if (nativeContentCatalogArgumentSeen)
		{
			openrepublic::compat::CatalogImportOptions importOptions;
			importOptions.buildingWhitelist = {"bakery", "coal_mine", "farm", "heating_plant",
				"power_plant", "storage"};
			economyCheckpoint->catalog = std::make_unique<openrepublic::economy::Catalog>();
			openrepublic::compat::ImportReport importReport;
			std::string importError;
			if (!openrepublic::compat::ImportCatalogFromContentDirectory(nativeContentCatalogPath,
				importOptions, *economyCheckpoint->catalog, importReport, importError))
				return LogNativeEconomyStartupError("content catalog import failed: " + importError);
			std::fprintf(stderr, "%s", openrepublic::compat::DescribeImport(importReport).c_str());
			if (importReport.buildingsImported == 0)
				return LogNativeEconomyStartupError("content catalog had no supported buildings in the curated allowlist");
		}
		else
		{
			if (campaignScenario)
			{
				economyCheckpoint->catalog = std::make_unique<openrepublic::economy::Catalog>();
				openrepublic::economy::ParseError parseError;
				if (!openrepublic::economy::ParseCatalogText(campaignScenario->at("catalog").get<std::string>(),
					*economyCheckpoint->catalog, parseError))
					return LogNativeEconomyStartupError("campaign catalog is invalid: " + parseError.message);
			}
			else
			{
				economyCheckpoint->catalog = std::make_unique<openrepublic::economy::Catalog>(
					nativeRailArgumentSeen ? openrepublic::economy::MakeSyntheticRailCatalog() :
					(nativeCoalDemoArgumentSeen ? openrepublic::economy::MakeSyntheticCoalCatalog() :
					(nativeConstructionSitesArgumentSeen ? openrepublic::economy::MakeSyntheticConstructionCatalog() :
					 (nativeTownDemoArgumentSeen ? openrepublic::economy::MakeSyntheticTownCatalog()
					                             : openrepublic::economy::MakeSyntheticCatalog()))));
			}
		}
		economyCheckpoint->world = std::make_unique<openrepublic::economy::World>(
			*economyCheckpoint->catalog, 64, 64);
		if (campaignScenario)
		{
			auto& world = *economyCheckpoint->world;
			const auto& catalog = *economyCheckpoint->catalog;
			std::vector<openrepublic::economy::CommandId> setupCommands;
			for (const auto& item : campaignScenario->at("buildings"))
			{
				const auto typeName = item.at("type").get<std::string>();
				openrepublic::economy::BuildingTypeId type{};
				if (!catalog.FindBuildingType(typeName, type))
					return LogNativeEconomyStartupError("campaign placement references an unknown building type");
				const openrepublic::economy::Cell cell{item.at("x").get<std::int32_t>(), item.at("z").get<std::int32_t>()};
				const auto command = item.value("constructionSite", false)
					? world.PlaceConstructionSite(type, cell) : world.PlaceBuilding(type, cell);
				if (!command) return LogNativeEconomyStartupError("campaign building placement could not be queued");
				setupCommands.push_back(command);
			}
			for (const auto& item : campaignScenario->at("roads"))
			{
				const openrepublic::economy::Cell a{item.at("a").at("x").get<std::int32_t>(),
					item.at("a").at("z").get<std::int32_t>()};
				const openrepublic::economy::Cell b{item.at("b").at("x").get<std::int32_t>(),
					item.at("b").at("z").get<std::int32_t>()};
				const auto dx = std::abs(a.x - b.x);
				const auto dz = std::abs(a.z - b.z);
				if (dx + dz != 1) return LogNativeEconomyStartupError("campaign roads must be adjacent grid cells");
				const auto command = world.BuildRoad(a, b);
				if (!command) return LogNativeEconomyStartupError("campaign road could not be queued");
				setupCommands.push_back(command);
			}
			if (!setupCommands.empty() && !world.Step(1).accepted)
				return LogNativeEconomyStartupError("campaign starting buildings and roads could not be applied");
			for (const auto command : setupCommands)
				if (world.GetCommandResult(command).state != openrepublic::economy::CommandState::Accepted)
					return LogNativeEconomyStartupError("campaign starting buildings or roads were rejected by the world");
			if (campaignScenario->contains("utilityConnections"))
			{
				std::vector<openrepublic::economy::CommandId> utilityCommands;
				for (const auto& item : campaignScenario->at("utilityConnections"))
				{
					const auto indexA = item.at("aBuilding").get<std::size_t>();
					const auto indexB = item.at("bBuilding").get<std::size_t>();
					if (indexA >= campaignScenario->at("buildings").size() || indexB >= campaignScenario->at("buildings").size())
						return LogNativeEconomyStartupError("campaign utility connection references an unknown building");
					openrepublic::economy::ResourceId resource{};
					if (!catalog.FindResource(item.at("resource").get<std::string>(), resource))
						return LogNativeEconomyStartupError("campaign utility connection references an unknown resource");
					const auto command = world.ConnectUtility(resource,
						static_cast<openrepublic::economy::BuildingId>(indexA + 1),
						static_cast<openrepublic::economy::BuildingId>(indexB + 1));
					if (!command) return LogNativeEconomyStartupError("campaign utility connection could not be queued");
					utilityCommands.push_back(command);
				}
				if (!utilityCommands.empty() && !world.Step(1).accepted)
					return LogNativeEconomyStartupError("campaign utility connections could not be applied");
				for (const auto command : utilityCommands)
					if (world.GetCommandResult(command).state != openrepublic::economy::CommandState::Accepted)
						return LogNativeEconomyStartupError("campaign utility connection was rejected by the world");
			}
		}
		else if (nativeCoalDemoArgumentSeen)
		{
			auto& world = *economyCheckpoint->world;
			const auto& catalog = *economyCheckpoint->catalog;
			for (const auto& item : std::array<std::pair<const char*, openrepublic::economy::Cell>, 4>{{
				{"road_depot", {1,3}}, {"border_post", {5,3}}, {"storage", {9,3}}, {"heating_plant", {13,3}}}})
			{
				openrepublic::economy::BuildingTypeId type{};
				if (!catalog.FindBuildingType(item.first, type) || !world.PlaceBuilding(type, item.second))
					return LogNativeEconomyStartupError("coal demo catalog or placement setup failed");
			}
			for (std::int32_t x = 1; x < 14; ++x)
				if (!world.BuildRoad({x,2}, {x+1,2})) return LogNativeEconomyStartupError("coal demo road setup failed");
			if (!world.Step(1).accepted || !world.Step(1).accepted)
				return LogNativeEconomyStartupError("coal demo initial setup step failed");
			openrepublic::economy::ResourceId grain{};
			if (!catalog.FindResource("grain", grain) || !world.TradeExport(2, grain, 4, 4) ||
				!world.Step(1).accepted || world.Treasury() < 4)
				return LogNativeEconomyStartupError("coal demo could not fund synthetic coal imports");
		}
		openrepublic::runtime::NativeEconomyScenePlan plan;
		std::string layoutError;
		if (!openrepublic::runtime::BuildNativeEconomyScenePlan(economyCheckpoint->world->Snapshot(),
			*economyCheckpoint->catalog, plan, layoutError) ||
			!openrepublic::runtime::NativeEconomyPlanFitsFloatPrecision(plan, layoutError))
		{
			return LogNativeEconomyStartupError(layoutError);
		}
	}
	if (utilityResourceGiven)
	{
		if (utilityResourceError.empty() && (mapPackage.empty() || !mapRegion || contentRoot.empty() ||
			economyUrlArgumentSeen || syntheticRegionFixture || !nmfCapturePaths.empty() || !mapShowcaseCapturePath.empty()))
			utilityResourceError = "--utility-resource requires an app-owned --map-package --map-region --content-root economy";
		if (!utilityResourceError.empty())
		{
			std::fprintf(stderr, "OpenRepublic utility startup failed: %s\n", utilityResourceError.c_str());
			return 2;
		}
	}
	if (storageProfileGiven)
	{
		if (storageProfileError.empty() && (mapPackage.empty() || !mapRegion || contentRoot.empty() ||
			economyUrlArgumentSeen || syntheticRegionFixture || !nmfCapturePaths.empty() || !mapShowcaseCapturePath.empty()))
			storageProfileError = "--storage-profile requires an app-owned --map-package --map-region --content-root economy";
		if (storageProfileError.empty())
			openrepublic::compat::LoadStorageCapacityProfile(storageProfilePath,
				regionCatalogOptions.storageCapacityFactors, storageProfileError);
		if (!storageProfileError.empty())
		{
			std::fprintf(stderr, "OpenRepublic storage profile startup failed: %s\n", storageProfileError.c_str());
			return 2;
		}
	}
	if (syntheticRegionFleetGiven && syntheticRegionFleetError.empty() &&
		(mapPackage.empty() || !mapRegion || contentRoot.empty() || economyUrlArgumentSeen ||
		 syntheticRegionFixture || nativeEconomyArgumentSeen || !nmfCapturePaths.empty() ||
		 !mapShowcaseCapturePath.empty()))
	{
		syntheticRegionFleetError = "--region-synthetic-fleet requires app-owned --map-package --map-region --content-root mode";
	}
	if (!syntheticRegionFleetError.empty())
	{
		std::fprintf(stderr, "OpenRepublic fleet startup failed: %s\n", syntheticRegionFleetError.c_str());
		return 2;
	}
	if (nmfCaptureArgumentError.empty() && mapBuildingModelLevelGiven && (mapPackage.empty() || !mapRegion))
	{
		nmfCaptureArgumentError = "--map-lod requires --map-package and --map-region";
	}
	if (nmfCaptureArgumentError.empty() && mapLodDistancesGiven && !mapBuildingDynamicLod)
	{
		nmfCaptureArgumentError = "--map-lod-distances requires --map-lod auto";
	}
	if (nmfCaptureArgumentError.empty() && localEconomyPipeNameGiven)
	{
		if (economyUrlArgumentSeen || economyPort > 0)
		{
			nmfCaptureArgumentError = "--economy-pipe-name cannot be combined with --economy-url";
		}
		else if (!(economyCheckpointArgumentSeen || nativeEconomyArgumentSeen) &&
			(mapPackage.empty() || !mapRegion))
		{
			nmfCaptureArgumentError = "--economy-pipe-name requires --map-package and --map-region, --economy-checkpoint, or --native-economy";
		}
		else if (!nmfCapturePaths.empty() || !mapShowcaseCapturePath.empty() || !sceneProbePath.empty() ||
			!glbProbePath.empty() || !nmfProbePath.empty() || !terrainProbePath.empty() ||
			terrainProbeTransform.has_value() || !buildingTypeName.empty())
		{
			nmfCaptureArgumentError = "--economy-pipe-name cannot be combined with probe or capture modes";
		}
	}
	if (nmfCaptureArgumentError.empty() && syntheticRegionFixture &&
		(!mapPackage.empty() || !contentRoot.empty() || mapRegion || mapBuildingIndexGiven ||
			mapBuildingModelLevelGiven || mapLodDistancesGiven || mapRadiusMeters != 0.0 || economyUrlArgumentSeen ||
			economyPort > 0 || localEconomyPipeNameGiven || !sceneProbePath.empty() ||
			!glbProbePath.empty() || !nmfProbePath.empty() || !terrainProbePath.empty() ||
			!buildingTypeName.empty() || !mapShowcaseCapturePath.empty() || !nmfCapturePaths.empty() ||
			terrainProbeTransform.has_value()))
	{
		nmfCaptureArgumentError = "--synthetic-region-fixture is a self-contained open-content mode and cannot be combined with map inputs, probes, capture modes, or economy endpoints";
	}
	if (nmfCaptureArgumentError.empty() && (mapShowcaseLookArgumentSeen || mapShowcaseCameraArgumentSeen) &&
		mapShowcaseCapturePath.empty())
	{
		nmfCaptureArgumentError = "--showcase-look and --showcase-camera require --showcase-capture";
	}
	if (nmfCaptureArgumentError.empty() && !mapShowcaseCapturePath.empty())
	{
		std::wstring extension = mapShowcaseCapturePath.extension().wstring();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) {
			return static_cast<wchar_t>(std::towlower(value));
		});
		if (mapPackage.empty() || !mapRegion || !nmfCapturePaths.empty() ||
			!terrainProbePath.empty() || !sceneProbePath.empty() || !glbProbePath.empty() ||
			!buildingTypeName.empty() || extension != L".png" || !mapShowcaseCapturePath.is_absolute() ||
			mapShowcaseCapturePath.parent_path().empty())
		{
			nmfCaptureArgumentError = "--showcase-capture requires --map-package with --map-region, one absolute .png output, and no other probe/capture mode";
		}
		else
		{
			std::error_code fileError;
			if (contentRoot.empty() || !contentRoot.is_absolute() ||
				!std::filesystem::is_directory(contentRoot, fileError) || fileError ||
				!mapPackage.is_absolute() || !std::filesystem::is_directory(mapPackage, fileError) || fileError)
			{
				nmfCaptureArgumentError = "--showcase-capture requires existing absolute --content-root and --map-package directories";
			}
			else
			{
				bool withinInputDirectory = false;
				std::string pathSafetyError;
				const bool contentPathResolved = PathIsSameOrWithinDirectory(
					mapShowcaseCapturePath, contentRoot, withinInputDirectory, pathSafetyError);
				const bool contentConflict = contentPathResolved && withinInputDirectory;
				bool mapPathResolved = false;
				bool mapConflict = false;
				if (!contentConflict && contentPathResolved)
				{
					mapPathResolved = PathIsSameOrWithinDirectory(
						mapShowcaseCapturePath, mapPackage, withinInputDirectory, pathSafetyError);
					mapConflict = mapPathResolved && withinInputDirectory;
				}
				if (!contentPathResolved || (!contentConflict && !mapPathResolved))
				{
					nmfCaptureArgumentError = "--showcase-capture could not verify that the output is outside source data: " + pathSafetyError;
				}
				else if (contentConflict || mapConflict)
				{
					nmfCaptureArgumentError = "--showcase-capture output must be outside --content-root and --map-package";
				}
			}
			if (nmfCaptureArgumentError.empty() &&
				(std::filesystem::exists(mapShowcaseCapturePath, fileError) || fileError))
			{
				nmfCaptureArgumentError = "--showcase-capture refuses to overwrite an existing or inaccessible output file";
			}
			else if (nmfCaptureArgumentError.empty())
			{
				std::filesystem::create_directories(mapShowcaseCapturePath.parent_path(), fileError);
				if (fileError || !std::filesystem::is_directory(mapShowcaseCapturePath.parent_path(), fileError) || fileError)
				{
					nmfCaptureArgumentError = "--showcase-capture could not create or inspect the output directory";
				}
			}
		}
	}
	std::optional<openrepublic::content::MapPlacementRequest> mapPlacementRequest;
	if (nmfCaptureArgumentError.empty() && !mapPackage.empty())
	{
		// Place one of a map package's buildings on its terrain. The package's
		// heightmap, script.ini and buildings_game.bin plus the content root's
		// building-type data resolve the model; it replaces --nmf-probe.
		std::error_code mapError;
		if (contentRoot.empty() || !buildingTypeName.empty() || !nmfProbePath.empty())
		{
			nmfCaptureArgumentError = "--map-package requires --content-root and cannot be combined with --building-type or --nmf-probe";
		}
		else if (!contentRoot.is_absolute() || !std::filesystem::is_directory(contentRoot, mapError) || mapError ||
			!mapPackage.is_absolute() || !std::filesystem::is_directory(mapPackage, mapError) || mapError)
		{
			nmfCaptureArgumentError = "--content-root and --map-package require existing absolute local directories";
		}
		else
		{
			openrepublic::content::MapPlacementRequest request;
			std::string mapResolveError;
			if (mapRegion && !mapBuildingIndexGiven)
			{
				// Centre the region on the busiest spot of the package.
				std::vector<openrepublic::content::PlacedBuilding> mapPlacements;
				if (openrepublic::content::LoadMapPlacements(mapPackage, mapPlacements, &mapResolveError))
				{
					mapBuildingIndex = openrepublic::content::FindDensestPlacement(
						mapPlacements, mapRadiusMeters > 0.0 ? mapRadiusMeters : 400.0);
				}
			}
			if (!openrepublic::content::ResolveMapPlacement(contentRoot, mapPackage, mapBuildingIndex, request, &mapResolveError, !mapRegion))
			{
				nmfCaptureArgumentError = "map placement could not be resolved: " + mapResolveError;
			}
			else
			{
				// A region view shows many models; the probe path only marks NMF preview mode.
				nmfProbePath = mapRegion ? mapPackage / "region.nmf" : request.assets.model;
				nmfMaterialPath = request.assets.material;
				nmfTextureRoots.insert(nmfTextureRoots.begin(), contentRoot);
				mapPlacementRequest = std::move(request);
			}
		}
	}
	else if (nmfCaptureArgumentError.empty() && (!buildingTypeName.empty() || !contentRoot.empty()))
	{
		// Resolve a building type from the content root's building-type data file
		// (OBSERVED location: buildings/buildingtypes.ini). The type's model and
		// MATERIAL replace --nmf-probe and the sibling-MTL lookup.
		std::error_code rootError;
		if (buildingTypeName.empty() || contentRoot.empty())
		{
			nmfCaptureArgumentError = "--building-type and --content-root must be supplied together";
		}
		else if (!nmfProbePath.empty())
		{
			nmfCaptureArgumentError = "--building-type cannot be combined with --nmf-probe";
		}
		else if (!contentRoot.is_absolute() || !std::filesystem::is_directory(contentRoot, rootError) || rootError)
		{
			nmfCaptureArgumentError = "--content-root requires an existing absolute local content directory";
		}
		else
		{
			const std::filesystem::path iniPath = contentRoot / "buildings" / "buildingtypes.ini";
			const std::uintmax_t iniBytes = std::filesystem::file_size(iniPath, rootError);
			std::string iniText;
			if (rootError || iniBytes > openrepublic::content::MaxBuildingTypesInputBytes)
			{
				nmfCaptureArgumentError = "buildings/buildingtypes.ini is missing or exceeds the 8 MiB profile limit";
			}
			else
			{
				iniText.resize(static_cast<std::size_t>(iniBytes));
				std::ifstream iniStream(iniPath, std::ios::binary);
				iniStream.read(iniText.data(), static_cast<std::streamsize>(iniText.size()));
				if (!iniStream || iniStream.gcount() != static_cast<std::streamsize>(iniText.size()))
				{
					nmfCaptureArgumentError = "buildings/buildingtypes.ini could not be read completely";
				}
			}
			openrepublic::content::BuildingTypesDocument buildingTypes;
			openrepublic::content::BuildingTypeAssetPaths assets;
			std::string buildingError;
			if (nmfCaptureArgumentError.empty() &&
				!openrepublic::content::ParseBuildingTypes(iniText, buildingTypes, &buildingError))
			{
				nmfCaptureArgumentError = "building-type data is outside the bounded profile: " + buildingError;
			}
			const auto* buildingType = nmfCaptureArgumentError.empty()
				? openrepublic::content::FindBuildingType(buildingTypes, buildingTypeName) : nullptr;
			if (nmfCaptureArgumentError.empty() && buildingType == nullptr)
			{
				nmfCaptureArgumentError = "building type was not found in buildings/buildingtypes.ini";
			}
			else if (nmfCaptureArgumentError.empty() &&
				!openrepublic::content::ResolveBuildingTypeAssets(
					*buildingType, buildingModelLevel, contentRoot, assets, &buildingError))
			{
				nmfCaptureArgumentError = "building type assets could not be resolved: " + buildingError;
			}
			else if (nmfCaptureArgumentError.empty())
			{
				nmfProbePath = assets.model;
				nmfMaterialPath = assets.material;
				nmfTextureRoots.insert(nmfTextureRoots.begin(), contentRoot);
			}
		}
	}
	if (nmfCaptureArgumentError.empty())
	{
		for (const auto& root : nmfTextureRoots)
		{
			std::error_code rootError;
			if (!root.is_absolute() || !std::filesystem::is_directory(root, rootError) || rootError)
			{
				nmfCaptureArgumentError = "--nmf-texture-root requires an existing absolute local content directory";
				break;
			}
		}
	}
	if (nmfCaptureArgumentError.empty())
	{
		if ((nmfCaptureZoomArgumentSeen || nmfCaptureElevationArgumentSeen) && nmfCapturePaths.empty())
		{
			nmfCaptureArgumentError = "capture-preview camera controls require at least one --capture-preview output";
		}
		else if (nmfUnlitPreview && (nmfProbePath.empty() || nmfCapturePaths.empty()))
		{
			nmfCaptureArgumentError = "--nmf-unlit-preview requires --nmf-probe <local.nmf> and at least one --capture-preview output";
		}
		else if (!nmfTextureRoots.empty() && nmfProbePath.empty())
		{
			nmfCaptureArgumentError = "--nmf-texture-root requires --nmf-probe <local.nmf>";
		}
		else if (!nmfTextureRoots.empty() && nmfUnlitPreview)
		{
			nmfCaptureArgumentError = "--nmf-texture-root cannot be combined with --nmf-unlit-preview";
		}
		else if (nmfMaterialUnlitPreview &&
			(nmfProbePath.empty() || nmfCapturePaths.empty()))
		{
			nmfCaptureArgumentError = "--nmf-material-unlit-preview requires --nmf-probe <local.nmf> and at least one --capture-preview output";
		}
		else if (nmfMaterialUnlitPreview && nmfUnlitPreview)
		{
			nmfCaptureArgumentError = "--nmf-material-unlit-preview cannot be combined with --nmf-unlit-preview";
		}
		else if (nmfMaterialUnlitPreview && !terrainProbePath.empty())
		{
			nmfCaptureArgumentError = "--nmf-material-unlit-preview cannot be combined with --terrain-probe";
		}
		else if (nmfUnlitPreview && !terrainProbePath.empty())
		{
			nmfCaptureArgumentError = "--nmf-unlit-preview cannot be combined with --terrain-probe";
		}
	}
	if (terrainTransformArgumentSeen && terrainProbePath.empty() && terrainProbeTransformError.empty())
	{
		terrainProbeTransformError = "--terrain-transform requires --terrain-probe <heightmap.dds>";
	}
	if (!nmfCapturePaths.empty() && nmfCaptureArgumentError.empty())
	{
		if (nmfProbePath.empty())
		{
			nmfCaptureArgumentError = "--capture-preview requires --nmf-probe <local.nmf>";
		}
		else if (!terrainProbePath.empty())
		{
			nmfCaptureArgumentError = "--capture-preview cannot be combined with --terrain-probe";
		}
		else if (!sceneProbePath.empty() || !glbProbePath.empty())
		{
			nmfCaptureArgumentError = "--capture-preview requires an isolated NMF scene and cannot be combined with --scene-probe or --glb-probe";
		}
		for (std::size_t i = 0; i < nmfCapturePaths.size() && nmfCaptureArgumentError.empty(); ++i)
		{
			const auto& path = nmfCapturePaths[i];
			std::wstring extension = path.extension().wstring();
			std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) {
				return static_cast<wchar_t>(std::towlower(value));
			});
			if (!path.is_absolute() || extension != L".png" || path.parent_path().empty())
			{
				nmfCaptureArgumentError = "--capture-preview requires an absolute output path ending in .png";
				break;
			}
			for (std::size_t earlier = 0; earlier < i; ++earlier)
			{
				if (path.lexically_normal() == nmfCapturePaths[earlier].lexically_normal())
				{
					nmfCaptureArgumentError = "--capture-preview output paths must be distinct";
					break;
				}
			}
			if (!nmfCaptureArgumentError.empty())
			{
				break;
			}
			std::error_code fileError;
			if (std::filesystem::exists(path, fileError) || fileError)
			{
				nmfCaptureArgumentError = "--capture-preview refuses to overwrite an existing or inaccessible output file";
				break;
			}
			std::filesystem::create_directories(path.parent_path(), fileError);
			if (fileError || !std::filesystem::is_directory(path.parent_path(), fileError) || fileError)
			{
				nmfCaptureArgumentError = "--capture-preview could not create or inspect the output directory";
				break;
			}
		}
	}
	if (!nmfCaptureArgumentError.empty())
	{
		MessageBoxA(nullptr, nmfCaptureArgumentError.c_str(), "OpenRepublic content preview options", MB_OK | MB_ICONERROR);
		return 2;
	}
	if (economyCheckpoint)
	{
		if (nativeObjectiveSet) application.SetNativeObjectives(std::move(*nativeObjectiveSet));
		application.SetEconomyCheckpoint(std::move(*economyCheckpoint), nativeEconomyArgumentSeen);
		if (campaignDefinition && campaignProgress && campaignMission)
			application.SetCampaignProgress(std::move(*campaignDefinition), std::move(*campaignProgress),
				campaignMission->id, std::move(campaignProgressPath), 0);
		application.SetNativeTerrainEnabled(nativeTerrainArgumentSeen);
		application.SetNativeRailEnabled(nativeRailArgumentSeen);
	}
	if (autoplayArgumentSeen) application.SetAutoplay(autoplaySpeed);
	if (!uiScript.actions.empty()) application.SetUiScript(std::move(uiScript));
	application.SetSceneProbePath(std::move(sceneProbePath));
	application.SetGlbProbePath(std::move(glbProbePath));
	application.SetNmfProbePath(std::move(nmfProbePath));
	application.SetNmfTextureRoots(std::move(nmfTextureRoots));
	application.SetNmfMaterialPath(std::move(nmfMaterialPath));
	if (graphicsSettings)
	{
		application.SetGraphicsSettings(*graphicsSettings);
	}
	if (economyPort > 0)
	{
		application.SetEconomyOverlay(economyHost, economyPort);
	}
	if (!localEconomyPipeName.empty())
	{
		application.SetLocalEconomyPipeName(std::move(localEconomyPipeName));
	}
	if (syntheticRegionFixture)
	{
		application.SetSyntheticRegionFixture();
	}
	if (mapPlacementRequest)
	{
		application.SetRegionCatalogImportOptions(std::move(regionCatalogOptions));
		application.SetMapPlacement(std::move(*mapPlacementRequest), mapRadiusMeters, mapRegion,
			contentRoot, mapBuildingModelLevel, mapBuildingDynamicLod, mapLodNearDistance, mapLodFarDistance);
	}
	application.SetNmfUnlitPreview(nmfUnlitPreview);
	application.SetNmfMaterialUnlitPreview(nmfMaterialUnlitPreview);
	application.SetNmfCaptureZoom(static_cast<float>(nmfCaptureZoom));
	if (nmfCaptureElevationDegrees.has_value())
	{
		application.SetNmfCaptureElevationDegrees(
			static_cast<float>(*nmfCaptureElevationDegrees));
	}
	const bool nmfCaptureMode = !nmfCapturePaths.empty();
	const bool mapShowcaseCaptureMode = !mapShowcaseCapturePath.empty();
	application.SetNmfCapturePaths(std::move(nmfCapturePaths));
	application.SetMapShowcaseSun(mapShowcaseSunElevationDegrees, mapShowcaseSunYawOffsetDegrees);
	application.SetScenePipeName(scenePipeName);
	application.SetSceneScriptPath(sceneScriptPath);
	application.SetMapShowcaseCapture(std::move(mapShowcaseCapturePath), mapShowcaseEnhancedLook,
		mapShowcaseCameraYawDegrees, mapShowcaseCameraElevationDegrees, mapShowcaseCameraDistanceScale,
		showcaseVegetationWind, showcaseGrassLayer);
	application.SetTerrainProbePath(std::move(terrainProbePath));
	application.SetTerrainProbeTransform(std::move(terrainProbeTransform), std::move(terrainProbeTransformError));
	application.SetAmbientTraffic(ambientTrafficArgumentSeen);
	application.SetNativeTrafficPolish(nativeTrafficPolishArgumentSeen);

	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = L"OpenRepublicMainWindow";
	if (RegisterClassExW(&windowClass) == 0)
	{
		return static_cast<int>(GetLastError());
	}

	HWND window = CreateWindowExW(
		0,
		windowClass.lpszClassName,
		L"OpenRepublic | WASD/Arrows move, Space/Ctrl rise/lower, wheel zoom, click select, Esc clear",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		nativeEconomyArgumentSeen ? 1600 : 1280,
		nativeEconomyArgumentSeen ? 1000 : 800,
		nullptr, nullptr, instance, nullptr
	);
	if (window == nullptr)
	{
		return static_cast<int>(GetLastError());
	}

	application.SetWindow(window);
	if (nmfCaptureMode || mapShowcaseCaptureMode)
	{
		ShowWindow(window, SW_SHOWNOACTIVATE);
	}
	else
	{
		ShowWindow(window, showCommand);
	}
	UpdateWindow(window);

	MSG message{};
	while (message.message != WM_QUIT)
	{
		if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		else
		{
			application.Run();
		}
	}

	wi::jobsystem::ShutDown();
	return static_cast<int>(message.wParam);
}
