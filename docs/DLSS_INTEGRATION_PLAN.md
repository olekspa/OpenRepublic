# Streamline integration plan for Wicked Engine

**Status: opt-in DLSS SR runtime path implemented as patches 0001+0002 and compiled; Streamline initialization and DLSS output remain unverified.** Patch 0002 adds D3D12 interposer startup, native resource/command-list access, `RenderPath3D::setDLSSEnabled`, optimal render-size selection and per-frame DLSS calls. The short windowed smoke run opened Wicked's template but exited with an access violation during close. The official runtime binaries were unavailable in the test directory, so `slInit` success and DLSS evaluation were not observed. Frame Generation remains unimplemented.

## Scope and pinned inputs

- **VERIFIED:** OpenRepublic pins Wicked Engine gitlink `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.
- **VERIFIED:** the inspected source has `D3D12CreateDevice` lookup at `WickedEngine/wiGraphicsDevice_DX12.cpp:2278`, device attempts around `:2451`, `GraphicsDevice_DX12::CreateSwapChain` at `:3096`, and `CreateSwapChainForHwnd` at `:3201`.
- **VERIFIED:** `RenderPath3D::RenderPostprocessChain` invokes FSR2 at `WickedEngine/wiRenderPath3D.cpp:2350-2364`; jitter is assigned at `:424`; motion vectors are `rtVelocity`; the temporal depth copy is `depthBuffer_Copy`; tone-map exposure is currently a scalar setting plus optional luminance texture.
- **VERIFIED:** patch 0002 adds opt-in native D3D12 device, graphics command-list, resource and engine-tracked state accessors. The state accessor converts Wicked's current `TextureDesc.layout`; it is not a runtime query of the D3D12 driver's actual state.
- **INFERRED:** `rtVelocity` maps to Streamline motion vectors. Wicked's FSR2 setup uses motion-vector scale `(1,1)` and documents jitter as already baked into the velocity buffer; mapping these details to Streamline's `mvecScale` and `motionVectorsJittered` flags still needs image-quality validation.
- **UNKNOWN:** whether this patch/API shape remains compatible with future Wicked Engine revisions. Recheck every source location against the pinned revision when applying.

## Patch contents

`0001-streamline-sdk-build-boundary.patch` adds the OFF-by-default `OPENREPUBLIC_STREAMLINE_DIR` option plus `OPENREPUBLIC_STREAMLINE_SDK_ROOT`; it consumes SDK headers in place and copies no NVIDIA files. `0002-streamline-dlss-runtime.patch` is layered after 0001. It adds Windows/D3D12-only startup and render-path calls. Both remain inert in default builds.

Patch 0002 loads `sl.interposer.dll` by an absolute executable-directory path before Wicked loads DXGI/D3D12, calls `slInit` with `kFeatureDLSS`, uses interposer factory/device creation exports when present, calls `slSetD3DDevice`, and shuts down after `WaitForGPU`. It exposes native D3D12 handles and converts the tracked engine texture layout to `D3D12_RESOURCE_STATES`. `RenderPath3D::setDLSSEnabled(true)` requests Balanced dimensions through `slDLSSGetOptimalSettings`, updates render scale, allocates display-size output resources, and schedules DLSS at the existing FSR2 postprocess location. The per-frame path supplies a token, camera constants, depth/motion/color input/color output tags and `slEvaluateFeature(kFeatureDLSS)`; it uses auto exposure because no validated Streamline exposure texture mapping was established.

The render-path flag is not connected to OpenRepublic's application setting yet. The engine API compiled with the SDK headers, but no SDK runtime DLL was present for this run.

## Concrete integration sequence

1. **Acquire and pin SDK files outside Git.** Use the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) for source/guides and its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases) for runtime binaries. The inspected source/tag is Streamline `v2.14.16`, commit `625db07d4bca5abc67d781c42af7e7fb0761f1b6`. The release ZIP download did not complete in the test session; only the source checkout and headers were used for compilation.
2. **Initialize before DXGI/D3D12.** In an opt-in Windows build, the patch loads `sl.interposer.dll` from the executable directory and calls `slInit` before Wicked loads DXGI/D3D12. It requests D3D12, `kFeatureDLSS`, plugin directories (`plugins` then executable directory) and frame-based resource tagging. It does not call from `DllMain`. Missing DLLs or initialization failure leave the existing renderer path available.
3. **Bind device and check feature support.** After Wicked creates its device, the patch calls `slSetD3DDevice`. The per-frame DLSS function calls `slIsFeatureSupported` with the adapter LUID and returns to the non-DLSS path on failure. The patch attempts to use the interposer's `CreateDXGIFactory2` and `D3D12CreateDevice` exports; successful proxy/present behavior has not been verified.
4. **Submit a per-view frame.** The implementation obtains one `FrameToken` for a DLSS dispatch, sets camera matrices/jitter constants, tags a viewport's depth, velocity, HDR input/output color, then calls `slEvaluateFeature(kFeatureDLSS)` on Wicked's command list. It removes the projection jitter before providing matrices. Reversed depth is source-verified in Wicked's camera code. Motion-vector scale and jitter interpretation remain inferred from the FSR2 integration and need runtime validation.
5. **Use DLSS at the HDR postprocess point.** The patch currently selects Balanced mode, obtains optimal dimensions with `slDLSSGetOptimalSettings`, scales the render path, and creates display-sized output resources. It evaluates in the existing FSR2 slot, before tone mapping. It enables `useAutoExposure`; it does not tag an exposure resource because Wicked's scalar exposure and luminance texture have not been shown to meet the Streamline texture contract. Correct output state, resize and temporal history are not yet runtime-verified.
6. **Fail safely and close the module.** API failures return to the ordinary temporal/postprocess path; `slShutdown` is called after the graphics queue is drained. The patch does not yet validate NVIDIA DLL signatures, explicitly report all failure reasons to OpenRepublic, or handle device loss beyond normal fallback. Interposer present behavior and plugin resource lifetime need runtime testing.
7. **Keep Frame Generation out of scope.** DLSS-G, Reflex markers and HUD/UI composition are not implemented. The patch requests only DLSS SR.

## OpenRepublic setting mapping

`src/runtime/graphics_settings.*` already parses `runtime::Upscaler::Dlss` and reports it unavailable. Map that neutral setting to the Streamline backend only when the Windows/D3D12 build option is ON and post-device support validation succeeds:

| OpenRepublic choice | Streamline mapping | Current status |
| --- | --- | --- |
| `Upscaler::None` | Do not evaluate a Streamline feature | Existing path |
| `Upscaler::Fsr2Quality/Balanced/Performance/UltraPerformance` | Preserve existing FSR2 modes | Existing engine path |
| `Upscaler::Dlss` | Intended to select `RenderPath3D::setDLSSEnabled(true)`; patch currently uses Balanced mode and `slDLSSGetOptimalSettings` | Engine patch exists; OpenRepublic setting is not connected and still reports unavailable |
| `GraphicsSettings::sharpen` | Map only if Streamline DLSS options offer the corresponding control in the selected SDK version, otherwise keep OpenRepublic's post-sharpen pass | Requires version-specific review |
| Frame generation | Separate future setting; do not overload `Upscaler::Dlss` | Not implemented; lacks HUD-less/UI buffers and present contract |

The UI/runtime must expose actual availability and reason (SDK not built, DLL missing/invalid, unsupported adapter/driver, initialization failure), never silently label FSR2 as DLSS.

## Runtime payload required for an opted-in build

The patch expects `sl.interposer.dll` beside the executable. Plugin search paths are `<exe>\plugins` and `<exe>`; the actual release package layout must be checked before packaging. A production payload is expected to include the Streamline core dependencies and DLSS SR plugin (the selected release's `sl.common.dll`, `sl.dlss.dll` and NVIDIA NGX DLSS runtime files such as `nvngx_dlss.dll`, as applicable). **UNKNOWN:** the exact 2.14.16 runtime file set and redistribution rights were not verified because the official release archive download did not complete. Do not package from this list until the release files and licenses are checked. No NVIDIA binaries, headers or SDK archive are in Git.

## SDK license and redistribution notes

- **OBSERVED:** at the reviewed public repository commit, root `license.txt` is an MIT-style grant for the Streamline repository files, but expressly excludes `sl_nvperf.h` and `sl_nvperf.dll`, which use the separate Nsight Perf SDK license. Do not include those files in an OpenRepublic integration.
- **OBSERVED:** `external/ngx-sdk/license.txt` is the separate NVIDIA RTX SDK license. Its text permits distribution of SDK materials incorporated in object-code form into a qualifying application subject to requirements, while prohibiting stand-alone SDK redistribution and imposing notice/contract obligations. This is not the root MIT license.
- **UNKNOWN:** the final legal terms applicable to a specific selected release and its plugins/runtime payloads. Review all release-specific notices and the current NVIDIA terms at the time of packaging; this plan is not legal advice.
- **UNKNOWN:** whether every runtime file needed by the selected Streamline/DLSS version may be shipped by OpenRepublic under the applicable agreement. Determine exact payloads and notices from the selected release and license; do not assume `nvngx_dlss.dll` redistribution terms from the Streamline root license.
- **Policy:** fetch the SDK from NVIDIA's public GitHub repository or release page into a user/build-provided location. Do not commit headers, DLLs, archives, extracted SDK files, or caches. A build-time download should be opt-in, pinned by release and cryptographic digest, and must not imply redistributability. Runtime packaging is a separate explicit legal/release decision.

## Verification record and acceptance gates

- **VERIFIED:** patches 0001 and 0002 apply in sequence to a fresh copy at pinned revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.
- **VERIFIED (compile):** configured the patched copy with Visual Studio 2022, Release, `OPENREPUBLIC_STREAMLINE_DIR=ON`, and the private source SDK include tree at `D:/.CODEX_Projects/OpenRepublic-Research/scratch/streamline-sdk`. Built `WickedEngine_common`, `WickedEngine_ext_shaders` (`WickedEngine.lib`) and `Template_Windows.exe`. This compiles the SDK API call sites against v2.14.16 headers. No NVIDIA DLL or DLSS plugin was linked or present.
- **OBSERVED (windowed smoke):** ran the patched Windows template app in a visible window for about two seconds; its main window appeared. Closing it returned process exit `-1073741819` (`0xC0000005`, access violation). This establishes a short renderer startup attempt only; it is not a clean shutdown acceptance result. The template does not enable `RenderPath3D::setDLSSEnabled`.
- **VERIFIED (source):** Wicked camera projection is reverse-Z and `wiRenderer.cpp` sets FSR2 motion-vector scale to `(1,1)` and notes jitter cancellation is already baked into the velocity buffer. **INFERRED:** Streamline's `motionVectorsJittered=true` and same scale are correct for this resource; validate with runtime visuals.
- **NOT VERIFIED:** the official `v2.14.16` release archive did not finish downloading, so no runtime DLLs were available. `slInit` success, device support, interposer export behavior, `slSetD3DDevice`, `slSetTagForFrame`, `slEvaluateFeature`, DLSS output, image quality, performance, actual resource-state correctness, resize/device loss and a clean shutdown were not observed. The short app run therefore confirms **ran: yes**, **DLSS output observed: no**.
- **NOT VERIFIED:** no OpenRepublic application code was changed. `runtime::Upscaler::Dlss` is not yet wired to the new Wicked Engine flag.
- **Previous baseline evidence:** the earlier headless OpenRepublic Release CTest passed 36/36 before this runtime-patch follow-up; no headless source changed in this follow-up.

**Acceptance required before claiming working DLSS:** obtain the official runtime package and verify its release-specific file list/licensing; run a private instrumented scene on the RTX 5070 Ti; record `slInit`, feature support, per-frame tags/evaluate result and output dimensions; inspect a rendered capture; verify motion-vector semantics, resize, fallback and clean shutdown; wire the OpenRepublic setting and report availability. DLSS-G requires separate UI-composition and present-path work and is not covered by this patch.

## Sources

- NVIDIA [Streamline repository and license](https://github.com/NVIDIA-RTX/Streamline)
- NVIDIA [Streamline releases](https://github.com/NVIDIA-RTX/Streamline/releases)
- NVIDIA [ProgrammingGuide.md](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md)
- NVIDIA [DLSS programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS.md)
- NVIDIA [DLSS Frame Generation guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
- NVIDIA [Reflex guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideReflex.md)
