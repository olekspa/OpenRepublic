#include "OpenRepublicApplication.h"
#include "openrepublic/content/mtl_dependencies.h"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <cwctype>
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
	std::vector<std::filesystem::path> nmfTextureRoots;
	bool nmfUnlitPreview = false;
	bool nmfMaterialUnlitPreview = false;
	std::vector<std::filesystem::path> nmfCapturePaths;
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
	application.SetNmfUnlitPreview(nmfUnlitPreview);
	application.SetNmfMaterialUnlitPreview(nmfMaterialUnlitPreview);
	application.SetNmfCaptureZoom(static_cast<float>(nmfCaptureZoom));
	if (nmfCaptureElevationDegrees.has_value())
	{
		application.SetNmfCaptureElevationDegrees(
			static_cast<float>(*nmfCaptureElevationDegrees));
	}
	const bool nmfCaptureMode = !nmfCapturePaths.empty();
	application.SetNmfCapturePaths(std::move(nmfCapturePaths));
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
	if (nmfCaptureMode)
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
