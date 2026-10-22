#include "OpenRepublicApplication.h"
#include "openrepublic/content/building_types.h"
#include "openrepublic/content/mtl_dependencies.h"
#include "openrepublic/runtime/graphics_settings.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	openrepublic::OpenRepublicApplication application;

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
	std::filesystem::path sceneProbePath;
	std::filesystem::path glbProbePath;
	std::filesystem::path nmfProbePath;
	std::filesystem::path nmfMaterialPath;
	std::filesystem::path contentRoot;
	std::filesystem::path mapPackage;
	std::size_t mapBuildingIndex = 0;
	bool mapBuildingIndexGiven = false;
	bool mapRegion = false;
	bool syntheticRegionFixture = false;
	double mapRadiusMeters = 0.0;
	int mapBuildingModelLevel = 0;
	bool mapBuildingModelLevelGiven = false;
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
	float mapShowcaseSunElevationDegrees = std::numeric_limits<float>::quiet_NaN();
	float mapShowcaseSunYawOffsetDegrees = 0.0f;
	double nmfCaptureZoom = 1.0;
	bool nmfCaptureZoomArgumentSeen = false;
	std::optional<double> nmfCaptureElevationDegrees;
	bool nmfCaptureElevationArgumentSeen = false;
	std::filesystem::path terrainProbePath;
	std::optional<openrepublic::content::TerrainTransform> terrainProbeTransform;
	std::string terrainProbeTransformError;
	std::string nmfCaptureArgumentError;
	bool terrainTransformArgumentSeen = false;
	if (arguments != nullptr)
	{
		for (int i = 1; i < argumentCount; ++i)
		{
			if (std::wstring_view(arguments[i]) == L"--scene-probe" && i + 1 < argumentCount)
			{
				sceneProbePath = arguments[++i];
			}
			else if (std::wstring_view(arguments[i]) == L"--glb-probe" && i + 1 < argumentCount)
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
					nmfCaptureArgumentError = "--map-lod requires 0, 1 or 2";
				}
				else
				{
					const std::wstring_view level = arguments[++i];
					if (level == L"0" || level == L"1" || level == L"2")
					{
						mapBuildingModelLevel = level[0] - L'0';
						mapBuildingModelLevelGiven = true;
					}
					else
					{
						nmfCaptureArgumentError = "--map-lod requires 0, 1 or 2";
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
			else if (std::wstring_view(arguments[i]) == L"--modern")
			{
				// Interactive next-generation look for the region view (same preset as the showcase
				// captures: physically based sky, lake, relief, trees, props, post effects).
				mapShowcaseEnhancedLook = true;
				if (!std::isfinite(mapShowcaseSunElevationDegrees))
				{
					mapShowcaseSunElevationDegrees = 17.0f;
					mapShowcaseSunYawOffsetDegrees = 45.0f;
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
	if (nmfCaptureArgumentError.empty() && mapBuildingModelLevelGiven && (mapPackage.empty() || !mapRegion))
	{
		nmfCaptureArgumentError = "--map-lod requires --map-package and --map-region";
	}
	if (nmfCaptureArgumentError.empty() && localEconomyPipeNameGiven)
	{
		if (economyUrlArgumentSeen || economyPort > 0)
		{
			nmfCaptureArgumentError = "--economy-pipe-name cannot be combined with --economy-url";
		}
		else if (mapPackage.empty() || !mapRegion)
		{
			nmfCaptureArgumentError = "--economy-pipe-name requires --map-package and --map-region";
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
			mapBuildingModelLevelGiven || mapRadiusMeters != 0.0 || economyUrlArgumentSeen ||
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
		application.SetMapPlacement(std::move(*mapPlacementRequest), mapRadiusMeters, mapRegion,
			contentRoot, mapBuildingModelLevel);
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
	application.SetMapShowcaseCapture(std::move(mapShowcaseCapturePath), mapShowcaseEnhancedLook,
		mapShowcaseCameraYawDegrees, mapShowcaseCameraElevationDegrees, mapShowcaseCameraDistanceScale);
	application.SetTerrainProbePath(std::move(terrainProbePath));
	application.SetTerrainProbeTransform(std::move(terrainProbeTransform), std::move(terrainProbeTransformError));

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
		CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
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
