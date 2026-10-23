#include "ContentPreview.h"
#include "RoadMesh.h"
#include "RoadPaths.h"
#include "ShowcaseProps.h"
#include "openrepublic/content/mtl_dependencies.h"
#include "openrepublic/content/mtl_preview_color.h"
#include "openrepublic/content/mtl_preview_texture.h"
#include "openrepublic/content/mtl_submaterial_binding.h"
#include "openrepublic/content/mtl_syntax.h"
#include "openrepublic/content/nmf_geometry.h"
#include "openrepublic/content/nmf_preview_geometry.h"
#include "openrepublic/content/terrain_preview.h"
#include "wiColor.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cwctype>
#include <exception>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <utility>

namespace openrepublic::preview
{
	namespace
	{
		class ShowcasePreviewTimer
		{
		public:
			explicit ShowcasePreviewTimer(const char* stage) : stage_(stage), start_(std::chrono::steady_clock::now()) {}
			~ShowcasePreviewTimer()
			{
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - start_).count();
				wilog("[OpenRepublic] Showcase timing: %s %lld ms", stage_, static_cast<long long>(elapsed));
			}
		private:
			const char* stage_;
			std::chrono::steady_clock::time_point start_;
		};
	}
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
		constexpr std::uintmax_t kMaxTerrainMetadataBytes = 64u * 1024u;

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
	std::string MtlDirectoryRelativeToRoots(const std::filesystem::path& mtlPath,
		const std::vector<std::filesystem::path>& roots)
	{
		std::error_code error;
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

	// Reads the explicit material path, or the NMF's sibling `.mtl` when empty.
	bool LoadNmfMtl(const std::filesystem::path& nmfPath, const std::filesystem::path& explicitMtlPath,
		openrepublic::content::MtlDocument& document, std::string& status)
	{
		std::filesystem::path mtlPath = explicitMtlPath;
		if (mtlPath.empty())
		{
			mtlPath = nmfPath;
			mtlPath.replace_extension(".mtl");
		}
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
		const std::filesystem::path& explicitMtlPath,
		std::string& description, std::string& geometryAuditSummary,
		std::string& error, NmfPreviewFrame* frame, bool sourceUnits, bool mirrorZ)
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
		else if (LoadNmfMtl(nmfPath, explicitMtlPath, mtlDocument, mtlStatus))
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
			else if (const std::string mtlDirectory = MtlDirectoryRelativeToRoots(
				explicitMtlPath.empty() ? std::filesystem::path(nmfPath).replace_extension(".mtl") : explicitMtlPath,
				textureRoots);
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
		if (!openrepublic::content::BuildNmfPreviewGeometry(geometry, previewGeometry, &error, sourceUnits, mirrorZ))
			return false;
		previewCameraTarget = XMFLOAT3(previewGeometry.cameraTarget.x,
			previewGeometry.cameraTarget.y, previewGeometry.cameraTarget.z);
		previewCameraDistance = previewGeometry.cameraDistance;
		if (frame != nullptr)
		{
			frame->displayScale = previewGeometry.displayScale;
			frame->sourceMinimumY = previewGeometry.sourceMinimum.y;
			frame->sourceMaximumY = previewGeometry.sourceMaximum.y;
			frame->sourceMinimumX = previewGeometry.sourceMinimum.x;
			frame->sourceMaximumX = previewGeometry.sourceMaximum.x;
			frame->sourceMinimumZ = previewGeometry.sourceMinimum.z;
			frame->sourceMaximumZ = previewGeometry.sourceMaximum.z;
			frame->sourceCenterX = (static_cast<double>(previewGeometry.sourceMinimum.x) +
				previewGeometry.sourceMaximum.x) * 0.5;
			frame->sourceCenterZ = (static_cast<double>(previewGeometry.sourceMinimum.z) +
				previewGeometry.sourceMaximum.z) * 0.5;
			frame->sourceMaxHorizontalExtent = std::max(
				static_cast<double>(previewGeometry.sourceMaximum.x) - previewGeometry.sourceMinimum.x,
				static_cast<double>(previewGeometry.sourceMaximum.z) - previewGeometry.sourceMinimum.z);
		}
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


	bool CreateMapTerrainAroundPlacement(wi::scene::Scene& scene,
		const openrepublic::content::MapPlacementRequest& request, const NmfPreviewFrame& frame,
		double radiusMeters, std::string& description, std::string& error)
	{
		using namespace openrepublic::content;
		if (!(frame.displayScale > 0.0))
		{
			error = "NMF preview frame is not available for terrain placement";
			return false;
		}
		const TerrainTransform transform = MakeMapTerrainTransform(request.metadata);
		const PlacedBuilding& placement = request.placement;
		const double radius = radiusMeters > 0.0
			? radiusMeters
			: std::max(60.0, frame.sourceMaxHorizontalExtent * 1.1);
		// Keep the sampled square inside the terrain; a placement at the map edge gets a
		// smaller region on that side rather than a failure.
		const double minX = std::max(transform.originX, placement.X() - radius);
		const double maxX = std::min(transform.originX + transform.sizeX, placement.X() + radius);
		const double minZ = std::max(transform.originZ, placement.Z() - radius);
		const double maxZ = std::min(transform.originZ + transform.sizeZ, placement.Z() + radius);
		constexpr std::uint32_t kRegionSamples = 193;
		TerrainPreviewGrid grid;
		if (!BuildTerrainRegionGrid(request.heightmap, transform, minX, minZ, maxX - minX, maxZ - minZ,
			kRegionSamples, kRegionSamples, grid, &error))
		{
			return false;
		}

		const double s = frame.displayScale;
		const auto& m = placement.matrix;
		std::vector<XMFLOAT3> positions;
		positions.reserve(static_cast<std::size_t>(kRegionSamples) * kRegionSamples);
		for (std::uint32_t z = 0; z < kRegionSamples; ++z)
		{
			for (std::uint32_t x = 0; x < kRegionSamples; ++x)
			{
				const double dx = grid.worldXCoordinates[x] - placement.X();
				const double dz = grid.worldZCoordinates[z] - placement.Z();
				// world = local * M (row vectors), so local = offset * transpose(rotation block).
				const double localX = dx * m[0] + dz * m[2];
				const double localZ = dx * m[8] + dz * m[10];
				const double localY = static_cast<double>(
					grid.heights[static_cast<std::size_t>(z) * kRegionSamples + x]) - placement.Y();
				positions.emplace_back(
					static_cast<float>(NmfPreviewCenterX + (localX - frame.sourceCenterX) * s),
					static_cast<float>((localY - frame.sourceMinimumY) * s),
					static_cast<float>((localZ - frame.sourceCenterZ) * s));
			}
		}
		std::vector<std::uint32_t> indices;
		indices.reserve(static_cast<std::size_t>(kRegionSamples - 1) * (kRegionSamples - 1) * 6);
		for (std::uint32_t z = 0; z + 1 < kRegionSamples; ++z)
		{
			for (std::uint32_t x = 0; x + 1 < kRegionSamples; ++x)
			{
				const std::uint32_t a = z * kRegionSamples + x;
				const std::uint32_t b = a + 1;
				const std::uint32_t c = a + kRegionSamples;
				const std::uint32_t d = c + 1;
				indices.insert(indices.end(), { a, b, c, b, d, c });
			}
		}
		const wi::ecs::Entity terrainEntity = scene.Entity_CreateMeshFromData(
			"OpenRepublic map terrain around placement", indices.size(), indices.data(),
			positions.size(), positions.data());
		wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(terrainEntity);
		wi::scene::MaterialComponent* material = scene.materials.GetComponent(terrainEntity);
		if (mesh == nullptr || material == nullptr)
		{
			error = "Wicked Engine did not create the map terrain components";
			return false;
		}
		mesh->CreateRenderData();
		material->SetBaseColor(XMFLOAT4(0.30f, 0.42f, 0.24f, 1.0f));
		material->SetRoughness(1.0f);
		material->SetReflectance(0.0f);
		material->CreateRenderData(true);

		std::ostringstream summary;
		summary << "Map terrain around placement " << request.placementIndex + 1 << '/'
			<< request.placementCount << " (" << placement.typeName << " at x=" << placement.X()
			<< " y=" << placement.Y() << " z=" << placement.Z() << ", yaw " << placement.YawRadians()
			<< " rad); region " << (maxX - minX) << " x " << (maxZ - minZ) << " m from "
			<< request.heightmap.width << 'x' << request.heightmap.height
			<< " R32F; transform: centred extent, height = raw*" << request.metadata.heightScale << '+'
			<< request.metadata.worldPositionY << ", rows ascend with Z (verified on one map package)";
		description = summary.str();
		return true;
	}

	bool CreateMapRegionPreview(wi::scene::Scene& scene,
		const openrepublic::content::MapPlacementRequest& request,
		const std::filesystem::path& contentRoot, double radiusMeters, int modelLevel, bool enhancedLook,
		XMFLOAT3& cameraTarget, float& cameraDistance, std::string& description, std::string& error,
		std::vector<RegionInstance>* outInstances, ShowcaseEnvironmentInfo* environmentInfo,
		BuildingLodController* lodController, bool dynamicLod, float lodNearDistance, float lodFarDistance)
	{
		ShowcasePreviewTimer totalTimer("CreateMapRegionPreview total");
		using namespace openrepublic::content;
		constexpr std::size_t kMaxRegionObjects = 10000;
		constexpr std::size_t kMaxRegionTypes = 160;
		if (modelLevel < 0 || modelLevel > 2)
		{
			error = "map region model level must be 0, 1 or 2";
			return false;
		}
		if (dynamicLod && (lodController == nullptr || !std::isfinite(lodNearDistance) ||
			!std::isfinite(lodFarDistance) || lodNearDistance <= 0.0f || lodFarDistance <= lodNearDistance))
		{
			error = "dynamic map LOD requires a controller and finite 0 < near < far distances";
			return false;
		}
		if (dynamicLod && !lodController->Configure(lodNearDistance, lodFarDistance))
		{
			error = "dynamic map LOD distance configuration is invalid";
			return false;
		}
		if (lodController != nullptr) lodController->Clear();
		const double radius = radiusMeters > 0.0 ? radiusMeters : 400.0;
		const PlacedBuilding& center = request.placement;

		BuildingTypesDocument types;
		if (!LoadBuildingTypesFile(contentRoot, types, &error))
		{
			return false;
		}
		const std::vector<std::size_t> selected =
			SelectPlacementsInRadius(request.allPlacements, request.placementIndex, radius);

		// Terrain first: a 1:1 mesh around the region centre, in centre-relative metres.
		const TerrainTransform transform = MakeMapTerrainTransform(request.metadata);
		const double regionRadius = radius * 1.25;
		const double minX = std::max(transform.originX, center.X() - regionRadius);
		const double maxX = std::min(transform.originX + transform.sizeX, center.X() + regionRadius);
		const double minZ = std::max(transform.originZ, center.Z() - regionRadius);
		const double maxZ = std::min(transform.originZ + transform.sizeZ, center.Z() + regionRadius);
		constexpr std::uint32_t kSamples = 257;
		TerrainPreviewGrid grid;
		if (!BuildTerrainRegionGrid(request.heightmap, transform, minX, minZ, maxX - minX, maxZ - minZ,
			kSamples, kSamples, grid, &error))
		{
			return false;
		}
		ShowcaseTerrainData showcaseTerrain;
		if (enhancedLook)
		{
			ShowcasePreviewTimer timer("PrepareShowcaseTerrain");
			if (!PrepareShowcaseTerrain(request, regionRadius, showcaseTerrain, error))
			{
				return false;
			}
		}
		if (enhancedLook)
		{
			// Authentic ground tile from the local install when present (never committed).
			const auto loadGround = [&](const char* name)
			{
				std::error_code fileError;
				const std::filesystem::path path = contentRoot / "tiles_normal" / name;
				return std::filesystem::is_regular_file(path, fileError) ? wi::resourcemanager::Load(path.generic_string()) : wi::Resource();
			};
			const wi::Resource ground = loadGround("grassrock1.dds");
			if (ground.IsValid())
			{
				SetShowcaseRealGroundTexture(ground, loadGround("grass1bump.dds"));
				wilog("[OpenRepublic] Showcase ground uses the local install's grassrock1 tile (local-only presentation).");
			}
		}
		std::vector<XMFLOAT3> positions;
		positions.reserve(static_cast<std::size_t>(kSamples) * kSamples);
		for (std::uint32_t z = 0; z < kSamples; ++z)
		{
			for (std::uint32_t x = 0; x < kSamples; ++x)
			{
				positions.emplace_back(
					static_cast<float>(grid.worldXCoordinates[x] - center.X()),
					grid.heights[static_cast<std::size_t>(z) * kSamples + x],
					static_cast<float>(grid.worldZCoordinates[z] - center.Z()));
			}
		}
		std::vector<std::uint32_t> indices;
		indices.reserve(static_cast<std::size_t>(kSamples - 1) * (kSamples - 1) * 6);
		for (std::uint32_t z = 0; z + 1 < kSamples; ++z)
		{
			for (std::uint32_t x = 0; x + 1 < kSamples; ++x)
			{
				const std::uint32_t a = z * kSamples + x;
				const std::uint32_t b = a + 1;
				const std::uint32_t c = a + kSamples;
				const std::uint32_t d = c + 1;
				indices.insert(indices.end(), { a, b, c, b, d, c });
			}
		}
		const wi::ecs::Entity terrainEntity = scene.Entity_CreateMeshFromData(
			"OpenRepublic map region terrain", indices.size(), indices.data(), positions.size(),
			positions.data());
		wi::scene::MeshComponent* terrainMesh = scene.meshes.GetComponent(terrainEntity);
		wi::scene::MaterialComponent* terrainMaterial = scene.materials.GetComponent(terrainEntity);
		if (terrainMesh == nullptr || terrainMaterial == nullptr)
		{
			error = "Wicked Engine did not create the map region terrain components";
			return false;
		}
		if (enhancedLook)
		{
			// OpenRepublic presentation styling over the verified height mesh (not an
			// interpretation of the map's unknown masks); shared with the far landscape so
			// the two meshes shade identically.
			const auto gridHeight = [&](std::uint32_t x, std::uint32_t z)
			{
				return grid.heights[static_cast<std::size_t>(z) * kSamples + x];
			};
			terrainMesh->vertex_colors.reserve(positions.size());
			terrainMesh->vertex_uvset_0.reserve(positions.size());
			for (std::uint32_t z = 0; z < kSamples; ++z)
			{
				for (std::uint32_t x = 0; x < kSamples; ++x)
				{
					const std::uint32_t left = x == 0 ? x : x - 1;
					const std::uint32_t right = std::min(x + 1, kSamples - 1);
					const std::uint32_t down = z == 0 ? z : z - 1;
					const std::uint32_t up = std::min(z + 1, kSamples - 1);
					const double dx = grid.worldXCoordinates[right] - grid.worldXCoordinates[left];
					const double dz = grid.worldZCoordinates[up] - grid.worldZCoordinates[down];
					const float slopeX = dx > 0.0 ? static_cast<float>((gridHeight(right, z) - gridHeight(left, z)) / dx) : 0.0f;
					const float slopeZ = dz > 0.0 ? static_cast<float>((gridHeight(x, up) - gridHeight(x, down)) / dz) : 0.0f;
					const float slope = std::sqrt(slopeX * slopeX + slopeZ * slopeZ);
					const double worldX = grid.worldXCoordinates[x];
					const double worldZ = grid.worldZCoordinates[z];
					const XMFLOAT3 color = ShadeShowcaseTerrain(showcaseTerrain, worldX, worldZ, gridHeight(x, z), slope);
					terrainMesh->vertex_colors.push_back(wi::Color::fromFloat4(XMFLOAT4(color.x, color.y, color.z, 1.0f)).rgba);
					terrainMesh->vertex_uvset_0.emplace_back(static_cast<float>(worldX / 22.0), static_cast<float>(worldZ / 22.0));
				}
			}
			terrainMaterial->SetUseVertexColors(true);
			terrainMaterial->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
			ApplyShowcaseGrass(*terrainMaterial);
		}
		else
		{
			terrainMaterial->SetBaseColor(XMFLOAT4(0.30f, 0.42f, 0.24f, 1.0f));
		}
		terrainMesh->CreateRenderData();
		terrainMaterial->SetRoughness(enhancedLook ? 0.92f : 1.0f);
		terrainMaterial->SetReflectance(enhancedLook ? 0.04f : 0.0f);
		terrainMaterial->CreateRenderData(true);

		// Roads are OpenRepublic preview geometry with an explicit comparison switch.
		std::size_t roadEdges = 0;
		const char* oldRoadSetting = std::getenv("OR_OLD_ROADS");
		const bool useOldRoads = oldRoadSetting != nullptr && std::string_view(oldRoadSetting) == "1";
		if (request.hasRoads && !useOldRoads)
		{
			RoadMeshOptions options;
			options.maximumDistance = regionRadius * 1.1;
			options.centerX = center.X();
			options.centerZ = center.Z();
			const char* sidewalksSetting = std::getenv("OR_ROAD_SIDEWALKS");
			options.buildSidewalks = sidewalksSetting != nullptr && std::string_view(sidewalksSetting) == "1";
			const RoadPathIndex roadPaths(request.roads);
			const auto heightAt = [&](double wx, double wz)
			{
				double h = 0.0;
				const double cx = std::min(std::max(wx, transform.originX), transform.originX + transform.sizeX);
				const double cz = std::min(std::max(wz, transform.originZ), transform.originZ + transform.sizeZ);
				SampleTerrainHeight(request.heightmap, transform, cx, cz, h, nullptr);
				return h;
			};
			RoadMeshBuffers meshes;
			if (BuildRoadMesh(request.roads, roadPaths, heightAt, options, meshes))
			{
				roadEdges = meshes.edgeCount;
				const auto addRoadMesh = [&](const RoadMeshBuffer& source, const char* name, int materialKind)
				{
					if (source.indices.empty()) return;
					std::vector<XMFLOAT3> positions;
					std::vector<XMFLOAT2> uvs;
					std::vector<std::uint32_t> colors;
					positions.reserve(source.vertices.size());
					uvs.reserve(source.vertices.size());
					colors.reserve(source.vertices.size());
					for (const auto& vertex : source.vertices)
					{
						positions.emplace_back(static_cast<float>(vertex.x - center.X()), static_cast<float>(vertex.y),
							static_cast<float>(vertex.z - center.Z()));
						uvs.emplace_back(vertex.u, vertex.v);
						colors.push_back(wi::Color::fromFloat4(XMFLOAT4(vertex.r, vertex.g, vertex.b, vertex.a)).rgba);
					}
					const wi::ecs::Entity entity = scene.Entity_CreateMeshFromData(name, source.indices.size(),
						source.indices.data(), positions.size(), positions.data());
					auto* mesh = scene.meshes.GetComponent(entity);
					auto* material = scene.materials.GetComponent(entity);
					if (mesh == nullptr || material == nullptr) return;
					mesh->vertex_uvset_0 = std::move(uvs);
					mesh->vertex_colors = std::move(colors);
					mesh->CreateRenderData();
					material->SetUseVertexColors(materialKind >= 1);
					if (materialKind == 0 && enhancedLook)
					{
						material->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
						material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource = ShowcaseAsphaltTexture();
						material->textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 0;
						material->SetRoughness(0.55f);
						material->SetReflectance(0.28f);
					}
					else if (materialKind == 0)
					{
						material->SetBaseColor(XMFLOAT4(0.16f, 0.17f, 0.19f, 1));
					}
					else if (materialKind == 1)
					{
						material->SetBaseColor(enhancedLook
							? XMFLOAT4(0.20f, 0.21f, 0.22f, 1) : XMFLOAT4(0.16f, 0.17f, 0.19f, 1));
					}
					else if (materialKind == 2)
					{
						material->SetBaseColor(XMFLOAT4(0.55f, 0.50f, 0.40f, 1));
					}
					else
					{
						material->SetBaseColor(XMFLOAT4(0.78f, 0.77f, 0.71f, 1));
					}
					material->SetRoughness(materialKind == 3 ? 0.85f : 0.95f);
					material->SetDoubleSided(true);
					material->CreateRenderData(true);
				};
				addRoadMesh(meshes.pavement, "OpenRepublic map road pavement", 0);
				addRoadMesh(meshes.junctions, "OpenRepublic map road junctions", 1);
				addRoadMesh(meshes.shoulders, "OpenRepublic map road shoulders", 2);
				addRoadMesh(meshes.sidewalks, "OpenRepublic map road sidewalks", 3);
			}
		}
		if (request.hasRoads && useOldRoads)
		{
			constexpr float kRoadHalfWidth = 2.5f;
			constexpr float kRoadLift = 0.25f;
			constexpr double kRoadStep = 8.0;
			constexpr std::size_t kMaxRoadVertices = 240000;
			std::vector<XMFLOAT3> roadPositions;
			std::vector<XMFLOAT2> roadUvs;
			std::vector<std::uint32_t> roadIndices;
			const double limit = regionRadius * 1.1;
			const auto& nodes = request.roads.nodes;
			const RoadPathIndex roadPaths(request.roads);
			std::size_t curvedRoadEdges = 0;
			const auto heightAt = [&](double wx, double wz)
			{
				double h = 0.0;
				const double cx = std::min(std::max(wx, transform.originX), transform.originX + transform.sizeX);
				const double cz = std::min(std::max(wz, transform.originZ), transform.originZ + transform.sizeZ);
				SampleTerrainHeight(request.heightmap, transform, cx, cz, h, nullptr);
				return h;
			};
			for (std::size_t i = 0; i < nodes.size() && roadPositions.size() < kMaxRoadVertices; ++i)
			{
				for (const std::uint32_t j : nodes[i].neighbors)
				{
					if (j <= i || j >= nodes.size())
					{
						continue;
					}
					const double ax = nodes[i].x;
					const double az = nodes[i].z;
					const double bx = nodes[j].x;
					const double bz = nodes[j].z;
					const bool inside =
						(std::fabs(ax - center.X()) < limit && std::fabs(az - center.Z()) < limit) ||
						(std::fabs(bx - center.X()) < limit && std::fabs(bz - center.Z()) < limit);
					const double length = std::hypot(bx - ax, bz - az);
					if (!inside || length < 0.5)
					{
						continue;
					}
			// Comparison path: prior fixed-spacing, independent edge ribbons.
					const std::vector<RoadSample> samples = RoadPathIndex::Resample(
						roadPaths.Path(static_cast<std::uint32_t>(i), j), kRoadStep);
					if (samples.size() < 2)
					{
						continue;
					}
					if (roadPaths.HasCurve(static_cast<std::uint32_t>(i), j))
					{
						++curvedRoadEdges;
					}
					const std::uint32_t base = static_cast<std::uint32_t>(roadPositions.size());
					for (const RoadSample& sample : samples)
					{
						const float y = static_cast<float>(heightAt(sample.x, sample.z)) + kRoadLift;
						roadPositions.emplace_back(static_cast<float>(sample.x - center.X() - sample.dz * kRoadHalfWidth), y,
							static_cast<float>(sample.z - center.Z() + sample.dx * kRoadHalfWidth));
						roadPositions.emplace_back(static_cast<float>(sample.x - center.X() + sample.dz * kRoadHalfWidth), y,
							static_cast<float>(sample.z - center.Z() - sample.dx * kRoadHalfWidth));
						// u across the 5 m ribbon, v along it (one texture repeat per 16 m).
						roadUvs.emplace_back(0.0f, static_cast<float>(sample.distance / 16.0));
						roadUvs.emplace_back(1.0f, static_cast<float>(sample.distance / 16.0));
					}
					for (std::size_t s = 0; s + 1 < samples.size(); ++s)
					{
						const std::uint32_t a = base + 2u * static_cast<std::uint32_t>(s);
						roadIndices.insert(roadIndices.end(), { a, a + 1, a + 2, a + 1, a + 3, a + 2 });
					}
					++roadEdges;
				}
			}
			if (!roadIndices.empty())
			{
				const wi::ecs::Entity roadEntity = scene.Entity_CreateMeshFromData(
					"OpenRepublic map region roads", roadIndices.size(), roadIndices.data(),
					roadPositions.size(), roadPositions.data());
				wi::scene::MeshComponent* roadMesh = scene.meshes.GetComponent(roadEntity);
				wi::scene::MaterialComponent* roadMaterial = scene.materials.GetComponent(roadEntity);
				if (roadMesh != nullptr && roadMaterial != nullptr)
				{
					if (enhancedLook)
					{
						roadMesh->vertex_uvset_0 = roadUvs;
					}
					roadMesh->CreateRenderData();
					roadMaterial->SetBaseColor(enhancedLook ? XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)
						: XMFLOAT4(0.16f, 0.17f, 0.19f, 1.0f));
					if (enhancedLook)
					{
						roadMaterial->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource = ShowcaseAsphaltTexture();
						roadMaterial->textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 0;
					}
					roadMaterial->SetRoughness(enhancedLook ? 0.55f : 1.0f);
					roadMaterial->SetReflectance(enhancedLook ? 0.35f : 0.0f);
					roadMaterial->SetDoubleSided(true);
					roadMaterial->CreateRenderData(true);
				}
			}
		}

		// Buildings: one hidden source mesh per distinct type and available LOD,
		// plus one placement object whose meshID is switched by the optional controller.
		struct TypeMeshes
		{
			BuildingLodController::MeshVariants byLod{
				wi::ecs::INVALID_ENTITY, wi::ecs::INVALID_ENTITY, wi::ecs::INVALID_ENTITY };
			int maximumLod = 0;
			int initialLod = -1;
		};
		std::map<std::string, TypeMeshes> meshByType;
		std::size_t instances = 0;
		std::size_t unresolvedTypes = 0;
		std::size_t failedLoads = 0;
		std::size_t skippedByLimit = 0;
		std::size_t resolvedTypes = 0;
		std::set<std::string> missingTypeNames;
		std::set<std::string> lodFallbackTypeNames;
		std::size_t rejectedMisalignedLods = 0;
		for (const std::size_t index : selected)
		{
			const PlacedBuilding& placed = request.allPlacements[index];
			if (instances >= kMaxRegionObjects)
			{
				++skippedByLimit;
				continue;
			}
			auto cached = meshByType.find(placed.typeName);
			if (cached == meshByType.end())
			{
				TypeMeshes typeMeshes;
				std::string assetError;
				if (meshByType.size() < kMaxRegionTypes)
				{
					// `MIRRORZ_<type>` is the loader's mirrored derivative of <type>.
					const std::string mirrorPrefix = "MIRRORZ_";
					const bool mirrored = placed.typeName.rfind(mirrorPrefix, 0) == 0;
					const std::string baseName = mirrored ? placed.typeName.substr(mirrorPrefix.size())
						: placed.typeName;
					const BuildingTypeDefinition* type = FindBuildingType(types, baseName);
					NmfPreviewFrame referenceBounds;
					bool haveReferenceBounds = false;
					if (type == nullptr)
					{
						assetError = "building type definition was not found";
					}
					else
					{
						const int firstCandidate = dynamicLod ? 0 : modelLevel;
						const int lastCandidate = dynamicLod ? modelLevel : 0;
						const int step = dynamicLod ? 1 : -1;
						for (int candidateLod = firstCandidate;
							(dynamicLod ? candidateLod <= lastCandidate : candidateLod >= lastCandidate);
							candidateLod += step)
						{
							BuildingTypeAssetPaths assets;
							std::string candidateError;
							if ((candidateLod == 2 && !type->lod2.has_value()) ||
								(candidateLod == 1 && !type->lod1.has_value()) ||
								!ResolveBuildingTypeAssets(*type, candidateLod, contentRoot, assets, &candidateError))
							{
								if (!candidateError.empty()) assetError = candidateError;
								continue;
							}
							wi::ecs::Entity meshEntity = wi::ecs::INVALID_ENTITY;
							XMFLOAT3 ignoredTarget;
							float ignoredDistance = 0.0f;
							std::string modelDescription, modelAudit, modelError;
							NmfPreviewFrame bounds;
							if (!CreateNmfStaticPreview(scene, assets.model, meshEntity, ignoredTarget, ignoredDistance,
								false, false, { contentRoot }, assets.material, modelDescription, modelAudit, modelError,
								&bounds, true, mirrored))
							{
								assetError = modelError;
								++failedLoads;
								wilog("[OpenRepublic] Map region type '%s' LOD %d failed to load: %s",
									placed.typeName.c_str(), candidateLod, modelError.c_str());
								continue;
							}
							if (wi::scene::ObjectComponent* own = scene.objects.GetComponent(meshEntity)) own->SetRenderable(false);
							if (!haveReferenceBounds)
							{
								referenceBounds = bounds;
								haveReferenceBounds = true;
							}
							else
							{
								const double tolerance = std::max(0.05, referenceBounds.sourceMaxHorizontalExtent * 0.01);
								const bool aligned =
									std::fabs(bounds.sourceMinimumX - referenceBounds.sourceMinimumX) <= tolerance &&
									std::fabs(bounds.sourceMaximumX - referenceBounds.sourceMaximumX) <= tolerance &&
									std::fabs(bounds.sourceMinimumY - referenceBounds.sourceMinimumY) <= tolerance &&
									std::fabs(bounds.sourceMaximumY - referenceBounds.sourceMaximumY) <= tolerance &&
									std::fabs(bounds.sourceMinimumZ - referenceBounds.sourceMinimumZ) <= tolerance &&
									std::fabs(bounds.sourceMaximumZ - referenceBounds.sourceMaximumZ) <= tolerance &&
									std::fabs(bounds.displayScale - referenceBounds.displayScale) <= 1e-9;
								if (!aligned)
								{
									++rejectedMisalignedLods;
									assetError = "LOD source bounds do not align within 1% of the reference footprint (minimum 0.05 m)";
									wilog("[OpenRepublic] Map region type '%s' LOD %d rejected: source bounds do not align with LOD %d",
										placed.typeName.c_str(), candidateLod, typeMeshes.initialLod);
									continue;
								}
							}
							typeMeshes.byLod[static_cast<std::size_t>(candidateLod)] = meshEntity;
							if (typeMeshes.initialLod < 0) typeMeshes.initialLod = candidateLod;
							if (!dynamicLod) break;
						}
					}
					typeMeshes.maximumLod = modelLevel;
				}
				if (typeMeshes.initialLod < 0)
				{
					++unresolvedTypes;
					missingTypeNames.insert(placed.typeName);
					wilog("[OpenRepublic] Map region type '%s' has no usable model at or below LOD %d: %s",
						placed.typeName.c_str(), modelLevel, assetError.c_str());
				}
				else
				{
					++resolvedTypes;
					const bool missingDynamicVariant = dynamicLod &&
						std::any_of(typeMeshes.byLod.begin(), typeMeshes.byLod.begin() + modelLevel + 1,
							[](wi::ecs::Entity id) { return id == wi::ecs::INVALID_ENTITY; });
					if (typeMeshes.initialLod != modelLevel || missingDynamicVariant)
					{
						lodFallbackTypeNames.insert(placed.typeName);
					}
				}
				cached = meshByType.emplace(placed.typeName, std::move(typeMeshes)).first;
			}
			TypeMeshes& typeMeshes = cached->second;
			if (typeMeshes.initialLod < 0)
			{
				continue;
			}
			const wi::ecs::Entity instance = scene.Entity_CreateObject(placed.typeName);
			const int initialLod = typeMeshes.initialLod;
			scene.objects.GetComponent(instance)->meshID = typeMeshes.byLod[static_cast<std::size_t>(initialLod)];
			XMFLOAT4X4 matrix(placed.matrix.data());
			matrix._41 = static_cast<float>(placed.X() - center.X());
			matrix._43 = static_cast<float>(placed.Z() - center.Z());
			scene.transforms.GetComponent(instance)->MatrixTransform(matrix);
			if (dynamicLod && lodController != nullptr)
			{
				lodController->AddInstance(instance, typeMeshes.byLod, typeMeshes.maximumLod,
					XMFLOAT3(matrix._41, matrix._42, matrix._43));
			}
			if (outInstances != nullptr)
			{
				const std::string prefix = "MIRRORZ_";
				const bool mirroredName = placed.typeName.rfind(prefix, 0) == 0;
				outInstances->push_back({ instance,
					mirroredName ? placed.typeName.substr(prefix.size()) : placed.typeName,
					placed.X(), placed.Z(), static_cast<std::uint64_t>(index) });
			}
			++instances;
		}

		std::string environmentSummary;
		if (enhancedLook)
		{
			// Material response: the converted building materials default to fully rough;
			// give them a satin, slightly reflective finish so sun and sky read on facades.
			for (const auto& entry : meshByType)
			{
				for (const wi::ecs::Entity meshEntity : entry.second.byLod)
				{
					const wi::scene::MeshComponent* mesh = meshEntity == wi::ecs::INVALID_ENTITY
						? nullptr : scene.meshes.GetComponent(meshEntity);
					if (mesh == nullptr) continue;
					for (const auto& subset : mesh->subsets)
					{
						if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(subset.materialID))
						{
							material->SetRoughness(0.62f);
							material->SetReflectance(0.30f);
							// A faint self-illumination from the albedo texture stands in for the bounced sky
							// light the renderer does not compute, lifting dark facades toward concrete tones.
							auto& baseMap = material->textures[wi::scene::MaterialComponent::BASECOLORMAP];
							if (baseMap.resource.IsValid())
							{
								material->textures[wi::scene::MaterialComponent::EMISSIVEMAP] = baseMap;
								material->SetEmissiveColor(XMFLOAT4(1.0f, 0.97f, 0.9f, 0.45f));
							}
							material->CreateRenderData(true);
						}
					}
				}
			}
			// Authentic ground and trees from the local install when present (never committed).
			RealTreeSet realTrees;
			openrepublic::content::TreePlacements realPlacements;
			{
				ShowcasePreviewTimer realTreeTimer("real tree mesh loading");
				const auto loadResource = [&](const std::filesystem::path& path)
				{
					std::error_code fileError;
					if (!std::filesystem::is_regular_file(path, fileError))
					{
						return wi::Resource();
					}
					return wi::resourcemanager::Load(path.generic_string());
				};
				const auto loadTree = [&](const char* stem, std::vector<wi::ecs::Entity>& into)
				{
					const std::filesystem::path model = contentRoot / "trees" / (std::string(stem) + ".nmf");
					std::string materialStem = stem;
					std::filesystem::path material = contentRoot / "trees" / (materialStem + ".mtl");
					std::error_code fileError;
					while (!std::filesystem::is_regular_file(material, fileError) && !materialStem.empty() && std::isdigit(static_cast<unsigned char>(materialStem.back())))
					{
						materialStem.pop_back();
						material = contentRoot / "trees" / (materialStem + ".mtl");
					}
					if (!std::filesystem::is_regular_file(model, fileError) || !std::filesystem::is_regular_file(material, fileError))
					{
						return;
					}
					wi::ecs::Entity meshEntity = wi::ecs::INVALID_ENTITY;
					XMFLOAT3 ignoredTarget;
					float ignoredDistance = 0.0f;
					std::string d, a, e;
					if (!CreateNmfStaticPreview(scene, model, meshEntity, ignoredTarget, ignoredDistance, false, false,
						{ contentRoot }, material, d, a, e, nullptr, true, false))
					{
						wilog("[OpenRepublic] Showcase tree '%s' skipped: %s", stem, e.c_str());
						return;
					}
					wilog("[OpenRepublic] Showcase tree '%s': %s", stem, d.c_str());
					if (wi::scene::ObjectComponent* own = scene.objects.GetComponent(meshEntity))
					{
						own->SetRenderable(false);
					}
					// The shared preview loader rejects these MTLs (blank-line/spacing profile), so read the
					// slot-0 texture line directly: `$TEXTURE 0 <path relative to the content root>`.
					wi::Resource leafTexture;
					{
						std::ifstream mtlStream(material, std::ios::binary);
						std::string line;
						while (std::getline(mtlStream, line))
						{
							if (line.rfind("$TEXTURE 0 ", 0) == 0)
							{
								std::string relative = line.substr(11);
								while (!relative.empty() && (relative.back() == '' || relative.back() == ' ')) relative.pop_back();
								const std::filesystem::path texturePath = contentRoot / relative;
								if (std::filesystem::is_regular_file(texturePath, fileError))
								{
									leafTexture = wi::resourcemanager::Load(texturePath.generic_string());
								}
								break;
							}
						}
					}
					if (const wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(meshEntity))
					{
						for (const auto& subset : mesh->subsets)
						{
							if (wi::scene::MaterialComponent* m = scene.materials.GetComponent(subset.materialID))
							{
								if (leafTexture.IsValid())
								{
									m->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
									m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource = leafTexture;
									m->textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 0;
									m->SetTextureStreamingDisabled(true);
								}
								m->SetAlphaRef(0.8f);
								if (leafTexture.IsValid())
								{
									// Stand-in for bounced sky light: faint self-illumination from the leaf texture.
									m->textures[wi::scene::MaterialComponent::EMISSIVEMAP] = m->textures[wi::scene::MaterialComponent::BASECOLORMAP];
									m->SetEmissiveColor(XMFLOAT4(1.0f, 1.0f, 0.9f, 0.35f));
								}
								m->SetDoubleSided(true);
								m->SetRoughness(0.9f);
								m->SetReflectance(0.02f);
								m->CreateRenderData(true);
							}
						}
					}
					into.push_back(meshEntity);
					realTrees.byName[stem] = meshEntity;
				};
				for (const char* stem : { "pine1", "pine2", "pine3", "pine4", "oldpine1", "oldpine2", "oldpine3" }) loadTree(stem, realTrees.conifers);
				for (const char* stem : { "strom", "afghan_leaved1a", "afghan_leaved2a" }) loadTree(stem, realTrees.broadleaf);
				for (const char* stem : { "topol" }) loadTree(stem, realTrees.poplars);
				for (const char* stem : { "broadbush1", "broadbush2", "broadbush3", "kriak", "kriak2", "kriak3" }) loadTree(stem, realTrees.shrubs);
				// The package's own tree species (trees.bin) and positions.
				bool loadedTreePlacements = false;
				{
					ShowcasePreviewTimer timer("LoadTreePlacements");
					loadedTreePlacements = LoadTreePlacements(request.mapDirectory, realPlacements, nullptr);
				}
				if (loadedTreePlacements)
				{
					realTrees.placements = &realPlacements;
					for (const auto& group : realPlacements.species)
					{
						if (realTrees.byName.find(group.modelName) == realTrees.byName.end())
						{
							std::vector<wi::ecs::Entity> unused;
							loadTree(group.modelName.c_str(), unused);
						}
					}
					wilog("[OpenRepublic] Showcase uses %zu real tree placements from trees.bin (%zu species)",
						realPlacements.placements.size(), realPlacements.species.size());
				}
				wilog("[OpenRepublic] Showcase real tree meshes: %zu conifers, %zu broadleaf, %zu poplars, %zu shrubs",
					realTrees.conifers.size(), realTrees.broadleaf.size(), realTrees.poplars.size(), realTrees.shrubs.size());
			}
			ShowcaseEnvironmentInfo localInfo;
			ShowcaseEnvironmentInfo& info = environmentInfo != nullptr ? *environmentInfo : localInfo;
			std::string environmentError;
			std::vector<RegionInstance> localInstances;
			for (const std::size_t index : selected)
			{
				const PlacedBuilding& placed = request.allPlacements[index];
				localInstances.push_back({ wi::ecs::INVALID_ENTITY, placed.typeName, placed.X(), placed.Z(), index });
			}
			if (BuildShowcaseWorld(scene, request, showcaseTerrain, regionRadius, localInstances, &realTrees, info, environmentError))
			{
				environmentSummary = info.summary;
				wilog("[OpenRepublic] %s", info.summary.c_str());
			}
			else
			{
				wilog("[OpenRepublic] Showcase environment skipped: %s", environmentError.c_str());
			}
			{
				ShowcasePropsInfo propsInfo;
				std::string propsError;
				if (BuildShowcaseProps(scene, request, showcaseTerrain, regionRadius, contentRoot, propsInfo, propsError))
				{
					wilog("[OpenRepublic] %s", propsInfo.summary.c_str());
				}
				else
				{
					wilog("[OpenRepublic] Showcase props skipped: %s", propsError.c_str());
				}
			}
		}

		cameraTarget = XMFLOAT3(0.0f, static_cast<float>(center.Y()), 0.0f);
		cameraDistance = static_cast<float>(radius * 1.5);
		std::ostringstream summary;
		summary << "Map region: " << instances << " building instances of "
			<< resolvedTypes << " models within " << radius
			<< " m of placement " << request.placementIndex + 1 << '/' << request.placementCount << " at requested LOD "
			<< modelLevel << " ("
			<< center.typeName << "); " << selected.size() << " placements selected, " << unresolvedTypes
			<< " unresolved types, " << failedLoads << " failed loads, " << lodFallbackTypeNames.size()
			<< " types used LOD fallback, " << rejectedMisalignedLods << " LODs rejected by source-bounds alignment, "
			<< skippedByLimit
			<< " skipped by the instance/type limits; " << roadEdges << " road links drawn (decoded curves where available, else chords); 1:1 metres,"
			<< " verified terrain transform, placement"
			<< " matrices applied as stored";
		if (dynamicLod)
		{
			summary << "; dynamic building LOD thresholds " << lodNearDistance << "/" << lodFarDistance
				<< " m with 8% hysteresis";
		}
		description = summary.str();
		return true;
	}
}
