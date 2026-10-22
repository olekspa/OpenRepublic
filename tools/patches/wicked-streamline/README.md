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
```

Patch 0001 adds the OFF-by-default `OPENREPUBLIC_STREAMLINE_DIR` CMake option and consumes headers from the local `OPENREPUBLIC_STREAMLINE_SDK_ROOT`. Patch 0002 adds an opt-in Windows/D3D12 DLSS Super Resolution path, native device/resource/command-list accessors, interposer initialization, per-frame tags and evaluation, and Balanced-mode optimal sizing. Patch 0003 keeps a DLSS request pending until physical output dimensions are nonzero, retries setup when dimensions/resources change, includes the motion-vector target in allocation, and avoids a one-pixel undersize caused by internal-size truncation. Patch 0004 selects DLSS Quality mode instead of Balanced; optimal dimensions still come from `slDLSSGetOptimalSettings`. Patch 0005 preserves the shared FSR output targets when DLSS is disabled but FSR2 is active; the source guard was implicated in the private patched build's FSR2 crash and a patched FSR2 build completed after correction. Patch 0006 chooses the limiting axis ratio for the DLSS render scale: in the measured 3840x2132 window it avoids an input allocation one pixel larger than Streamline's optimal 2560x1421, and the runtime then logged repeated successful DLSS evaluations. Private native/FSR2/DLSS runs at that output measured 60.11/77.24/65.64 FPS, respectively, with GPU utilization near saturation. The result is specific to this scene and system. High-resolution image quality, temporal correctness, and GPU-only frame time remain unverified. It does not implement DLSS Frame Generation, Reflex markers, or a production-ready DLL deployment flow. OpenRepublic application settings wiring exists in the application repository, not these Wicked Engine patches. See [the integration plan](../../../docs/DLSS_INTEGRATION_PLAN.md) for its contract, validation evidence, license findings and remaining acceptance gates.

For an opted-in build, obtain Streamline source/SDK from the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) or a versioned package from its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases), and keep those files outside Git. Configure with `-DOPENREPUBLIC_STREAMLINE_DIR=ON -DOPENREPUBLIC_STREAMLINE_SDK_ROOT=<local NVIDIA SDK directory>`. Default builds remain unchanged. Do not commit SDK files or package runtime binaries until the exact release payload and redistribution terms have been reviewed.
