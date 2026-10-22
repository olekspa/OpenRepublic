# Wicked Engine Streamline patch set

Base submodule revision: `4f93f3fd4f337aeb38dbfe410701b83024ccdd42`.

`0001-streamline-sdk-build-boundary.patch` adds an OFF-by-default CMake option and validates the user-supplied `OPENREPUBLIC_STREAMLINE_DIR` before exposing the SDK include directory/compile definition to the WickedEngine target. It does not copy SDK files or add calls into engine runtime. See [the integration plan](../../../docs/DLSS_INTEGRATION_PLAN.md) for the concrete SR/FG implementation sequence, resource contract, license findings and remaining acceptance gates.

Apply from a clean checkout of the pinned submodule root:

```powershell
git apply --check path\to\0001-streamline-sdk-build-boundary.patch
git apply path\to\0001-streamline-sdk-build-boundary.patch
```

Default configuration remains `OPENREPUBLIC_STREAMLINE_DIR=OFF`. When enabled for SDK header development, configure the Wicked Engine project with `-DOPENREPUBLIC_STREAMLINE_DIR=ON` and `-DOPENREPUBLIC_STREAMLINE_SDK_ROOT=<local NVIDIA SDK directory>`. This option currently exposes headers only; it does not enable DLSS at runtime. Never commit SDK files or build/download them into a public artifact without a release-specific license and packaging review.
