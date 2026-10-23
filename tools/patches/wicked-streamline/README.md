# Wicked Engine Streamline patch set

Base submodule revision: `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.

Apply the patches in order to a clean checkout of that pinned revision:

```powershell
git apply --check path\to\0001-streamline-sdk-build-boundary.patch
git apply path\to\0001-streamline-sdk-build-boundary.patch
git apply --check path\to\0002-streamline-dlss-runtime.patch
git apply path\to\0002-streamline-dlss-runtime.patch
git apply --check path\to\0003-streamline-deferred-resize.patch
git apply path\to\0003-streamline-deferred-resize.patch
git apply --check path\to\0004-dlss-quality-preset.patch
git apply path\to\0004-dlss-quality-preset.patch
git apply --check path\to\0005-preserve-fsr2-targets.patch
git apply path\to\0005-preserve-fsr2-targets.patch
git apply --check path\to\0006-exact-dlss-quality-scale.patch
git apply path\to\0006-exact-dlss-quality-scale.patch
git apply --check path\to\0007-dlss-performance-preset.patch
git apply path\to\0007-dlss-performance-preset.patch
git apply --check path\to\0008-streamline-temporal-input-contract.patch
git apply path\to\0008-streamline-temporal-input-contract.patch
git apply --check path\to\0009-dlssg-capability-probe.patch
git apply path\to\0009-dlssg-capability-probe.patch
```

Patch 0001 adds the OFF-by-default `OPENREPUBLIC_STREAMLINE_DIR` CMake option and consumes headers from the local `OPENREPUBLIC_STREAMLINE_SDK_ROOT`. Patch 0002 adds an opt-in Windows/D3D12 DLSS Super Resolution path, native device/resource/command-list accessors, interposer initialization, per-frame tags and evaluation, and Balanced-mode optimal sizing. Patch 0003 keeps a DLSS request pending until physical output dimensions are nonzero, retries setup when dimensions/resources change, includes the motion-vector target in allocation, and avoids a one-pixel undersize caused by internal-size truncation. Patch 0004 selects DLSS Quality mode instead of Balanced; optimal dimensions still come from `slDLSSGetOptimalSettings`. Patch 0005 preserves the shared FSR output targets when DLSS is disabled but FSR2 is active. Patch 0006 chooses the limiting axis ratio for DLSS render scale; patch 0007 adds `OPENREPUBLIC_DLSS_PRESET=performance`. Patch 0008 corrects projection jitter removal (`_31/_32`), marks Wicked's velocity input as unjittered, flips its Y scale for Streamline's Direct3D convention, and uses SDK optimal sharpness.

With patches 0001–0008, the private app rebuilt and logged successful DLSS Quality and Performance evaluations; FSR2 Quality also completed. Follow-up 9's 3840×2132 single-frame PSNR/SSIM against a newly captured native reference was FSR2 Quality 23.83 dB/0.820 whole and 20.52/0.730 detail; DLSS Quality 24.09/0.829 and 20.69/0.744; DLSS Performance 23.83/0.800 and 20.41/0.705. App FPS samples in that round were 101.45 native, 70.41 FSR2 Quality, 69.60 DLSS Quality and 75.48 DLSS Performance; no GPU utilization was sampled and focus was not confirmed for the full interval, so no performance conclusion is claimed. A short camera-motion screen showed no obvious persistent smear, but temporal correctness remains unknown. DLSS Frame Generation, Reflex markers, GPU-only frame timing, and production DLL deployment are not implemented. OpenRepublic application settings wiring exists in the application repository, not these Wicked Engine patches. See [the integration plan](../../../docs/DLSS_INTEGRATION_PLAN.md) for the investigation, validation evidence, license findings and remaining acceptance gates.

For an opted-in build, obtain Streamline source/SDK from the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) or a versioned package from its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases), and keep those files outside Git. Configure with `-DOPENREPUBLIC_STREAMLINE_DIR=ON -DOPENREPUBLIC_STREAMLINE_SDK_ROOT=<local NVIDIA SDK directory>`. Default builds remain unchanged. Do not commit SDK files or package runtime binaries until the exact release payload and redistribution terms have been reviewed.

Patch 0009 is a **capability probe only**, not frame-generation activation. After patches 0001–0008, apply it and set `OPENREPUBLIC_DLSS_FRAME_GENERATION=probe` in a private test run to load DLSS-G, Reflex and PCL, query DLSS-G state before present, and explicitly request `eOff`. The private RTX 5070 Ti / Streamline 2.14.16 run returned feature/state `eOk`, max 5 generated frames and dynamic MFG supported, but no generated frame was active. It logged a presenting-thread synchronization warning. UI-color/alpha and HUD-less tagging, PCL timing markers, Reflex sleep, verified `presentCommon()` routing and an `eOn` run remain unimplemented. Follow-up 10 FPS lines were not captured; see the integration plan's status table for the precise evidence and gaps.
