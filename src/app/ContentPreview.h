#pragma once

#include "WickedEngine.h"
#include "openrepublic/content/glb_container.h"
#include "EconomyOverlay.h"
#include "openrepublic/content/glb_semantic.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/terrain_dds.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

// Content-preview paths for developer probes: bounded local NMF/MTL, terrain
// DDS and embedded GLB inputs rendered through Wicked Engine. This module owns
// no simulation state and none of its inputs are part of the OpenRepublic
// save format.
namespace openrepublic::preview
{
	using namespace DirectX;

	inline constexpr char kGlbProbeLimitations[] =
		"Trusted local inputs only: a bounded embedded-resource profile is checked before importing an exact temporary snapshot; "
		"the pinned importer and image decoders are not sandboxed, and this is not a licensing assessment.";

	bool HasValidIndexedTriangleData(const wi::scene::MeshComponent& mesh);

	bool ValidateEmbeddedGlbProfile(const std::vector<std::uint8_t>& bytes,
		const openrepublic::content::GlbContainerInfo& info,
		openrepublic::content::GlbSemanticSummary& summary, std::string& error);

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

	bool CreateTerrainPreview(wi::scene::Scene& scene, const std::filesystem::path& heightmapPath,
		const std::optional<openrepublic::content::TerrainTransform>& explicitTransform,
		wi::ecs::Entity& terrainEntity, std::string& description, std::string& error);

	// How the NMF preview maps source model coordinates into preview space:
	// preview = (NmfPreviewCenterX + (x - centerX) * scale, (y - minimumY) * scale,
	// (z - centerZ) * scale), where the centre is the midpoint of the source bounds.
	struct NmfPreviewFrame
	{
		double displayScale = 0.0;
		double sourceMinimumY = 0.0;
		double sourceCenterX = 0.0;
		double sourceCenterZ = 0.0;
		double sourceMaxHorizontalExtent = 0.0;
	};

	bool CreateNmfStaticPreview(wi::scene::Scene& scene, const std::filesystem::path& nmfPath,
		wi::ecs::Entity& meshEntity, XMFLOAT3& previewCameraTarget, float& previewCameraDistance,
		bool unlitDiagnostic, bool materialUnlitDiagnostic,
		const std::vector<std::filesystem::path>& textureRoots,
		const std::filesystem::path& explicitMtlPath,
		std::string& description, std::string& geometryAuditSummary,
		std::string& error, NmfPreviewFrame* frame = nullptr, bool sourceUnits = false,
		bool mirrorZ = false);

	// Adds the terrain around a placed building in that building's local frame so
	// it lines up with the NMF preview: the region around the placement is sampled
	// with the map's verified terrain transform, rotated by the placement matrix
	// and mapped through `frame` (1:1, no vertical exaggeration). `radiusMeters`
	// <= 0 selects an automatic radius from the model size.
	// Shows every placement within `radiusMeters` of the request's selected placement
	// (instanced 1:1 at its authentic matrix) on the map terrain around it. Types that
	// are missing from the building-type data or fail to load are skipped and counted.
	// The preview frame is the region centre: camera target/distance are set for it.
	bool CreateMapRegionPreview(wi::scene::Scene& scene,
		const openrepublic::content::MapPlacementRequest& request,
		const std::filesystem::path& contentRoot, double radiusMeters,
		XMFLOAT3& cameraTarget, float& cameraDistance, std::string& description, std::string& error,
		std::vector<RegionInstance>* outInstances = nullptr);

	bool CreateMapTerrainAroundPlacement(wi::scene::Scene& scene,
		const openrepublic::content::MapPlacementRequest& request, const NmfPreviewFrame& frame,
		double radiusMeters, std::string& description, std::string& error);
}
