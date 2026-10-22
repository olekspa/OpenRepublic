#pragma once

#include "WickedEngine.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

namespace openrepublic
{
	using namespace DirectX;

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

		// Frames to wait after each camera change before reading back; temporal features
		// (FSR2, ray-traced denoisers) need history that matches the new view.
		void SetSettleFrames(std::uint32_t frames)
		{
			settleFrames = frames;
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
					settleRemaining = settleFrames;
				}
				return;
			}
			if (nextCapture >= outputPaths.size())
			{
				return;
			}
			if (settleRemaining > 0)
			{
				--settleRemaining;
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
				settleRemaining = settleFrames;
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
		std::uint32_t settleFrames = 0;
		std::uint32_t settleRemaining = 0;
	};
}
