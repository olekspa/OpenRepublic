# Streamline integration plan for Wicked Engine

**Status: the opt-in path compiles and DLSS evaluates successfully in a private windowed OpenRepublic run on the RTX 5070 Ti.** Patch 0003 defers setup until physical dimensions are known and retries setup when output dimensions/resources differ. Using the official signed Streamline v2.14.16 runtime, `slInit`, `slSetD3DDevice`, optimal settings, constants, and `slEvaluateFeature(kFeatureDLSS)` returned success; the showcase capture contains the rendered scene. The FSR2 comparison run crashed before writing a capture. Dynamic motion quality, resize behavior, and GPU/frame time remain unverified. Frame Generation remains unimplemented.

## Scope and pinned inputs

- **VERIFIED:** OpenRepublic pins Wicked Engine gitlink `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.
- **VERIFIED:** the inspected source has `D3D12CreateDevice` lookup at `WickedEngine/wiGraphicsDevice_DX12.cpp:2278`, device attempts around `:2451`, `GraphicsDevice_DX12::CreateSwapChain` at `:3096`, and `CreateSwapChainForHwnd` at `:3201`.
- **VERIFIED:** `RenderPath3D::RenderPostprocessChain` invokes FSR2 at `WickedEngine/wiRenderPath3D.cpp:2350-2364`; jitter is assigned at `:424`; motion vectors are `rtVelocity`; the temporal depth copy is `depthBuffer_Copy`; tone-map exposure is currently a scalar setting plus optional luminance texture.
- **VERIFIED:** patch 0002 adds opt-in native D3D12 device, graphics command-list, resource and engine-tracked state accessors. The state accessor converts Wicked's current `TextureDesc.layout`; it is not a runtime query of the D3D12 driver's actual state.
- **INFERRED:** `rtVelocity` maps to Streamline motion vectors. Wicked's FSR2 setup uses motion-vector scale `(1,1)` and documents jitter as already baked into the velocity buffer; mapping these details to Streamline's `mvecScale` and `motionVectorsJittered` flags still needs image-quality validation.
- **UNKNOWN:** whether this patch/API shape remains compatible with future Wicked Engine revisions. Recheck every source location against the pinned revision when applying.

## Patch contents

`0001-streamline-sdk-build-boundary.patch` adds the OFF-by-default `OPENREPUBLIC_STREAMLINE_DIR` option plus `OPENREPUBLIC_STREAMLINE_SDK_ROOT`; it consumes SDK headers in place and copies no NVIDIA files. `0002-streamline-dlss-runtime.patch` is layered after 0001. `0003-streamline-deferred-resize.patch` is layered after 0002 and fixes lazy sizing/resource allocation. All remain inert in default builds.

Patch 0002 loads `sl.interposer.dll` by an absolute executable-directory path before Wicked loads DXGI/D3D12, calls `slInit` with `kFeatureDLSS`, uses interposer factory/device creation exports when present, calls `slSetD3DDevice`, and shuts down after `WaitForGPU`. It exposes native D3D12 handles and converts the tracked engine texture layout to `D3D12_RESOURCE_STATES`. `RenderPath3D::setDLSSEnabled(true)` requests Balanced dimensions through `slDLSSGetOptimalSettings`, updates render scale, allocates display-size output resources, and schedules DLSS at the existing FSR2 postprocess location. The per-frame path supplies a token, camera constants, depth/motion/color input/color output tags and `slEvaluateFeature(kFeatureDLSS)`; it uses auto exposure because no validated Streamline exposure texture mapping was established.

OpenRepublic's `runtime::Upscaler::Dlss` was connected to the render-path flag in the private app copy. The initial early settings call can occur at `0×0`; patch 0003 keeps that request pending and initializes after the first nonzero physical size. A later resize is detected by comparing DLSS output resource dimensions with the current physical dimensions and calling the same setup path. A resize event was not exercised in this run.

## Concrete integration sequence

1. **Acquire and pin SDK files outside Git.** Use the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) for source/guides and its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases) for runtime binaries. The inspected source/tag is Streamline `v2.14.16`, commit `625db07d4bca5abc67d781c42af7e7fb0761f1b6`. **VERIFIED:** downloaded the public v2.14.16 release ZIP privately (SHA-256 `32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b`); the four runtime DLLs used were Authenticode-valid and signed by NVIDIA. No SDK binaries were added to Git.
2. **Initialize before DXGI/D3D12.** In an opt-in Windows build, the patch loads `sl.interposer.dll` from the executable directory and calls `slInit` before Wicked loads DXGI/D3D12. It requests D3D12, `kFeatureDLSS`, plugin directories (`plugins` then executable directory) and frame-based resource tagging. It does not call from `DllMain`. Missing DLLs or initialization failure leave the existing renderer path available.
3. **Bind device and check feature support.** After Wicked creates its device, the patch calls `slSetD3DDevice`. The per-frame DLSS function calls `slIsFeatureSupported` with the adapter LUID and returns to the non-DLSS path on failure. The patch attempts to use the interposer's `CreateDXGIFactory2` and `D3D12CreateDevice` exports; successful proxy/present behavior has not been verified.
4. **Submit a per-view frame.** The implementation obtains one `FrameToken` for a DLSS dispatch, sets camera matrices/jitter constants, tags a viewport's depth, velocity, HDR input/output color, then calls `slEvaluateFeature(kFeatureDLSS)` on Wicked's command list. It removes the projection jitter before providing matrices. Reversed depth is source-verified in Wicked's camera code. Motion-vector scale and jitter interpretation remain inferred from the FSR2 integration and need runtime validation.
5. **Use DLSS at the HDR postprocess point.** The patch currently selects Balanced mode, obtains optimal dimensions with `slDLSSGetOptimalSettings`, scales the render path, and creates display-sized output resources. Patch 0003 defers this sequence at zero size and retries on size/resource mismatch. It evaluates in the existing FSR2 slot, before tone mapping. It enables `useAutoExposure`; it does not tag an exposure resource because Wicked's scalar exposure and luminance texture have not been shown to meet the Streamline texture contract. Runtime evaluation succeeded at 731×434 input and 1260×748 output. Resize behavior and temporal image correctness remain unverified.
6. **Fail safely and close the module.** API failures return to the ordinary temporal/postprocess path; `slShutdown` is called after the graphics queue is drained. The patch does not yet validate NVIDIA DLL signatures, explicitly report all failure reasons to OpenRepublic, or handle device loss beyond normal fallback. Interposer present behavior and plugin resource lifetime need runtime testing.
7. **Keep Frame Generation out of scope.** DLSS-G, Reflex markers and HUD/UI composition are not implemented. The patch requests only DLSS SR.

## OpenRepublic setting mapping

`src/runtime/graphics_settings.*` already parses `runtime::Upscaler::Dlss` and reports it unavailable. Map that neutral setting to the Streamline backend only when the Windows/D3D12 build option is ON and post-device support validation succeeds:

| OpenRepublic choice | Streamline mapping | Current status |
| --- | --- | --- |
| `Upscaler::None` | Do not evaluate a Streamline feature | Existing path |
| `Upscaler::Fsr2Quality/Balanced/Performance/UltraPerformance` | Preserve existing FSR2 modes | Existing engine path |
| `Upscaler::Dlss` | Selects `RenderPath3D::setDLSSEnabled(true)` in the private run copy; patch uses Balanced mode and `slDLSSGetOptimalSettings` | **OBSERVED:** setting wired; 0×0 request remains pending, then `slEvaluateFeature` succeeds at 731×434 → 1260×748 |
| `GraphicsSettings::sharpen` | Map only if Streamline DLSS options offer the corresponding control in the selected SDK version, otherwise keep OpenRepublic's post-sharpen pass | Requires version-specific review |
| Frame generation | Separate future setting; do not overload `Upscaler::Dlss` | Not implemented; lacks HUD-less/UI buffers and present contract |

The UI/runtime must expose actual availability and reason (SDK not built, DLL missing/invalid, unsupported adapter/driver, initialization failure), never silently label FSR2 as DLSS.

## Runtime payload required for an opted-in build

The patch expects `sl.interposer.dll` beside the executable. Plugin search paths are `<exe>\plugins` and `<exe>`; the test placed `sl.common.dll`, `sl.dlss.dll`, and `nvngx_dlss.dll` beside it. **OBSERVED:** the local runtime also loaded Streamline-managed DLSS model data from the machine's existing NVIDIA NGX cache; that cache is machine-managed and was neither copied nor modified. No NVIDIA binaries, headers or SDK archive are in Git. The complete release runtime distribution requirements remain subject to release-specific license review.

## SDK license and redistribution notes

- **OBSERVED:** at the reviewed public repository commit, root `license.txt` is an MIT-style grant for the Streamline repository files, but expressly excludes `sl_nvperf.h` and `sl_nvperf.dll`, which use the separate Nsight Perf SDK license. Do not include those files in an OpenRepublic integration.
- **OBSERVED:** `external/ngx-sdk/license.txt` is the separate NVIDIA RTX SDK license. Its text permits distribution of SDK materials incorporated in object-code form into a qualifying application subject to requirements, while prohibiting stand-alone SDK redistribution and imposing notice/contract obligations. This is not the root MIT license.
- **OBSERVED:** the downloaded v2.14.16 package includes the separate NGX SDK license text. It limits NGX SDK redistribution to qualifying object-code incorporation subject to its conditions and excludes stand-alone redistribution; review the full license and current terms before shipping. The runtime DLLs used in this test were NVIDIA signed. This is not a determination that this project has met every distribution condition.
- **UNKNOWN:** final legal terms applicable to a production package of the selected release and its plugins/runtime payloads. Review all release-specific notices and current NVIDIA terms at packaging time; this plan is not legal advice.
- **UNKNOWN:** whether every runtime file needed by the selected Streamline/DLSS version may be shipped by OpenRepublic under the applicable agreement. Determine exact payloads and notices from the selected release and license; do not assume `nvngx_dlss.dll` redistribution terms from the Streamline root license.
- **Policy:** fetch the SDK from NVIDIA's public GitHub repository or release page into a user/build-provided location. Do not commit headers, DLLs, archives, extracted SDK files, or caches. A build-time download should be opt-in, pinned by release and cryptographic digest, and must not imply redistributability. Runtime packaging is a separate explicit legal/release decision.

## Verification record and acceptance gates

- **VERIFIED:** patches 0001, 0002, and 0003 apply in sequence to a clean copy of the pinned revision. Patch 0003 also passed `git apply --check` after 0001+0002.
- **VERIFIED (compile):** configured the private OpenRepublic copy with Visual Studio 2022, Release, `OPENREPUBLIC_STREAMLINE_DIR=ON`, and the private SDK v2.14.16 headers. Built the full `OpenRepublic` client and Wicked Engine library after patch 0003.
- **OBSERVED (windowed run):** the private OpenRepublic app ran with `--graphics upscaler=dlss`, target map/building arguments, and showcase capture. Process exited `0`; captured `dlss-map-building61-success.png` (2,505,936 bytes). The static image shows the map scene without gross corruption. This does not establish temporal stability or equivalence to FSR2.
- **OBSERVED (runtime):** `slInit result=0 initialized=true`; `slSetD3DDevice result=0`; NGX reported `FeatureInitResult = NvNGXFeatureInitSuccess` and application ID `100721531`. `slDLSSGetOptimalSettings result=0` reported render 731×434 for display 1260×748. `slSetConstants result=0` and repeated `slEvaluateFeature(kFeatureDLSS) result=0` records were observed; shutdown returned `0`.
- **OBSERVED (fixes):** before patch 0003, optimal settings failed at `0×0` (result 25). Deferral fixed that ordering. A later dimension check caught that the earlier scale calculation truncated one input dimension to 433 where optimal settings required 434; using the larger axis ratio yields 731×434. A DLSS dispatch also initially lacked a valid `rtVelocity`; patch 0003 includes DLSS in the velocity-target allocation condition. The final run evaluated successfully with that target allocated.
- **VERIFIED (source), INFERRED (mapping):** Wicked `wiRenderer.cpp` sets FSR2 motion-vector scale to `(1,1)` and comments that jitter cancellation is baked into its velocity buffer. NVIDIA's [DLSS programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS.md) describes `(1,1)` for normalized `[-1,1]` motion vectors and inverse-render-dimension scaling for pixel-space vectors. Matching FSR2's scale and `motionVectorsJittered=true` remains an inference; no camera-motion/ghosting acceptance test was completed.
- **OBSERVED (comparison attempt):** the FSR2 showcase attempt exited `-1073741819` (`0xC0000005`) and produced no FSR2 PNG. Therefore no DLSS-vs-FSR2 image comparison is available. The failure cause was not diagnosed in this task.
- **OBSERVED (timing):** the instrumentation measures CPU duration around `slEvaluateFeature`, not frame or GPU time. The first call was about 32.86 ms; later sampled calls were about 0.16–0.23 ms. At dispatch count 2,820 the rolling/aggregate CPU call mean was approximately 0.237 ms. This is not an end-to-end frame-time measurement.
- **UNKNOWN:** whether temporal output is free of ghosting, exact velocity normalization/jitter semantics, visual parity against FSR2, GPU/frame time, runtime resize behavior, and device-loss handling. Runtime diagnostics and captures remain in the private research folder and are not committed.
- **VERIFIED (source fix):** zero-size settings now retain `dlssRequested`; `Update`/postprocess retry after physical dimensions become nonzero. `ResizeBuffers` replays requested/enabled state, and output-resource mismatch triggers reconfiguration. No actual resize interaction was performed.
- **Previous baseline evidence:** the earlier headless OpenRepublic Release CTest passed 36/36 before this runtime-patch follow-up; no headless source changed in this follow-up.

| Acceptance item | Result |
| --- | --- |
| Client and Wicked Engine compiled | **YES** (private VS2022 Release build) |
| `slInit` succeeded | **YES** (`0`) |
| DLSS evaluated | **YES** (`slEvaluateFeature` result `0`, input 731×434, output 1260×748) |
| DLSS image correct, including velocity units | **UNKNOWN** (static scene capture exists; no FSR2 comparison or motion/ghosting test) |
| Frame time measured | **NO** (CPU dispatch-call time observed; no GPU or end-to-end frame-time measurement) |
| App run and capture | **YES**, windowed OpenRepublic run exited `0`; DLSS capture written |
| FSR2 comparison capture | **NO** (attempt exited `0xC0000005`; no PNG) |
| Resize retry verified | **NO** (implemented; not exercised) |

**Acceptance still required:** produce a successful FSR2 capture for side-by-side comparison; test moving-camera/object temporal behavior and validate velocity semantics; measure GPU and end-to-end frame time; exercise resize, fallback and clean shutdown. DLSS-G requires separate UI-composition and present-path work and is not covered by this patch.

## Sources

- NVIDIA [Streamline repository and license](https://github.com/NVIDIA-RTX/Streamline)
- NVIDIA [Streamline releases](https://github.com/NVIDIA-RTX/Streamline/releases)
- NVIDIA [ProgrammingGuide.md](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md)
- NVIDIA [DLSS programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS.md)
- NVIDIA [DLSS Frame Generation guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
- NVIDIA [Reflex guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideReflex.md)
