#include "OpenRepublicApplication.h"
#include "ModelImporter.h"
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
#include <sstream>
#include <string>
#include <string_view>
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
		constexpr double kTerrainDisplayUnitsPerSourceUnit = 0.001;
		constexpr double kTerrainVerticalPreviewExaggeration = 5.0;
		constexpr double kTerrainVerticalPreviewScale = 0.005;
		constexpr double kRawPreviewVerticalScale = 5.0;
		constexpr double kMaxTerrainPreviewHeight = 1000.0;
		constexpr std::uintmax_t kMaxNmfPreviewMtlBytes = 4u * 1024u * 1024u;
		constexpr std::uintmax_t kMaxNmfPreviewTextureBytes = 64u * 1024u * 1024u;
		constexpr std::uintmax_t kMaxNmfPreviewTextureBytesTotal = 256u * 1024u * 1024u;
		constexpr XMFLOAT4 kNmfDebugMaterialPalette[] = {
			XMFLOAT4(0.70f, 0.35f, 0.24f, 1.0f),
			XMFLOAT4(0.28f, 0.52f, 0.72f, 1.0f),
			XMFLOAT4(0.36f, 0.61f, 0.34f, 1.0f),
			XMFLOAT4(0.78f, 0.65f, 0.36f, 1.0f),
			XMFLOAT4(0.54f, 0.40f, 0.68f, 1.0f),
			XMFLOAT4(0.30f, 0.64f, 0.62f, 1.0f),
			XMFLOAT4(0.76f, 0.43f, 0.59f, 1.0f),
			XMFLOAT4(0.58f, 0.56f, 0.48f, 1.0f),
		};
		// Current synthetic storage capacity also shown by the demo stock readout.
		constexpr std::uint32_t kStorageFlourCapacity = 5;
		constexpr std::uintmax_t kMaxTerrainMetadataBytes = 64u * 1024u;
		constexpr char kGlbProbeLimitations[] =
			"Trusted local inputs only: a bounded embedded-resource profile is checked before importing an exact temporary snapshot; "
			"the pinned importer and image decoders are not sandboxed, and this is not a licensing assessment.";

		class NmfPreviewCaptureRenderPath final : public wi::RenderPath3D
		{
		public:
			explicit NmfPreviewCaptureRenderPath(std::vector<std::filesystem::path> outputPaths,
				const XMFLOAT3& cameraTarget, float cameraDistance, float cameraZoom,
				std::optional<float> cameraElevationDegrees)
				: outputPaths(std::move(outputPaths)), cameraTarget(cameraTarget), cameraDistance(cameraDistance),
				cameraZoom(cameraZoom), cameraElevationDegrees(cameraElevationDegrees)
			{
			}

			void SetDiagnosticCameraView(std::size_t index)
			{
				// Preserve the existing first five diagnostic views, then complete a
				// uniform-elevation cardinal sweep so an unknown source front axis is
				// less likely to hide the facade from every capture. Two quarter-step
				// views around -Z provide a closer three-quarter reference candidate.
				static constexpr std::array<XMFLOAT3, 11> cameraDirections = {
					XMFLOAT3(0.6432f, 0.4153f, 0.6432f),  // diagonal A
					XMFLOAT3(1.0f, 0.0f, 0.0f),          // level +X diagnostic side
					XMFLOAT3(-0.6432f, 0.4153f, 0.6432f), // diagonal B
					XMFLOAT3(0.6432f, 0.4153f, -0.6432f), // diagonal C, opposite Z side
					XMFLOAT3(-0.6432f, 0.4153f, -0.6432f), // diagonal D, opposite Z side
					XMFLOAT3(0.9096f, 0.4153f, 0.0f),    // elevated +X
					XMFLOAT3(0.0f, 0.4153f, 0.9096f),    // elevated +Z
					XMFLOAT3(-0.9096f, 0.4153f, 0.0f),   // elevated -X
					XMFLOAT3(0.0f, 0.4153f, -0.9096f),   // elevated -Z
					XMFLOAT3(0.3482f, 0.4153f, -0.8404f), // -Z oblique from +X
					XMFLOAT3(-0.3482f, 0.4153f, -0.8404f), // -Z oblique from -X
				};

				if (camera == nullptr || index >= cameraDirections.size())
				{
					return;
				}

				const float distance = std::max(cameraDistance, 12.0f) / cameraZoom;
				XMFLOAT3 direction = cameraDirections[index];
				if (cameraElevationDegrees.has_value() && index != 1)
				{
					// Rebuild only the pitch: retain each preset's horizontal azimuth.
					// With no override, the historical direction vectors remain exact.
					constexpr float degreesToRadians = 0.01745329251994329577f;
					const float horizontalLength = std::hypot(direction.x, direction.z);
					if (horizontalLength > 0.0f && std::isfinite(horizontalLength))
					{
						const float elevation = *cameraElevationDegrees * degreesToRadians;
						const float horizontalScale = std::cos(elevation) / horizontalLength;
						direction.x *= horizontalScale;
						direction.y = std::sin(elevation);
						direction.z *= horizontalScale;
					}
				}
				const XMFLOAT3 eye(cameraTarget.x + distance * direction.x,
					cameraTarget.y + distance * direction.y,
					cameraTarget.z + distance * direction.z);
				camera->Eye = eye;
				XMStoreFloat3(&camera->At,
					XMVector3Normalize(XMLoadFloat3(&cameraTarget) - XMLoadFloat3(&eye)));
				camera->Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
				camera->UpdateCamera();
			}

			void PostRender() override
			{
				// Let renderer startup and asynchronous PSO creation settle for a bounded
				// number of complete frames before readback.
				if (warmupFramesRemaining > 0)
				{
					--warmupFramesRemaining;
					if (warmupFramesRemaining == 0)
					{
						// Apply view A after path activation and let its next full frame render
						// before the first readback.
						SetDiagnosticCameraView(0);
					}
					return;
				}
				if (nextCapture >= outputPaths.size())
				{
					return;
				}

				std::uint64_t coveredPixels = 0;
				std::string coverageError;
				if (!CountOpaqueDepthCoverage(depthBuffer_Main, coveredPixels, coverageError))
				{
					wilog("[OpenRepublic] NMF preview raster coverage unavailable for view %zu: %s",
						nextCapture, coverageError.c_str());
					PostQuitMessage(1);
					nextCapture = outputPaths.size();
					return;
				}
				if (coveredPixels == 0)
				{
					wilog("[OpenRepublic] NMF preview has no opaque depth coverage in view %zu; capture rejected",
						nextCapture);
					PostQuitMessage(1);
					nextCapture = outputPaths.size();
					return;
				}
				const wi::graphics::TextureDesc& desc = depthBuffer_Main.GetDesc();
				const double coverage = 100.0 * static_cast<double>(coveredPixels) /
					(static_cast<double>(desc.width) * static_cast<double>(desc.height));
				wilog("[OpenRepublic] NMF preview raster coverage: view=%zu pixels=%llu/%llu (%.3f%%) depth=D32_FLOAT_S8X24_UINT",
					nextCapture,
					static_cast<unsigned long long>(coveredPixels),
					static_cast<unsigned long long>(desc.width) * desc.height,
					coverage);

				const auto& outputPath = outputPaths[nextCapture];
				const std::string filename = outputPath.string();
				if (!wi::helper::saveTextureToFile(GetRenderResult3D(), filename))
				{
					wilog("[OpenRepublic] NMF preview capture failed for %s", outputPath.filename().string().c_str());
					PostQuitMessage(1);
					nextCapture = outputPaths.size();
					return;
				}

				wilog("[OpenRepublic] NMF preview capture saved: %s (3D render output; GUI excluded)",
					outputPath.filename().string().c_str());
				++nextCapture;
				if (nextCapture < outputPaths.size())
				{
					SetDiagnosticCameraView(nextCapture);
					return;
				}
				PostQuitMessage(0);
			}

		private:
			static bool CountOpaqueDepthCoverage(const wi::graphics::Texture& depthTexture,
				std::uint64_t& coveredPixels, std::string& error)
			{
				using wi::graphics::Format;
				const wi::graphics::TextureDesc& desc = depthTexture.GetDesc();
				if (!depthTexture.IsValid() || desc.format != Format::D32_FLOAT_S8X24_UINT ||
					desc.sample_count != 1 || desc.width == 0 || desc.height == 0 ||
					desc.mip_levels != 1 || desc.array_size != 1)
				{
					error = "main depth buffer does not match the single-sample D32_FLOAT_S8X24_UINT profile";
					return false;
				}

				const std::uint64_t pixelCount = static_cast<std::uint64_t>(desc.width) * desc.height;
				if (pixelCount > std::numeric_limits<std::size_t>::max() / sizeof(float))
				{
					error = "depth plane size exceeds the host address range";
					return false;
				}
				const std::size_t depthPlaneBytes = static_cast<std::size_t>(pixelCount) * sizeof(float);
				wi::vector<std::uint8_t> readback;
				if (!wi::helper::saveTextureToMemory(depthTexture, readback) || readback.size() < depthPlaneBytes)
				{
					error = "Wicked Engine depth readback failed or returned a truncated depth plane";
					return false;
				}

				coveredPixels = 0;
				for (std::size_t pixelIndex = 0; pixelIndex < static_cast<std::size_t>(pixelCount); ++pixelIndex)
				{
					float depth = 0.0f;
					std::memcpy(&depth, readback.data() + pixelIndex * sizeof(float), sizeof(depth));
					if (!std::isfinite(depth) || depth < 0.0f || depth > 1.0f)
					{
						error = "depth plane contains a value outside the finite [0, 1] range";
						return false;
					}
					// Wicked's main camera uses reversed Z with a 0.0 clear depth.
					if (depth > 0.0f)
					{
						++coveredPixels;
					}
				}
				return true;
			}

			std::vector<std::filesystem::path> outputPaths;
			XMFLOAT3 cameraTarget{};
			float cameraDistance = 12.0f;
			float cameraZoom = 1.0f;
			std::optional<float> cameraElevationDegrees;
			std::uint32_t warmupFramesRemaining = 60;
			std::size_t nextCapture = 0;
		};

		std::string PendingTruckPlanCommandLabel(const openrepublic::agent_api::AgentApi& api,
			std::uint64_t simulationCommandId)
		{
			if (const auto apiHandle = api.LookupApiCommandHandleForSimulationCommand(simulationCommandId))
			{
				return "API command " + std::to_string(*apiHandle);
			}
			return "simulation command ID " + std::to_string(simulationCommandId) + " (no API handle)";
		}

		const char* TerrainOrientationLabel(openrepublic::content::RasterOrientation orientation)
		{
			using openrepublic::content::RasterOrientation;
			switch (orientation)
			{
			case RasterOrientation::Identity: return "identity";
			case RasterOrientation::FlipX: return "flip-X";
			case RasterOrientation::FlipZ: return "flip-Z";
			case RasterOrientation::Rotate180: return "rotate-180";
			case RasterOrientation::Transpose: return "transpose";
			case RasterOrientation::TransposeFlipX: return "transpose + flip-X";
			case RasterOrientation::TransposeFlipZ: return "transpose + flip-Z";
			case RasterOrientation::TransposeRotate180: return "transpose + rotate-180";
			default: return "invalid";
			}
		}

		bool HasValidIndexedTriangleData(const wi::scene::MeshComponent& mesh)
		{
			if (mesh.vertex_positions.size() < 3 || mesh.indices.size() < 3 || mesh.indices.size() % 3 != 0)
			{
				return false;
			}

			for (const XMFLOAT3& position : mesh.vertex_positions)
			{
				if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
				{
					return false;
				}
			}

			for (const std::uint32_t index : mesh.indices)
			{
				if (index >= mesh.vertex_positions.size())
				{
					return false;
				}
			}

			return true;
		}

		bool HasValidMeshSubsetRanges(const wi::scene::MeshComponent& mesh)
		{
			for (const auto& subset : mesh.subsets)
			{
				if (subset.indexOffset > mesh.indices.size() ||
					subset.indexCount > mesh.indices.size() - subset.indexOffset ||
					subset.indexCount % 3 != 0)
				{
					return false;
				}
			}
			return !mesh.subsets.empty();
		}

		const XMFLOAT4& NmfDebugMaterialColor(std::size_t index)
		{
			return kNmfDebugMaterialPalette[index % std::size(kNmfDebugMaterialPalette)];
		}

		bool WindowsPathComponentEqual(const std::filesystem::path& left,
			const std::filesystem::path& right)
		{
			const auto& leftNative = left.native();
			const auto& rightNative = right.native();
			if (leftNative.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
				rightNative.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				return false;
			}
			return CompareStringOrdinal(leftNative.data(), static_cast<int>(leftNative.size()),
				rightNative.data(), static_cast<int>(rightNative.size()), TRUE) == CSTR_EQUAL;
		}

		bool WindowsPathIsWithin(const std::filesystem::path& candidate,
			const std::filesystem::path& root)
		{
			auto rootComponent = root.begin();
			auto candidateComponent = candidate.begin();
			for (; rootComponent != root.end(); ++rootComponent, ++candidateComponent)
			{
				if (candidateComponent == candidate.end() ||
					!WindowsPathComponentEqual(*candidateComponent, *rootComponent))
				{
					return false;
				}
			}
			return true;
		}

		bool WindowsPathsEqual(const std::filesystem::path& left,
			const std::filesystem::path& right)
		{
			auto leftComponent = left.begin();
			auto rightComponent = right.begin();
			for (; leftComponent != left.end() && rightComponent != right.end();
				++leftComponent, ++rightComponent)
			{
				if (!WindowsPathComponentEqual(*leftComponent, *rightComponent))
				{
					return false;
				}
			}
			return leftComponent == left.end() && rightComponent == right.end();
		}

		std::filesystem::path NormalizeHandlePath(std::wstring path)
		{
			constexpr std::wstring_view uncPrefix = L"\\\\?\\UNC\\";
			constexpr std::wstring_view devicePrefix = L"\\\\?\\";
			if (path.compare(0, uncPrefix.size(), uncPrefix) == 0)
			{
				path = L"\\\\" + path.substr(uncPrefix.size());
			}
			else if (path.compare(0, devicePrefix.size(), devicePrefix) == 0)
			{
				path.erase(0, devicePrefix.size());
			}
			return std::filesystem::path(std::move(path));
		}

		struct ScopedFileHandle
		{
			HANDLE value = INVALID_HANDLE_VALUE;

			~ScopedFileHandle()
			{
				if (value != INVALID_HANDLE_VALUE)
				{
					CloseHandle(value);
				}
			}
		};

		bool ReadNmfTextureFileContained(const std::filesystem::path& expectedPath,
			const std::vector<std::filesystem::path>& trustedRoots, std::size_t maximumBytes,
			std::vector<std::uint8_t>& output, std::string& error)
		{
			ScopedFileHandle file;
			file.value = CreateFileW(expectedPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
			if (file.value == INVALID_HANDLE_VALUE)
			{
				error = "texture candidate could not be opened read-only";
				return false;
			}

			std::vector<wchar_t> finalPathBuffer(32768, L'\0');
			const DWORD finalPathLength = GetFinalPathNameByHandleW(file.value,
				finalPathBuffer.data(), static_cast<DWORD>(finalPathBuffer.size()),
				FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
			if (finalPathLength == 0 || finalPathLength >= finalPathBuffer.size())
			{
				error = "opened texture path could not be verified";
				return false;
			}
			std::error_code pathError;
			const auto openedPath = std::filesystem::canonical(
				NormalizeHandlePath(std::wstring(finalPathBuffer.data(), finalPathLength)), pathError);
			if (pathError)
			{
				error = "opened texture path could not be canonicalized";
				return false;
			}
			const auto expectedCanonicalPath = std::filesystem::canonical(expectedPath, pathError);
			if (pathError || !WindowsPathsEqual(openedPath, expectedCanonicalPath))
			{
				error = "opened texture does not match the resolved local candidate";
				return false;
			}

			bool contained = false;
			for (const auto& root : trustedRoots)
			{
				const auto canonicalRoot = std::filesystem::canonical(root, pathError);
				if (!pathError && WindowsPathIsWithin(openedPath, canonicalRoot))
				{
					contained = true;
					break;
				}
				pathError.clear();
			}
			if (!contained)
			{
				error = "opened texture escaped caller-supplied content roots";
				return false;
			}

			FILE_STANDARD_INFO fileInfo{};
			if (!GetFileInformationByHandleEx(file.value, FileStandardInfo, &fileInfo, sizeof(fileInfo)) ||
				fileInfo.Directory || fileInfo.EndOfFile.QuadPart <= 0 ||
				static_cast<unsigned long long>(fileInfo.EndOfFile.QuadPart) > maximumBytes)
			{
				error = "texture file is empty, non-regular, or exceeds the bounded image input size";
				return false;
			}

			std::vector<std::uint8_t> candidate(static_cast<std::size_t>(fileInfo.EndOfFile.QuadPart));
			std::size_t offset = 0;
			while (offset < candidate.size())
			{
				const DWORD requestBytes = static_cast<DWORD>(std::min<std::size_t>(
					candidate.size() - offset, std::numeric_limits<DWORD>::max()));
				DWORD bytesRead = 0;
				if (!ReadFile(file.value, candidate.data() + offset, requestBytes, &bytesRead, nullptr) ||
					bytesRead == 0)
				{
					error = "texture file could not be read completely from the verified handle";
					return false;
				}
				offset += bytesRead;
			}

			output = std::move(candidate);
			error.clear();
			return true;
		}

		bool IsSupportedNmfPreviewImageExtension(const std::filesystem::path& path)
		{
			std::string extension = path.extension().string();
			if (extension.empty() || extension.size() > 16 || extension.front() != '.')
				return false;
			extension.erase(extension.begin());
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
				return static_cast<char>(std::toupper(value));
			});
			for (std::string supported : wi::resourcemanager::GetSupportedImageExtensions())
			{
				if (!supported.empty() && supported.front() == '.')
					supported.erase(supported.begin());
				std::transform(supported.begin(), supported.end(), supported.begin(), [](unsigned char value) {
					return static_cast<char>(std::toupper(value));
				});
				if (extension == supported)
					return true;
			}
			return false;
		}

		bool LoadNmfPreviewTexture(const std::filesystem::path& resolvedPath,
			const std::vector<std::filesystem::path>& trustedRoots, std::size_t remainingBytes,
			wi::Resource& resource, std::string& resourceName, std::size_t& loadedBytes,
			std::string& error)
		{
			if (!IsSupportedNmfPreviewImageExtension(resolvedPath))
			{
				error = "texture candidate extension is not supported by the pinned image loader";
				return false;
			}

			std::vector<std::uint8_t> fileBytes;
			const std::size_t maximumBytes = std::min<std::size_t>(
				static_cast<std::size_t>(kMaxNmfPreviewTextureBytes), remainingBytes);
			if (maximumBytes == 0 || !ReadNmfTextureFileContained(
				resolvedPath, trustedRoots, maximumBytes, fileBytes, error))
			{
				if (error.empty())
					error = "aggregate preview texture memory limit was reached";
				return false;
			}

			static std::atomic<std::uint64_t> nextResourceId{0};
			const std::string generatedName = "openrepublic-nmf-preview-texture-" +
				std::to_string(nextResourceId.fetch_add(1, std::memory_order_relaxed)) +
				resolvedPath.extension().string();
			auto candidateResource = wi::resourcemanager::Load(generatedName,
				wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA,
				fileBytes.data(), fileBytes.size());
			if (!candidateResource.IsValid() || !candidateResource.GetTexture().IsValid())
			{
				error = "pinned image loader rejected the bounded local texture bytes";
				return false;
			}

			resource = std::move(candidateResource);
			resourceName = generatedName;
			loadedBytes = fileBytes.size();
			error.clear();
			return true;
		}

		// Directory of the sibling MTL relative to the first supplied content root that
		// contains it (generic '/' separators). Empty when no root yields a non-empty
		// relative directory; `$TEXTURE_MTL` entries then stay unresolved.
		std::string MtlDirectoryRelativeToRoots(const std::filesystem::path& nmfPath,
			const std::vector<std::filesystem::path>& roots)
		{
			std::error_code error;
			std::filesystem::path mtlPath = nmfPath;
			mtlPath.replace_extension(".mtl");
			const std::filesystem::path directory = std::filesystem::weakly_canonical(mtlPath.parent_path(), error);
			if (error)
			{
				return {};
			}
			for (const std::filesystem::path& root : roots)
			{
				const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(root, error);
				if (error)
				{
					error.clear();
					continue;
				}
				const std::filesystem::path relative = directory.lexically_relative(canonicalRoot);
				const std::string generic = relative.generic_string();
				if (relative.empty() || generic == "." || generic.rfind("..", 0) == 0)
				{
					continue;
				}
				return generic;
			}
			return {};
		}

		bool LoadSiblingNmfMtl(const std::filesystem::path& nmfPath,
			openrepublic::content::MtlDocument& document, std::string& status)
		{
			std::filesystem::path mtlPath = nmfPath;
			mtlPath.replace_extension(".mtl");
			std::error_code fileSizeError;
			const std::uintmax_t fileBytes = std::filesystem::file_size(mtlPath, fileSizeError);
			if (fileSizeError)
			{
				status = "sibling MTL is unavailable; diagnostic colors are used";
				return false;
			}
			if (fileBytes > kMaxNmfPreviewMtlBytes)
			{
				status = "sibling MTL exceeds the 4 MiB syntax profile; diagnostic colors are used";
				return false;
			}

			std::ifstream input(mtlPath, std::ios::binary);
			if (!input)
			{
				status = "sibling MTL could not be read; diagnostic colors are used";
				return false;
			}

			std::string bytes(static_cast<std::size_t>(fileBytes), '\0');
			if (!bytes.empty())
			{
				input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
				if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()))
				{
					status = "sibling MTL changed or failed during the bounded read; diagnostic colors are used";
					return false;
				}
			}
			if (input.peek() != std::char_traits<char>::eof() || input.bad())
			{
				status = "sibling MTL changed during the bounded read; diagnostic colors are used";
				return false;
			}

			std::string parseError;
			if (!openrepublic::content::ParseMtlSyntax(bytes, document, &parseError))
			{
				status = "sibling MTL is outside the bounded syntax profile; diagnostic colors are used";
				return false;
			}
			status = "sibling MTL parsed under the bounded syntax profile";
			return true;
		}

		bool ValidateEmbeddedGlbProfile(const std::vector<std::uint8_t>& bytes,
			const openrepublic::content::GlbContainerInfo& info,
			openrepublic::content::GlbSemanticSummary& summary, std::string& error)
		{
#ifdef OPENREPUBLIC_HAS_GLB_SEMANTIC_VALIDATOR
			openrepublic::content::GlbSemanticError semanticError;
			if (!openrepublic::content::ValidateEmbeddedGlbV2(bytes, info, summary, semanticError))
			{
				error = semanticError.message;
				return false;
			}
			error.clear();
			return true;
#else
			static_cast<void>(bytes);
			static_cast<void>(info);
			static_cast<void>(summary);
			error = "Bounded GLB semantic validation is unavailable in this build; no import was attempted.";
			return false;
#endif
		}

		class ScopedGlbSnapshot
		{
		public:
			ScopedGlbSnapshot() = default;
			ScopedGlbSnapshot(const ScopedGlbSnapshot&) = delete;
			ScopedGlbSnapshot& operator=(const ScopedGlbSnapshot&) = delete;

			~ScopedGlbSnapshot()
			{
				Cleanup();
			}

			bool Write(const std::vector<std::uint8_t>& bytes, std::string& error)
			{
				std::error_code tempError;
				const std::filesystem::path tempRoot = std::filesystem::temp_directory_path(tempError);
				if (tempError || tempRoot.empty())
				{
					error = "Could not resolve a temporary directory for the validated GLB snapshot.";
					return false;
				}

				static std::atomic<std::uint64_t> sequence{ 0 };
				for (std::size_t attempt = 0; attempt < 64; ++attempt)
				{
					const std::wstring directoryName = L"OpenRepublic-GLB-" +
						std::to_wstring(GetCurrentProcessId()) + L"-" +
						std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed));
					const std::filesystem::path candidate = tempRoot / directoryName;
					if (CreateDirectoryW(candidate.c_str(), nullptr))
					{
						directory_ = candidate;
						break;
					}

					const DWORD createError = GetLastError();
					if (createError != ERROR_ALREADY_EXISTS && createError != ERROR_FILE_EXISTS)
					{
						error = "Could not create the temporary directory for the validated GLB snapshot.";
						return false;
					}
				}

				if (directory_.empty())
				{
					error = "Could not reserve a unique temporary directory for the validated GLB snapshot.";
					return false;
				}

				file_ = directory_ / L"validated-input.glb";
				HANDLE file = CreateFileW(file_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
					FILE_ATTRIBUTE_TEMPORARY, nullptr);
				if (file == INVALID_HANDLE_VALUE)
				{
					Cleanup();
					error = "Could not create the temporary file for the validated GLB snapshot.";
					return false;
				}

				bool writeSucceeded = true;
				std::size_t offset = 0;
				while (offset < bytes.size())
				{
					const std::size_t remaining = bytes.size() - offset;
					const DWORD chunkBytes = static_cast<DWORD>(std::min<std::size_t>(
						remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
					DWORD written = 0;
					if (!WriteFile(file, bytes.data() + offset, chunkBytes, &written, nullptr) || written != chunkBytes)
					{
						writeSucceeded = false;
						break;
					}
					offset += written;
				}

				if (writeSucceeded && !FlushFileBuffers(file))
				{
					writeSucceeded = false;
				}
				CloseHandle(file);
				if (!writeSucceeded)
				{
					Cleanup();
					error = "Could not write the complete validated GLB snapshot to the temporary file.";
					return false;
				}

				error.clear();
				return true;
			}

			const std::filesystem::path& Path() const { return file_; }

		private:
			void Cleanup() noexcept
			{
				if (!file_.empty())
				{
					DeleteFileW(file_.c_str());
					file_.clear();
				}
				if (!directory_.empty())
				{
					RemoveDirectoryW(directory_.c_str());
					directory_.clear();
				}
			}

			std::filesystem::path directory_;
			std::filesystem::path file_;
		};

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
			const std::optional<openrepublic::content::TerrainTransform>& explicitTransform,
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

			openrepublic::content::TerrainMetadata metadata;
			bool hasMetadata = false;
			if (!explicitTransform)
			{
				const std::filesystem::path metadataPath = heightmapPath.parent_path() / L"script.ini";
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
			}

			const bool transposedRaster = explicitTransform &&
				openrepublic::content::RasterOrientationSwapsAxes(explicitTransform->orientation);
			const std::uint32_t previewWidth = std::min(
				transposedRaster ? heightmap.height : heightmap.width, kTerrainPreviewMaxSide);
			const std::uint32_t previewHeight = std::min(
				transposedRaster ? heightmap.width : heightmap.height, kTerrainPreviewMaxSide);
			openrepublic::content::TerrainPreviewGrid transformedGrid;
			if (explicitTransform && !openrepublic::content::BuildTerrainPreviewGrid(
				heightmap, *explicitTransform, previewWidth, previewHeight, transformedGrid, &error))
			{
				return false;
			}

			double minimumHeight = explicitTransform
				? static_cast<double>(transformedGrid.minHeight)
				: std::numeric_limits<double>::infinity();
			double maximumHeight = explicitTransform
				? static_cast<double>(transformedGrid.maxHeight)
				: -std::numeric_limits<double>::infinity();
			if (!explicitTransform)
			{
				for (const float rawHeight : heightmap.samples)
				{
					const double height = hasMetadata
						? static_cast<double>(rawHeight) * metadata.heightScale + metadata.worldPositionY
						: static_cast<double>(rawHeight);
					minimumHeight = std::min(minimumHeight, height);
					maximumHeight = std::max(maximumHeight, height);
				}
			}

			const double defaultWidth = std::max(1.0, static_cast<double>(heightmap.width) * 0.01);
			const double defaultDepth = std::max(1.0, static_cast<double>(heightmap.height) * 0.01);
			double displayWidth = 0.0;
			double displayDepth = 0.0;
			double previewHorizontalScale = 0.0;
			double previewCenterX = 0.0;
			double previewCenterZ = 0.0;
			if (explicitTransform)
			{
				const double maxWorldExtent = std::max(explicitTransform->sizeX, explicitTransform->sizeZ);
				previewHorizontalScale = std::min(kTerrainDisplayUnitsPerSourceUnit, 100.0 / maxWorldExtent);
				displayWidth = explicitTransform->sizeX * previewHorizontalScale;
				displayDepth = explicitTransform->sizeZ * previewHorizontalScale;
				previewCenterX = explicitTransform->originX + explicitTransform->sizeX * 0.5;
				previewCenterZ = explicitTransform->originZ + explicitTransform->sizeZ * 0.5;
				if (!std::isfinite(previewHorizontalScale) || previewHorizontalScale <= 0.0 ||
					!std::isfinite(displayWidth) || !std::isfinite(displayDepth) ||
					displayWidth <= 0.0 || displayDepth <= 0.0 || displayWidth > 100.0 || displayDepth > 100.0 ||
					!std::isfinite(previewCenterX) || !std::isfinite(previewCenterZ))
				{
					error = "explicit terrain extent cannot be represented in the bounded preview frame";
					return false;
				}
			}
			else
			{
				const double candidateWidth = hasMetadata
					? metadata.worldSizeX * kTerrainDisplayUnitsPerSourceUnit : defaultWidth;
				const double candidateDepth = hasMetadata
					? metadata.worldSizeY * kTerrainDisplayUnitsPerSourceUnit : defaultDepth;
				displayWidth = std::clamp(candidateWidth, 1.0, 100.0);
				displayDepth = std::clamp(candidateDepth, 1.0, 100.0);
			}
			const double verticalScale = explicitTransform
				? previewHorizontalScale * kTerrainVerticalPreviewExaggeration
				: (hasMetadata ? kTerrainVerticalPreviewScale : kRawPreviewVerticalScale);
			const double verticalSpan = (maximumHeight - minimumHeight) * verticalScale;
			if (!std::isfinite(verticalScale) || verticalScale <= 0.0 ||
				!std::isfinite(verticalSpan) || verticalSpan > kMaxTerrainPreviewHeight)
			{
				error = "height range is outside the safe preview bounds";
				return false;
			}

			std::vector<float> displayXCoordinates;
			std::vector<float> displayZCoordinates;
			if (explicitTransform)
			{
				displayXCoordinates.resize(previewWidth);
				displayZCoordinates.resize(previewHeight);
				for (std::uint32_t x = 0; x < previewWidth; ++x)
				{
					const double localX = (transformedGrid.worldXCoordinates[x] - previewCenterX) * previewHorizontalScale;
					const float displayX = static_cast<float>(localX);
					if (!std::isfinite(localX) || !std::isfinite(displayX) ||
						(x != 0 && displayX <= displayXCoordinates[x - 1]))
					{
						error = "explicit terrain X coordinates collapse in the bounded preview frame";
						return false;
					}
					displayXCoordinates[x] = displayX;
				}
				for (std::uint32_t z = 0; z < previewHeight; ++z)
				{
					const double localZ = (transformedGrid.worldZCoordinates[z] - previewCenterZ) * previewHorizontalScale;
					const float displayZ = static_cast<float>(localZ);
					if (!std::isfinite(localZ) || !std::isfinite(displayZ) ||
						(z != 0 && displayZ <= displayZCoordinates[z - 1]))
					{
						error = "explicit terrain Z coordinates collapse in the bounded preview frame";
						return false;
					}
					displayZCoordinates[z] = displayZ;
				}
			}

			std::vector<XMFLOAT3> positions;
			positions.reserve(static_cast<std::size_t>(previewWidth) * previewHeight);
			for (std::uint32_t z = 0; z < previewHeight; ++z)
			{
				for (std::uint32_t x = 0; x < previewWidth; ++x)
				{
					double sourceHeight = 0.0;
					if (explicitTransform)
					{
						sourceHeight = transformedGrid.heights[static_cast<std::size_t>(z) * previewWidth + x];
					}
					else
					{
						const std::uint64_t sourceZNumerator = static_cast<std::uint64_t>(z) * (heightmap.height - 1u);
						const std::uint32_t sourceZ = static_cast<std::uint32_t>((sourceZNumerator + (previewHeight - 1u) / 2u) / (previewHeight - 1u));
						const std::uint64_t sourceXNumerator = static_cast<std::uint64_t>(x) * (heightmap.width - 1u);
						const std::uint32_t sourceX = static_cast<std::uint32_t>((sourceXNumerator + (previewWidth - 1u) / 2u) / (previewWidth - 1u));
						const std::size_t sourceIndex = static_cast<std::size_t>(sourceZ) * heightmap.width + sourceX;
						sourceHeight = hasMetadata
							? static_cast<double>(heightmap.samples[sourceIndex]) * metadata.heightScale + metadata.worldPositionY
							: static_cast<double>(heightmap.samples[sourceIndex]);
					}
					const double displayY = (sourceHeight - minimumHeight) * verticalScale;
					if (!std::isfinite(displayY) || displayY > kMaxTerrainPreviewHeight)
					{
						error = "a terrain vertex is outside the safe preview bounds";
						return false;
					}
					const float u = static_cast<float>(x) / static_cast<float>(previewWidth - 1u);
					const float v = static_cast<float>(z) / static_cast<float>(previewHeight - 1u);
					positions.emplace_back(
						explicitTransform ? displayXCoordinates[x] : static_cast<float>((static_cast<double>(u) - 0.5) * displayWidth),
						static_cast<float>(displayY),
						explicitTransform ? displayZCoordinates[z] : static_cast<float>((static_cast<double>(v) - 0.5) * displayDepth));
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
				<< previewWidth << 'x' << previewHeight << " mesh";
			if (explicitTransform)
			{
				summary << "; explicit caller world coordinates mapped into a recentered local preview frame using "
					<< previewHorizontalScale << " display units/source unit; orientation "
					<< TerrainOrientationLabel(explicitTransform->orientation)
					<< " (5x vertical exaggeration; no WRSR transform is verified)";
			}
			else if (hasMetadata)
			{
				summary << "; candidate height raw*" << metadata.heightScale << " + offset " << metadata.worldPositionY
					<< " (vertical preview scale 0.005; coordinates/orientation unverified; preview remains centered)";
			}
			else
			{
				summary << "; raw sample-height preview (no adjacent script.ini metadata)";
			}
			description = summary.str();
			return true;
		}

		bool CreateNmfStaticPreview(wi::scene::Scene& scene, const std::filesystem::path& nmfPath,
			wi::ecs::Entity& meshEntity, XMFLOAT3& previewCameraTarget, float& previewCameraDistance,
			bool unlitDiagnostic, bool materialUnlitDiagnostic,
			const std::vector<std::filesystem::path>& textureRoots,
			std::string& description, std::string& geometryAuditSummary,
			std::string& error)
		{
			openrepublic::content::NmfStaticGeometry geometry;
			if (!openrepublic::content::LoadNmfStaticGeometry(nmfPath, geometry, &error))
			{
				return false;
			}

			openrepublic::content::MtlDocument mtlDocument;
			std::vector<openrepublic::content::MtlSubmaterialBinding> mtlBindings;
			std::vector<openrepublic::content::MtlPreviewBaseColorTexture> baseColorTextures;
			std::string mtlStatus;
			bool haveMtlBindings = false;
			bool haveTextureBindings = false;
			std::string mtlTextureStatus;
			std::size_t mtlRelativeTextureCount = 0;
			if (unlitDiagnostic)
			{
				mtlStatus = "neutral unlit geometry diagnostic bypasses sibling MTL parsing";
			}
			else if (LoadSiblingNmfMtl(nmfPath, mtlDocument, mtlStatus))
			{
				std::string bindingError;
				// Mirrors the build-scoped loader selection rule (single section, exact
				// name, else first section); see SelectMtlSubmaterialsLikeLoader.
				haveMtlBindings = openrepublic::content::SelectMtlSubmaterialsLikeLoader(
					geometry.submaterialIdentifiers, mtlDocument, mtlBindings, &bindingError);
				if (!haveMtlBindings)
				{
					mtlDocument = {};
					mtlStatus = "NMF/MTL identifiers could not be selected within the bounded profile; diagnostic colors are used";
				}
				else if (const std::string mtlDirectory = MtlDirectoryRelativeToRoots(nmfPath, textureRoots);
					!mtlDirectory.empty())
				{
					// `$TEXTURE_MTL` paths are relative to the MTL's own directory.
					openrepublic::content::ApplyMtlDirectoryToRelativeTextures(
						mtlDocument, mtlDirectory, &mtlRelativeTextureCount, &bindingError);
				}
			}
			if (!unlitDiagnostic && !textureRoots.empty() && haveMtlBindings)
			{
				openrepublic::content::MtlDependencyManifest dependencyManifest;
				std::string dependencyError;
				if (openrepublic::content::BuildMtlDependencyManifest(
					mtlDocument, textureRoots, dependencyManifest, &dependencyError) ==
					openrepublic::content::MtlDependencyManifestError::None)
				{
					std::string bindingError;
					haveTextureBindings = openrepublic::content::BuildMtlPreviewBaseColorTextures(
						mtlDocument, dependencyManifest, baseColorTextures, &bindingError);
					mtlTextureStatus = haveTextureBindings
						? "caller-supplied texture roots resolved under the bounded dependency profile"
						: "slot-0 texture preview binding was rejected by the bounded adapter";
				}
				else
				{
					mtlTextureStatus = "texture dependency manifest exceeded its bounded input profile";
				}
			}
			else if (!textureRoots.empty())
			{
				mtlTextureStatus = "texture preview was unavailable because sibling MTL name binding failed";
			}

			openrepublic::content::NmfTextureCoordinateStreams textureCoordinates;
			if (!unlitDiagnostic && !openrepublic::content::BuildNmfTextureCoordinateStreams(
				geometry, textureCoordinates, &error))
				return false;

			openrepublic::content::NmfPreviewGeometry previewGeometry;
			if (!openrepublic::content::BuildNmfPreviewGeometry(geometry, previewGeometry, &error))
				return false;
			previewCameraTarget = XMFLOAT3(previewGeometry.cameraTarget.x,
				previewGeometry.cameraTarget.y, previewGeometry.cameraTarget.z);
			previewCameraDistance = previewGeometry.cameraDistance;
			const std::size_t vertexCount = previewGeometry.positions.size();
			const std::size_t indexCount = previewGeometry.indices.size();
			const std::size_t sourceNormalCount = previewGeometry.sourceNormalCount;
			const std::size_t ignoredSubsetIndexCount = previewGeometry.ignoredSubsetIndexCount;
			const auto& indices = previewGeometry.indices;
			const auto& drawRanges = previewGeometry.drawRanges;
			const auto& sourceNormalMask = previewGeometry.sourceNormalMask;
			std::vector<XMFLOAT3> positions;
			positions.reserve(vertexCount);
			for (const auto& position : previewGeometry.positions)
				positions.emplace_back(position.x, position.y, position.z);
			std::vector<XMFLOAT3> sourceNormals;
			sourceNormals.reserve(vertexCount);
			for (const auto& normal : previewGeometry.sourceNormals)
				sourceNormals.emplace_back(normal.x, normal.y, normal.z);
			if ((textureCoordinates.hasTextureCoordinates0 &&
				textureCoordinates.textureCoordinates0.size() != vertexCount) ||
				(textureCoordinates.hasTextureCoordinates1 &&
				textureCoordinates.textureCoordinates1.size() != vertexCount))
			{
				error = "NMF combined texture-coordinate streams do not match the combined vertex count";
				return false;
			}
			if (drawRanges.empty())
			{
				error = "NMF geometry has no complete subset triangle range to preview";
				return false;
			}

			meshEntity = scene.Entity_CreateMeshFromData("OpenRepublic bounded NMF geometry preview",
				indices.size(), indices.data(), positions.size(), positions.data());
			std::vector<wi::ecs::Entity> submaterialEntities(geometry.submaterialIdentifiers.size(),
				wi::ecs::INVALID_ENTITY);
			if (wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(meshEntity))
			{
				if (!HasValidIndexedTriangleData(*mesh))
				{
					error = "Wicked Engine rejected the normalized NMF triangle data";
					return false;
				}
				if (mesh->vertex_normals.size() != positions.size() ||
					sourceNormals.size() != positions.size() || sourceNormalMask.size() != positions.size())
				{
					error = "Wicked Engine generated-normal fallback does not match the NMF vertex count";
					return false;
				}
				for (std::size_t vertexIndex = 0; !unlitDiagnostic && vertexIndex < sourceNormalMask.size(); ++vertexIndex)
				{
					if (sourceNormalMask[vertexIndex] != 0)
					{
						mesh->vertex_normals[vertexIndex] = sourceNormals[vertexIndex];
					}
				}
				if (textureCoordinates.hasTextureCoordinates0)
				{
					mesh->vertex_uvset_0.clear();
					mesh->vertex_uvset_0.reserve(textureCoordinates.textureCoordinates0.size());
					for (const auto& uv : textureCoordinates.textureCoordinates0)
						mesh->vertex_uvset_0.emplace_back(uv.u, uv.v);
				}
				if (textureCoordinates.hasTextureCoordinates1)
				{
					mesh->vertex_uvset_1.clear();
					mesh->vertex_uvset_1.reserve(textureCoordinates.textureCoordinates1.size());
					for (const auto& uv : textureCoordinates.textureCoordinates1)
						mesh->vertex_uvset_1.emplace_back(uv.u, uv.v);
				}
				if ((textureCoordinates.hasTextureCoordinates0 &&
					mesh->vertex_uvset_0.size() != positions.size()) ||
					(textureCoordinates.hasTextureCoordinates1 &&
					mesh->vertex_uvset_1.size() != positions.size()))
				{
					error = "Wicked Engine NMF UV sets do not match the mesh vertex count";
					return false;
				}
				mesh->subsets.clear();
				for (const openrepublic::content::NmfPreviewDrawRange& drawRange : drawRanges)
				{
					wi::ecs::Entity materialEntity = meshEntity;
					if (drawRange.submaterialIndex !=
						openrepublic::content::NoNmfPreviewSubmaterialIndex)
					{
						if (drawRange.submaterialIndex >= submaterialEntities.size())
						{
							error = "NMF subset references a missing submaterial debug entry";
							return false;
						}
						materialEntity = submaterialEntities[drawRange.submaterialIndex];
						if (materialEntity == wi::ecs::INVALID_ENTITY)
						{
							materialEntity = scene.Entity_CreateMaterial("OpenRepublic NMF submaterial index " +
								std::to_string(drawRange.submaterialIndex) + " debug color");
							submaterialEntities[drawRange.submaterialIndex] = materialEntity;
						}
					}

					wi::scene::MeshComponent::MeshSubset& renderSubset = mesh->subsets.emplace_back();
					renderSubset.indexOffset = drawRange.indexOffset;
					renderSubset.indexCount = drawRange.indexCount;
					renderSubset.materialID = materialEntity;
				}
				if (!HasValidMeshSubsetRanges(*mesh))
				{
					error = "NMF normalized subset ranges are invalid for Wicked Engine";
					return false;
				}
				mesh->CreateRenderData();
			}
			else
			{
				error = "Wicked Engine did not create the NMF mesh component";
				return false;
			}
			std::size_t mappedSubmaterialCount = 0;
			std::size_t exactMtlMatchCount = 0;
			std::size_t diffuseColorCount = 0;
			std::size_t absentDiffuseColorCount = 0;
			std::size_t invalidDiffuseColorCount = 0;
			std::size_t missingMtlMatchCount = 0;
		std::size_t fallbackMtlMatchCount = 0;
			std::size_t ambiguousMtlMatchCount = 0;
			std::size_t baseColorTextureCandidateCount = 0;
			std::size_t baseColorTextureLoadedCount = 0;
			std::size_t baseColorTextureLoadFailureCount = 0;
			std::size_t loadedPreviewTextureBytes = 0;
			std::size_t ambiguousBaseColorTextureCount = 0;
			std::size_t unresolvedBaseColorTextureCount = 0;
			std::size_t invalidBaseColorTextureIndexCount = 0;
			for (std::size_t materialIndex = 0; materialIndex < submaterialEntities.size(); ++materialIndex)
			{
				const wi::ecs::Entity materialEntity = submaterialEntities[materialIndex];
				if (materialEntity == wi::ecs::INVALID_ENTITY)
				{
					continue;
				}
				wi::scene::MaterialComponent* material = scene.materials.GetComponent(materialEntity);
				if (material == nullptr)
				{
					error = "Wicked Engine did not create an NMF submaterial debug component";
					return false;
				}
				if (unlitDiagnostic)
				{
					material->SetBaseColor(XMFLOAT4(0.72f, 0.72f, 0.72f, 1.0f));
				}
				++mappedSubmaterialCount;
				if (!unlitDiagnostic && haveMtlBindings && materialIndex < mtlBindings.size() &&
					(mtlBindings[materialIndex].status ==
						openrepublic::content::MtlSubmaterialBindingStatus::Matched ||
					mtlBindings[materialIndex].status ==
						openrepublic::content::MtlSubmaterialBindingStatus::FallbackFirstSection) &&
					mtlBindings[materialIndex].sectionIndex < mtlDocument.submaterials.size())
				{
					if (mtlBindings[materialIndex].status ==
						openrepublic::content::MtlSubmaterialBindingStatus::Matched)
					{
						++exactMtlMatchCount;
					}
					else
					{
						++fallbackMtlMatchCount;
					}
					const auto diffuseColor = openrepublic::content::ExtractMtlDiffuseColorForPreview(
						mtlDocument.submaterials[mtlBindings[materialIndex].sectionIndex]);
					if (diffuseColor.status ==
						openrepublic::content::MtlPreviewDiffuseColorStatus::Valid)
					{
						// This is an approximate preview adapter from the observed source
						// four-float value to Wicked's PBR base color, not shader parity.
						material->SetBaseColor(XMFLOAT4(
							diffuseColor.value[0], diffuseColor.value[1],
							diffuseColor.value[2], diffuseColor.value[3]));
						++diffuseColorCount;
					}
					else if (diffuseColor.status ==
						openrepublic::content::MtlPreviewDiffuseColorStatus::Absent)
					{
						// The inspected loader initializes all four DiffuseColor values to
						// 1.0 before reading optional section directives.
						material->SetBaseColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
						++absentDiffuseColorCount;
					}
					else
					{
						// A syntax-valid double can exceed float range. Keep it diagnostic
						// instead of presenting the observed default as source data.
						material->SetBaseColor(NmfDebugMaterialColor(materialIndex));
						++invalidDiffuseColorCount;
					}
				}
				else if (!unlitDiagnostic)
				{
					material->SetBaseColor(NmfDebugMaterialColor(materialIndex));
					if (haveMtlBindings && materialIndex < mtlBindings.size() &&
						mtlBindings[materialIndex].status ==
							openrepublic::content::MtlSubmaterialBindingStatus::Ambiguous)
					{
						++ambiguousMtlMatchCount;
					}
					else if (haveMtlBindings && materialIndex < mtlBindings.size() &&
						mtlBindings[materialIndex].status ==
							openrepublic::content::MtlSubmaterialBindingStatus::Missing)
					{
						++missingMtlMatchCount;
					}
				}
				bool expectsBaseColorTexture = false;
				if (!unlitDiagnostic && haveTextureBindings && haveMtlBindings &&
					materialIndex < mtlBindings.size() &&
					(mtlBindings[materialIndex].status ==
						openrepublic::content::MtlSubmaterialBindingStatus::Matched ||
					mtlBindings[materialIndex].status ==
						openrepublic::content::MtlSubmaterialBindingStatus::FallbackFirstSection) &&
					mtlBindings[materialIndex].sectionIndex < baseColorTextures.size())
				{
					const auto& texture = baseColorTextures[mtlBindings[materialIndex].sectionIndex];
					switch (texture.status)
					{
					case openrepublic::content::MtlPreviewBaseColorTextureStatus::Resolved:
						if (!texture.resolvedPath.empty() && texture.resolvedPath.is_absolute())
						{
							++baseColorTextureCandidateCount;
							wi::Resource loadedTexture;
							std::string resourceName;
							std::size_t imageBytes = 0;
							std::string loadError;
							const std::size_t remainingBytes =
								loadedPreviewTextureBytes >= kMaxNmfPreviewTextureBytesTotal
									? 0
									: static_cast<std::size_t>(kMaxNmfPreviewTextureBytesTotal) -
										loadedPreviewTextureBytes;
							if (LoadNmfPreviewTexture(texture.resolvedPath, textureRoots, remainingBytes,
								loadedTexture, resourceName, imageBytes, loadError))
							{
								auto& baseColorMap = material->textures[
									wi::scene::MaterialComponent::BASECOLORMAP];
								baseColorMap.name = std::move(resourceName);
								baseColorMap.resource = std::move(loadedTexture);
								// The indexed source texture-coordinate input is observed;
								// assigning it as Wicked UV set 0 is an adapter inference.
								baseColorMap.uvset = 0;
								material->SetTextureStreamingDisabled(true);
								expectsBaseColorTexture = true;
								loadedPreviewTextureBytes += imageBytes;
							}
							else
							{
								++baseColorTextureLoadFailureCount;
							}
						}
						else
						{
							++unresolvedBaseColorTextureCount;
						}
						break;
					case openrepublic::content::MtlPreviewBaseColorTextureStatus::Ambiguous:
						++ambiguousBaseColorTextureCount;
						break;
					case openrepublic::content::MtlPreviewBaseColorTextureStatus::Unresolved:
						++unresolvedBaseColorTextureCount;
						break;
					case openrepublic::content::MtlPreviewBaseColorTextureStatus::InvalidSlotIndex:
						++invalidBaseColorTextureIndexCount;
						break;
					case openrepublic::content::MtlPreviewBaseColorTextureStatus::NoSlotZero:
						break;
					}
				}
				// Loaded texture bytes have a generated in-memory resource name and are
				// already retained by ResourceManager. Avoid invalidating/reopening them.
				material->CreateRenderData(!expectsBaseColorTexture);
				if (expectsBaseColorTexture)
				{
					if (material->textures[wi::scene::MaterialComponent::BASECOLORMAP].GetGPUResource() != nullptr)
					{
						++baseColorTextureLoadedCount;
					}
					else
					{
						++baseColorTextureLoadFailureCount;
					}
				}
			}
			if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(meshEntity))
			{
				// Keep unpartitioned ranges on a generic material. A table entry is
				// assigned only when an observed subset explicitly references it.
				if (unlitDiagnostic)
				{
					material->SetBaseColor(XMFLOAT4(0.72f, 0.72f, 0.72f, 1.0f));
				}
				else
				{
					material->SetBaseColor(XMFLOAT4(0.66f, 0.55f, 0.42f, 1.0f));
				}
				material->CreateRenderData(true);
			}
			else
			{
				error = "Wicked Engine did not create the NMF debug material";
				return false;
			}

			const std::size_t drawRangeTriangleCount = previewGeometry.drawRangeTriangleCount;
			std::string mtlBindingSummary;
			if (unlitDiagnostic)
			{
				mtlBindingSummary = "MTL name/color bindings were not evaluated in neutral unlit diagnostic mode";
			}
			else if (haveMtlBindings)
			{
				mtlBindingSummary = std::to_string(exactMtlMatchCount) +
					" exact-name or single-section MTL selections and " +
					std::to_string(fallbackMtlMatchCount) +
					" first-section fallbacks (build-scoped loader rule); " +
					std::to_string(mtlRelativeTextureCount) + " $TEXTURE_MTL paths rebased onto the MTL directory; " +
					std::to_string(diffuseColorCount) +
					" parseable $DIFFUSECOLOR values shown approximately as Wicked base colors; " +
					std::to_string(absentDiffuseColorCount) +
					" absent diffuse directives use the observed all-ones source default; " +
					std::to_string(invalidDiffuseColorCount) +
					" float-unrepresentable diffuse values, " + std::to_string(missingMtlMatchCount) +
					" missing names, and " + std::to_string(ambiguousMtlMatchCount) +
					" ambiguous names remain diagnostic";
			}
			else
			{
				mtlBindingSummary =
					"sibling MTL binding was unavailable, so name matches were not checked and referenced entries remain diagnostic";
			}
			std::string textureBindingSummary;
			if (unlitDiagnostic)
			{
				textureBindingSummary = "texture loading was bypassed in neutral unlit diagnostic mode";
			}
			else if (textureRoots.empty())
			{
				textureBindingSummary = "texture loading is disabled unless caller-supplied --nmf-texture-root values are provided";
			}
			else if (haveTextureBindings)
			{
				textureBindingSummary = std::to_string(baseColorTextureLoadedCount) + "/" +
					std::to_string(baseColorTextureCandidateCount) +
					" unique slot-0 candidates loaded as Wicked base-color maps on inferred UV set 0; " +
					std::to_string(baseColorTextureLoadFailureCount) + " image loads failed, " +
					std::to_string(ambiguousBaseColorTextureCount) + " slot-0 references were ambiguous, " +
					std::to_string(unresolvedBaseColorTextureCount) + " were unresolved, and " +
					std::to_string(invalidBaseColorTextureIndexCount) + " had non-integer slot tokens; " +
					"other texture slots remain unmapped";
			}
			else
			{
				textureBindingSummary = mtlTextureStatus.empty()
					? "texture dependency binding was unavailable"
					: mtlTextureStatus;
			}
			const std::string normalLengthSummary = previewGeometry.sourceNormalCount == 0
				? "no source-normal lengths are available"
				: "source-normal lengths min/mean/max=" +
					std::to_string(previewGeometry.sourceNormalLengthMinimum) + "/" +
					std::to_string(previewGeometry.sourceNormalLengthMean) + "/" +
					std::to_string(previewGeometry.sourceNormalLengthMaximum);
			const std::string uvSummary = unlitDiagnostic
				? "UV stream aggregation was bypassed in neutral unlit diagnostic mode. "
				: "Aligned preview UV vertex counts: set 0=" +
					std::to_string(textureCoordinates.textureCoordinates0.size()) +
					", set 1=" + std::to_string(textureCoordinates.textureCoordinates1.size()) + ". ";
			const std::string materialAdapterSummary = unlitDiagnostic
				? "MTL material parsing and diffuse-to-PBR adaptation were bypassed; no material compatibility claim is made. "
				: "The Wicked base-color mapping is approximate and does not establish source color-space, alpha, lighting, or shader parity. ";
			const std::string sourceNormalLengthAudit = previewGeometry.sourceNormalCount == 0
				? "none"
				: std::to_string(previewGeometry.sourceNormalLengthMinimum) + "/" +
					std::to_string(previewGeometry.sourceNormalLengthMean) + "/" +
					std::to_string(previewGeometry.sourceNormalLengthMaximum);
			// Keep the bounded geometry counters separate from the verbose UI notice:
			// Wicked's wilog formatter uses a fixed 1024-byte buffer.
			geometryAuditSummary =
				std::string(unlitDiagnostic ? "mode=neutral-unlit" :
					(materialUnlitDiagnostic ? "mode=material-unlit" : "mode=regular")) +
				" objects=" + std::to_string(geometry.objects.size()) +
				" vertices=" + std::to_string(vertexCount) +
				" source_indices=" + std::to_string(previewGeometry.indices.size()) +
				" draw_ranges=" + std::to_string(previewGeometry.drawRanges.size()) +
				" expected_draw_indices=" + std::to_string(drawRangeTriangleCount * 3) +
				" draw_range_triangles=" + std::to_string(drawRangeTriangleCount) +
				" subset_tail_indices=" +
					std::to_string(previewGeometry.ignoredSubsetIndexCount) +
				" extents_xyz=" +
					std::to_string(static_cast<double>(previewGeometry.sourceMaximum.x) - previewGeometry.sourceMinimum.x) + "/" +
					std::to_string(static_cast<double>(previewGeometry.sourceMaximum.y) - previewGeometry.sourceMinimum.y) + "/" +
					std::to_string(static_cast<double>(previewGeometry.sourceMaximum.z) - previewGeometry.sourceMinimum.z) +
				" exact_zero_area=" + std::to_string(previewGeometry.zeroAreaTriangleCount) +
				" source_normals=" + std::to_string(previewGeometry.sourceNormalCount) +
				" normal_lengths_min_mean_max=" + sourceNormalLengthAudit +
				" slot0_candidates=" + std::to_string(baseColorTextureCandidateCount) +
				" slot0_loaded=" + std::to_string(baseColorTextureLoadedCount) +
				" slot0_load_failures=" + std::to_string(baseColorTextureLoadFailureCount) +
				" slot0_ambiguous=" + std::to_string(ambiguousBaseColorTextureCount) +
				" slot0_unresolved=" + std::to_string(unresolvedBaseColorTextureCount) +
				" slot0_invalid_index=" + std::to_string(invalidBaseColorTextureIndexCount);
			description = "Bounded static NMF geometry preview: " +
				std::to_string(geometry.objects.size()) + " objects (one LOD each), " +
				std::to_string(vertexCount) + " vertices, " +
				std::to_string(drawRangeTriangleCount) + " complete triangles across " +
				std::to_string(drawRanges.size()) + " retained draw ranges. Geometry is uniformly normalized to a 10-unit display span at x=8. " +
				"Source position extents X/Y/Z=" +
				std::to_string(static_cast<double>(previewGeometry.sourceMaximum.x) - previewGeometry.sourceMinimum.x) + "/" +
				std::to_string(static_cast<double>(previewGeometry.sourceMaximum.y) - previewGeometry.sourceMinimum.y) + "/" +
				std::to_string(static_cast<double>(previewGeometry.sourceMaximum.z) - previewGeometry.sourceMinimum.z) +
				"; exact-zero-area draw-range triangles=" +
				std::to_string(previewGeometry.zeroAreaTriangleCount) + "; " + normalLengthSummary +
				". These are structural diagnostics, not source-convention or fidelity checks. " +
				std::to_string(mappedSubmaterialCount) + " referenced submaterial-table entries, " +
				mtlBindingSummary + "; " + textureBindingSummary + ". " +
				mtlStatus + ". " +
				(unlitDiagnostic ? std::string() :
					"Build 23935965 labels flag 0x8 _NRM FLOAT3 and flags 0x100/0x200 _TC0/_TC1 eight-byte streams; interpreting each pair as FLOAT2 and assigning Wicked UV sets 0/1 is an inferred preview adapter. The `$TEXTURE` integer is observed as the source texture register index; using slot 0 as Wicked base color on UV set 0 is an inferred preview mapping, and other roles remain unknown. ") +
				uvSummary +
				(unlitDiagnostic
					? std::to_string(sourceNormalCount) + " source normals are present but bypassed in neutral unlit diagnostic mode; MTL colors and UV streams are also bypassed. Common fog and post-processing may still affect the output. "
					: std::to_string(sourceNormalCount) + " source normals are preserved as-is and other vertices use Wicked smooth-normal fallback. ") +
				"This does not apply the original loader's optional coordinate-reflection mode or establish shading parity. " +
				materialAdapterSummary +
				"Auxiliary subset indices are retained but not rendered. " +
				(ignoredSubsetIndexCount == 0 ? std::string() :
					std::to_string(ignoredSubsetIndexCount) + " trailing subset indices are ignored by the observed indexCount/3 draw rule. ") +
				"Source placement, game units, texture fidelity, and target-matched visual acceptance remain unverified.";
			error.clear();
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
		const bool capturePreview = !nmfCapturePaths.empty();
		bool terrainPreviewLoaded = false;
		bool nmfPreviewLoaded = false;
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
		if (!capturePreview)
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
				if (CreateNmfStaticPreview(previewWorld, nmfProbePath, meshEntity,
					nmfPreviewCameraTarget, nmfPreviewCameraDistance, nmfUnlitPreview,
					nmfMaterialUnlitPreview, nmfTextureRoots,
					description, geometryAuditSummary, error))
				{
					nmfPreviewLoaded = true;
					uiNotice = description;
					// Fill light so faces turned away from the preview sun remain legible; the
					// synthetic scene keeps Wicked's default weather.
					const wi::ecs::Entity weatherEntity = wi::ecs::CreateEntity();
					previewWorld.names.Create(weatherEntity) = "OpenRepublic NMF preview weather";
					wi::scene::WeatherComponent& weather = previewWorld.weathers.Create(weatherEntity);
					weather.ambient = XMFLOAT3(0.55f, 0.55f, 0.6f);
					weather.horizon = XMFLOAT3(0.55f, 0.6f, 0.7f);
					weather.zenith = XMFLOAT3(0.45f, 0.55f, 0.75f);
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

		if (capturePreview && nmfPreviewLoaded)
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
				wilog("[OpenRepublic] NMF preview capture skipped because no supported NMF model was loaded");
				PostQuitMessage(2);
			}
		}
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
			cameraTransform.Translate(terrainPreviewLoaded ? XMFLOAT3(0, 18.0f, -27.0f) :
				(nmfPreviewLoaded ? XMFLOAT3(8.0f, 8.0f, -17.0f) : XMFLOAT3(0, 6.5f, -11.5f)));
			cameraTransform.RotateRollPitchYaw(XMFLOAT3(terrainPreviewLoaded ? 0.52f : (nmfPreviewLoaded ? 0.38f : 0.43f), 0, 0));
			cameraTransform.UpdateTransform();
			renderPath->camera->TransformCamera(cameraTransform);
			renderPath->camera->UpdateCamera();
		}

		ActivatePath(renderPath.get());
		if (!capturePreview)
		{
			UpdateSimulationPresentation();
		}
	}

	void OpenRepublicApplication::Update(float dt)
	{
		wi::Application::Update(dt);
		if (nmfCaptureMode)
		{
			return;
		}

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
