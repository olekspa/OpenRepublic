#pragma once

#include "WickedEngine.h"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace openrepublic
{
	// One-shot capture path for an already-imported map region. The same fixed
	// camera parameters can be reused for baseline and enhanced render presets.
	class MapShowcaseCaptureRenderPath final : public wi::RenderPath3D
	{
	public:
		MapShowcaseCaptureRenderPath(std::filesystem::path outputPath,
			const XMFLOAT3& cameraTarget, float cameraDistance,
			float cameraYawDegrees, float cameraElevationDegrees)
			: outputPath(std::move(outputPath)), cameraTarget(cameraTarget), cameraDistance(cameraDistance),
			cameraYawDegrees(cameraYawDegrees), cameraElevationDegrees(cameraElevationDegrees)
		{
		}

		void PostRender() override
		{
			if (warmupFramesRemaining > 0)
			{
				--warmupFramesRemaining;
				if (warmupFramesRemaining == 0)
				{
					SetCamera();
					settleFramesRemaining = 48;
				}
				return;
			}
			if (settleFramesRemaining > 0)
			{
				--settleFramesRemaining;
				return;
			}
			if (captureFinished)
			{
				return;
			}
			captureFinished = true;

			std::uint64_t coveredPixels = 0;
			std::string error;
			if (!CountOpaqueDepthCoverage(depthBuffer_Main, coveredPixels, error))
			{
				wilog("[OpenRepublic] Map showcase depth readback failed: %s", error.c_str());
				PostQuitMessage(1);
				return;
			}
			if (coveredPixels == 0)
			{
				wilog("[OpenRepublic] Map showcase capture rejected: no geometry wrote main-depth pixels");
				PostQuitMessage(1);
				return;
			}

			const wi::graphics::TextureDesc& desc = GetRenderResult3D().GetDesc();
			std::filesystem::path temporaryPath;
			bool temporaryPathReserved = false;
			const std::wstring temporaryStem = outputPath.stem().wstring() + L".openrepublic-" +
				std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
			for (std::uint32_t attempt = 0; attempt < 64; ++attempt)
			{
				temporaryPath = outputPath.parent_path() /
					(temporaryStem + L"-" + std::to_wstring(attempt) + L".tmp.png");
				HANDLE reservation = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr,
					CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
				if (reservation != INVALID_HANDLE_VALUE)
				{
					CloseHandle(reservation);
					temporaryPathReserved = true;
					break;
				}
				const DWORD createError = GetLastError();
				if (createError != ERROR_FILE_EXISTS && createError != ERROR_ALREADY_EXISTS)
				{
					wilog("[OpenRepublic] Map showcase could not reserve a temporary output (Windows error %lu)",
						static_cast<unsigned long>(createError));
					PostQuitMessage(1);
					return;
				}
			}
			if (!temporaryPathReserved)
			{
				wilog("[OpenRepublic] Map showcase could not reserve a unique temporary output path");
				PostQuitMessage(1);
				return;
			}

			const std::string temporaryFilename = temporaryPath.string();
			if (!wi::helper::saveTextureToFile(GetRenderResult3D(), temporaryFilename))
			{
				DeleteFileW(temporaryPath.c_str());
				wilog("[OpenRepublic] Map showcase capture failed for %s", outputPath.filename().string().c_str());
				PostQuitMessage(1);
				return;
			}
			if (!MoveFileW(temporaryPath.c_str(), outputPath.c_str()))
			{
				const DWORD moveError = GetLastError();
				DeleteFileW(temporaryPath.c_str());
				wilog("[OpenRepublic] Map showcase capture could not promote output without overwrite (Windows error %lu)",
					static_cast<unsigned long>(moveError));
				PostQuitMessage(1);
				return;
			}

			const std::uint64_t pixelCount = static_cast<std::uint64_t>(desc.width) * desc.height;
			const double coverage = pixelCount == 0 ? 0.0 : 100.0 * static_cast<double>(coveredPixels) / pixelCount;
			wilog("[OpenRepublic] Map showcase capture saved: %s (%ux%u; depth coverage %.3f%%; 3D render output, GUI excluded)",
				outputPath.filename().string().c_str(), desc.width, desc.height, coverage);
			PostQuitMessage(0);
		}

	private:
		void SetCamera()
		{
			if (camera == nullptr)
			{
				return;
			}
			constexpr float degreesToRadians = 0.01745329251994329577f;
			const float yaw = cameraYawDegrees * degreesToRadians;
			const float elevation = cameraElevationDegrees * degreesToRadians;
			const float horizontal = std::cos(elevation);
			const XMFLOAT3 direction(std::sin(yaw) * horizontal, std::sin(elevation), std::cos(yaw) * horizontal);
			const XMFLOAT3 eye(
				cameraTarget.x + cameraDistance * direction.x,
				cameraTarget.y + cameraDistance * direction.y,
				cameraTarget.z + cameraDistance * direction.z);
			camera->Eye = eye;
			XMStoreFloat3(&camera->At, XMVector3Normalize(XMLoadFloat3(&cameraTarget) - XMLoadFloat3(&eye)));
			camera->Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
			camera->UpdateCamera();
			wilog("[OpenRepublic] Map showcase camera: target=(%.3f, %.3f, %.3f), distance=%.3f, yaw=%.3f deg, elevation=%.3f deg",
				cameraTarget.x, cameraTarget.y, cameraTarget.z, cameraDistance, cameraYawDegrees, cameraElevationDegrees);
		}

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
			wi::vector<std::uint8_t> readback;
			const std::size_t depthPlaneBytes = static_cast<std::size_t>(pixelCount) * sizeof(float);
			if (!wi::helper::saveTextureToMemory(depthTexture, readback) || readback.size() < depthPlaneBytes)
			{
				error = "Wicked Engine depth readback failed or returned a truncated depth plane";
				return false;
			}
			coveredPixels = 0;
			for (std::size_t index = 0; index < static_cast<std::size_t>(pixelCount); ++index)
			{
				float depth = 0.0f;
				std::memcpy(&depth, readback.data() + index * sizeof(float), sizeof(depth));
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

		std::filesystem::path outputPath;
		XMFLOAT3 cameraTarget{};
		float cameraDistance = 1.0f;
		float cameraYawDegrees = 45.0f;
		float cameraElevationDegrees = 24.5f;
		std::uint32_t warmupFramesRemaining = 120;
		std::uint32_t settleFramesRemaining = 0;
		bool captureFinished = false;
	};
}
