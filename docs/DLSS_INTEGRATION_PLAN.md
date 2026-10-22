# Streamline integration plan for Wicked Engine

**Status: opt-in runtime code compiles and Streamline/NGX initializes on the RTX 5070 Ti, but DLSS dispatch has not succeeded.** A private OpenRepublic build and windowed capture run used the official signed v2.14.16 runtime. NGX accepted a locally supplied project identity, but the app applies graphics settings while the render path still has a `0×0` display size. `slDLSSGetOptimalSettings` returned result `25` for that size and the renderer disabled DLSS before token/tag/evaluate calls. Frame Generation remains unimplemented.

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

OpenRepublic can connect `runtime::Upscaler::Dlss` to the render-path flag in a private app build. The runtime integration was exercised with SDK binaries, but zero-sized early settings application currently disables DLSS before the scene has a display size; see the status table below.

## Concrete integration sequence

1. **Acquire and pin SDK files outside Git.** Use the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) for source/guides and its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases) for runtime binaries. The inspected source/tag is Streamline `v2.14.16`, commit `625db07d4bca5abc67d781c42af7e7fb0761f1b6`. **VERIFIED:** downloaded the public v2.14.16 release ZIP privately (SHA-256 `32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b`); the four runtime DLLs used were Authenticode-valid and signed by NVIDIA. No SDK binaries were added to Git.
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
| `Upscaler::Dlss` | Selects `RenderPath3D::setDLSSEnabled(true)` in the private run copy; patch currently uses Balanced mode and `slDLSSGetOptimalSettings` | **OBSERVED:** setting is wired in private copy; rejected at zero display size before dispatch |
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

- **VERIFIED:** patches 0001 and 0002 apply in sequence to a fresh copy at pinned revision `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.
- **VERIFIED (compile):** configured the private OpenRepublic copy with Visual Studio 2022, Release, `OPENREPUBLIC_STREAMLINE_DIR=ON`, and the private source SDK include tree. Built the full `OpenRepublic` client and Wicked Engine library against v2.14.16 headers.
- **OBSERVED (windowed smoke):** ran the patched Windows template app in a visible window for about two seconds; its main window appeared. Closing it returned process exit `-1073741819` (`0xC0000005`, access violation). This establishes a short renderer startup attempt only; it is not a clean shutdown acceptance result. The template does not enable `RenderPath3D::setDLSSEnabled`.
- **VERIFIED (source):** Wicked camera projection is reverse-Z and `wiRenderer.cpp` sets FSR2 motion-vector scale to `(1,1)` and notes jitter cancellation is already baked into the velocity buffer. **INFERRED:** Streamline's `motionVectorsJittered=true` and same scale are correct for this resource; validate with runtime visuals.
- **OBSERVED (runtime):** in the private OpenRepublic run, `slInit result=0`, `slSetD3DDevice result=0`, NGX reported `FeatureInitResult = NvNGXFeatureInitSuccess` and assigned application ID `100721531`. These are initialization results, not proof of DLSS output.
- **OBSERVED (failure):** `slGetFeatureFunction(slDLSSGetOptimalSettings)` returned `0`; the function pointer was non-null. But the render path's physical output dimensions were `0×0`; `slDLSSGetOptimalSettings` returned result `25`, with `0×0` optimal size. `setDLSSEnabled` then rejected DLSS. The frame token, tags and `slEvaluateFeature` were not reached.
- **OBSERVED (run/capture):** the full OpenRepublic app ran windowed, exited with code `0`, and wrote a private showcase PNG. This was a fallback-rendered capture, not DLSS output. Capture paths and full logs are retained in private research notes, outside Git.
- **UNKNOWN:** image correctness, motion-vector units/jitter semantics under DLSS, GPU frame time, successful dispatch, resize/device loss and clean shutdown behavior. An instrumented CPU call-time sample was not obtained because dispatch never occurred.
- **VERIFIED (source correction needed):** graphics settings are applied before the render path has non-zero physical dimensions. The current setter treats the zero-sized settings request as failure; it must defer/retry setup after resize before DLSS can be evaluated.
- **Previous baseline evidence:** the earlier headless OpenRepublic Release CTest passed 36/36 before this runtime-patch follow-up; no headless source changed in this follow-up.

| Acceptance item | Result |
| --- | --- |
| Client and Wicked Engine compiled | **YES** (private VS2022 Release build) |
| `slInit` succeeded | **YES** (`0`) |
| DLSS evaluated | **NO** (zero-size optimal-settings failure before token/tags/evaluation) |
| DLSS image correct, including velocity units | **NO / UNKNOWN** (no DLSS output observed) |
| Frame time measured | **NO** (no DLSS dispatch; no GPU frame-time instrumentation) |
| App run and capture | **YES**, fallback-rendered PNG only |

**Acceptance still required:** defer DLSS setup until after non-zero display dimensions, rebuild and rerun; record per-frame tags/evaluate result and output dimensions; inspect the DLSS capture against FSR2; verify motion-vector semantics, resize, fallback and clean shutdown. DLSS-G requires separate UI-composition and present-path work and is not covered by this patch.

## Sources

- NVIDIA [Streamline repository and license](https://github.com/NVIDIA-RTX/Streamline)
- NVIDIA [Streamline releases](https://github.com/NVIDIA-RTX/Streamline/releases)
- NVIDIA [ProgrammingGuide.md](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md)
- NVIDIA [DLSS programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS.md)
- NVIDIA [DLSS Frame Generation guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md)
- NVIDIA [Reflex guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideReflex.md)
