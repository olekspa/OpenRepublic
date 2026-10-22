# Wicked Engine Streamline patch set

Base submodule revision: `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.

Apply the patches in order to a clean checkout of that pinned revision:

```powershell
git apply --check path\to\0001-streamline-sdk-build-boundary.patch
git apply path\to\0001-streamline-sdk-build-boundary.patch
git apply --check path\to\0002-streamline-dlss-runtime.patch
git apply path\to\0002-streamline-dlss-runtime.patch
```

Patch 0001 adds the OFF-by-default `OPENREPUBLIC_STREAMLINE_DIR` CMake option and consumes headers from the local `OPENREPUBLIC_STREAMLINE_SDK_ROOT`. Patch 0002 adds an opt-in Windows/D3D12 DLSS Super Resolution path, native device/resource/command-list accessors, interposer initialization, per-frame tags and evaluation, and Balanced-mode optimal sizing. It does not implement DLSS Frame Generation, Reflex markers, or a production-ready DLL deployment flow. OpenRepublic application settings wiring exists in the application repository, not these Wicked Engine patches. See [the integration plan](../../../docs/DLSS_INTEGRATION_PLAN.md) for its contract, validation evidence, license findings and remaining acceptance gates.

For an opted-in build, obtain Streamline source/SDK from the public [NVIDIA-RTX/Streamline repository](https://github.com/NVIDIA-RTX/Streamline) or a versioned package from its [releases page](https://github.com/NVIDIA-RTX/Streamline/releases), and keep those files outside Git. Configure with `-DOPENREPUBLIC_STREAMLINE_DIR=ON -DOPENREPUBLIC_STREAMLINE_SDK_ROOT=<local NVIDIA SDK directory>`. Default builds remain unchanged. Do not commit SDK files or package runtime binaries until the exact release payload and redistribution terms have been reviewed.
