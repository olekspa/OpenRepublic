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
```

Patch 0001 adds the OFF-by-default `OPENREPUBLIC_STREAMLINE_DIR` CMake option and consumes headers from the local `OPENREPUBLIC_STREAMLINE_SDK_ROOT`. Patch 0002 adds an opt-in Windows/D3D12 DLSS Super Resolution path, native device/resource/command-list accessors, interposer initialization, per-frame tags and evaluation, and Balanced-mode optimal sizing. Patch 0003 keeps a DLSS request pending until physical output dimensions are nonzero, retries setup when dimensions/resources change, includes the motion-vector target in allocation, and avoids a one-pixel undersize caused by internal-size truncation. Patch 0004 selects DLSS Quality mode instead of Balanced; optimal dimensions still come from `slDLSSGetOptimalSettings`. Patch 0005 preserves the shared FSR output targets when DLSS is disabled but FSR2 is active; a patched FSR2 build completed after correction. Patch 0006 chooses the limiting axis ratio for DLSS render scale, allowing exact 2560x1421 input at a 3840x2132 output. Patch 0007 adds `OPENREPUBLIC_DLSS_PRESET=performance` for `eMaxPerformance`; unset or other values retain Quality. In the private test, both presets evaluated successfully. Matched-output FPS was 60.11 native, 77.24 FSR2 Quality, 65.64 DLSS Quality and 79.19 DLSS Performance. Single-capture RGB PSNR/SSIM against native favored FSR2 Quality (23.81 dB/0.820 whole-frame; 20.49 dB/0.728 detail), then DLSS Quality (22.39/0.739; 18.72/0.570), then DLSS Performance (21.52/0.684; 18.11/0.511). These results apply to one scene and machine. Temporal correctness and GPU-only frame time remain unverified. The patch set does not implement DLSS Frame Generation, Reflex markers, or a production-ready DLL deployment flow. OpenRepublic application settings wiring exists in the application repository, not these Wicked Engine patches. See [the integration plan](../../../docs/DLSS_INTEGRATION_PLAN.md) for its contract, validation evidence, license findings and remaining acceptance gates.

For an opted-in build, obtain Streamline source/SDK from the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) or a versioned package from its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases), and keep those files outside Git. Configure with `-DOPENREPUBLIC_STREAMLINE_DIR=ON -DOPENREPUBLIC_STREAMLINE_SDK_ROOT=<local NVIDIA SDK directory>`. Default builds remain unchanged. Do not commit SDK files or package runtime binaries until the exact release payload and redistribution terms have been reviewed.
