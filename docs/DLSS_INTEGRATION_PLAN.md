# Streamline integration plan for Wicked Engine

**Status: integration design and opt-in SDK build boundary prepared; runtime integration is not implemented.** This document is deliberately an implementation plan rather than a claim of working DLSS. The patch is an SDK discovery/build boundary only; it does not call Streamline, alter device creation, evaluate DLSS, or enable Frame Generation.

## Scope and pinned inputs

- **VERIFIED:** OpenRepublic pins Wicked Engine gitlink `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.
- **VERIFIED:** the inspected source has `D3D12CreateDevice` lookup at `WickedEngine/wiGraphicsDevice_DX12.cpp:2278`, device attempts around `:2451`, `GraphicsDevice_DX12::CreateSwapChain` at `:3096`, and `CreateSwapChainForHwnd` at `:3201`.
- **VERIFIED:** `RenderPath3D::RenderPostprocessChain` invokes FSR2 at `WickedEngine/wiRenderPath3D.cpp:2350-2364`; jitter is assigned at `:424`; motion vectors are `rtVelocity`; the temporal depth copy is `depthBuffer_Copy`; tone-map exposure is currently a scalar setting plus optional luminance texture.
- **INFERRED:** these engine resources are candidate inputs for DLSS SR, but the current source does not expose the native D3D12 resource/state and Streamline viewport/constants contract needed to submit valid tags safely. FSR2's wrapper owns its own dispatch and is not a drop-in Streamline adapter.
- **UNKNOWN:** whether this patch/API shape remains compatible with future Wicked Engine revisions. Recheck every source location against the pinned revision when applying.

## Patch contents

`tools/patches/wicked-streamline/0001-streamline-sdk-build-boundary.patch` adds `OPENREPUBLIC_STREAMLINE_DIR` (an OFF-by-default option) and `OPENREPUBLIC_STREAMLINE_SDK_ROOT`. When opted in on Windows, CMake requires `${OPENREPUBLIC_STREAMLINE_SDK_ROOT}/include/sl.h`, adds that SDK include directory to the WickedEngine common target, and publishes a compile definition. It does not copy SDK contents, link an SDK binary, alter default builds, or implement runtime features. The check rejects non-Windows configurations when enabled.

This is a safe prerequisite, not the completed renderer feature. It is intentionally possible to verify with default builds without possessing NVIDIA files. A later patch should add implementation only after native resource handles, resource states, frame token/viewport ownership, and output resolve semantics are established.

## Concrete integration sequence

1. **Acquire and pin the SDK outside Git.** Use the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) for source, guides and its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases) for a versioned SDK package. Record the selected release/tag and commit in the build receipt. The private research clone used for this review is commit `625db07d4bca5abc67d781c42af7e7fb0761f1b6` in a private scratch folder; no SDK files are in this repository.
2. **Initialize before DXGI/D3D12.** The application must call `slInit` on the main initialization path before Wicked loads/uses DXGI or D3D12, with D3D12 render API, requested features (`kFeatureDLSS`, optional Reflex and DLSS-G), plugin search path, logging and frame-based resource tagging preferences. Do not call from `DllMain`. The current engine dynamically loads D3D12/DXGI in device initialization; calling from `GraphicsDevice_DX12` after this point would be too late. Make this an explicit application startup dependency and fail closed to the existing renderer if opt-in initialization fails.
3. **Bind device and validate support.** Adapter enumeration must check `slGetFeatureRequirements` / `slIsFeatureSupported`; after the final D3D12 device is created, call `slSetD3DDevice`. A proxy/interposer factory/device route is an alternative, but needs to be designed and verified against Wicked's manual DLL loading. Do not assume `slInit` alone makes features available.
4. **Define a per-view frame contract.** Allocate a single `FrameToken` via `slGetNewFrameToken` for each rendered frame and use the same token and viewport across `slSetConstants`, `slSetTagForFrame`, `slDLSSSetOptions`, and `slEvaluateFeature`. Supply unjittered view/projection data and the jitter offset in output-pixel units; reconcile matrix layout, reversed depth and motion-vector sign/scale against Wicked's camera and velocity shaders. The engine's camera jitter and `rtVelocity` are candidates, not yet validated Streamline semantics.
5. **Run DLSS SR at the correct HDR point.** Select output dimensions and `DLSSMode` from the runtime setting; obtain render dimensions from `slDLSSGetOptimalSettings` and resize/recreate temporal resources on changes. Tag render-resolution HDR input color, output-resolution color, depth, motion vectors and a valid exposure resource (or explicitly enable auto exposure). Evaluate DLSS where FSR2 currently runs, before later HDR effects/tone mapping, then restore Wicked's expected resource states and continue with the upscaled output. Include barriers and lifetime tags for this command list. This must not call both FSR2 and DLSS for the same frame.
6. **Fail safely and maintain lifecycle.** Check every Streamline result; on feature/device/resource failure revert the active mode to the existing supported path and report `dlss` unavailable. Ensure the common plugin's `presentCommon()` runs exactly once per presented frame (manual-hook integrations need to provide it), handle resize/device loss, and call `slShutdown` after renderer teardown. Protect DLL loading with a full path and NVIDIA signature validation as described in the SDK guide.
7. **Keep Frame Generation a later gate.** DLSS-G needs present integration, correct timing/markers and valid HUD-less plus UI color/alpha buffers. Current Wicked/OpenRepublic rendering does not expose a separated HUD-less buffer at the necessary point. Do not enable DLSS-G by just adding `kFeatureDLSS_G`; first add and verify the composition buffers, swapchain/present ordering, resize behavior, pacing and UI treatment. Reflex may be integrated independently after placing markers and sleep correctly.

## OpenRepublic setting mapping

`src/runtime/graphics_settings.*` already parses `runtime::Upscaler::Dlss` and reports it unavailable. Map that neutral setting to the Streamline backend only when the Windows/D3D12 build option is ON and post-device support validation succeeds:

| OpenRepublic choice | Streamline mapping | Current status |
| --- | --- | --- |
| `Upscaler::None` | Do not evaluate a Streamline feature | Existing path |
| `Upscaler::Fsr2Quality/Balanced/Performance/UltraPerformance` | Preserve existing FSR2 modes | Existing engine path |
| `Upscaler::Dlss` | `sl::DLSSOptions.mode`; output width/height from active swapchain; render width/height from `slDLSSGetOptimalSettings` | Reserved/unavailable today; no runtime mapping implemented |
| `GraphicsSettings::sharpen` | Map only if Streamline DLSS options offer the corresponding control in the selected SDK version, otherwise keep OpenRepublic's post-sharpen pass | Requires version-specific review |
| Frame generation | Separate future setting; do not overload `Upscaler::Dlss` | Not implemented; lacks HUD-less/UI buffers and present contract |

The UI/runtime must expose actual availability and reason (SDK not built, DLL missing/invalid, unsupported adapter/driver, initialization failure), never silently label FSR2 as DLSS.

## SDK license and redistribution notes

- **OBSERVED:** at the reviewed public repository commit, root `license.txt` is an MIT-style grant for the Streamline repository files, but expressly excludes `sl_nvperf.h` and `sl_nvperf.dll`, which use the separate Nsight Perf SDK license. Do not include those files in an OpenRepublic integration.
- **OBSERVED:** `external/ngx-sdk/license.txt` is the separate NVIDIA RTX SDK license. Its text permits distribution of SDK materials incorporated in object-code form into a qualifying application subject to requirements, while prohibiting stand-alone SDK redistribution and imposing notice/contract obligations. This is not the root MIT license.
- **UNKNOWN:** the final legal terms applicable to a specific selected release and its plugins/runtime payloads. Review all release-specific notices and the current NVIDIA terms at the time of packaging; this plan is not legal advice.
- **UNKNOWN:** whether every runtime file needed by the selected Streamline/DLSS version may be shipped by OpenRepublic under the applicable agreement. Determine exact payloads and notices from the selected release and license; do not assume `nvngx_dlss.dll` redistribution terms from the Streamline root license.
- **Policy:** fetch the SDK from NVIDIA's public GitHub repository or release page into a user/build-provided location. Do not commit headers, DLLs, archives, extracted SDK files, or caches. A build-time download should be opt-in, pinned by release and cryptographic digest, and must not imply redistributability. Runtime packaging is a separate explicit legal/release decision.

## Verification record and acceptance gates

- **VERIFIED:** the pinned engine source was read at the locations above; the private Streamline clone contains `ProgrammingGuide.md`, `ProgrammingGuideDLSS.md`, `ProgrammingGuideDLSS_G.md`, and `ProgrammingGuideReflex.md`.
- **OBSERVED:** the guide requires early `slInit`, post-create `slSetD3DDevice`, frame tokens/constants/resource tags, a matching viewport/token at evaluation, and `presentCommon()` once per frame for manual-hook resource collection. DLSS-G guide calls for a HUD-less buffer before UI is drawn.
- **VERIFIED (build only):** applied the patch to a clean copy at the pinned revision, configured it with `OPENREPUBLIC_STREAMLINE_DIR=ON` and the private SDK root, and built the `WickedEngine_ext_shaders` target in Release. This validates CMake SDK discovery and compilation of the unchanged engine library with the option enabled; it does not compile Streamline API calls because this patch adds none. OpenRepublic headless Release CTest passed 36/36.
- **UNKNOWN / NOT VERIFIED:** no Streamline API is called; no DLSS output or DLSS-G frame has been observed; no device support, temporal quality, performance, signature validation, DLL deployment, resize or device-loss behavior has been tested.
- **Acceptance required before claiming integration:** apply patch to an exact `4f93f3fd4f337aeb38dbfe410701b83024ccdd42` copy; build with option OFF and ON; add native-resource/state validation; run a synthetic scene on RTX hardware; capture Streamline logs and verify selected feature, input/output dimensions, temporal history, resize, FSR2 fallback and clean shutdown. DLSS-G has its own UI-composition and present-path acceptance.

## Sources

- NVIDIA [Streamline repository and license](https://github.com/NVIDIA-RTX/Streamline)
- NVIDIA [Streamline releases](https://github.com/NVIDIA-RTX/Streamline/releases)
- NVIDIA [ProgrammingGuide.md](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md)
- NVIDIA [DLSS programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS.md)
- NVIDIA [DLSS Frame Generation guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
- NVIDIA [Reflex guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideReflex.md)
