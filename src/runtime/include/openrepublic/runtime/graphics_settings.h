#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::runtime {

// Renderer-independent description of the quality features OpenRepublic can
// request. The same structure is parsed from the command line today and is meant
// to be exposed through the agent interface, so an LLM can change graphics
// without touching renderer code. Applying it is the renderer's job; features the
// device or backend lacks are reported, never silently ignored.
enum class Upscaler {
    None,
    Fsr2Quality,
    Fsr2Balanced,
    Fsr2Performance,
    Fsr2UltraPerformance,
    // Reserved: NVIDIA DLSS needs the NVIDIA Streamline SDK, which is not integrated yet.
    // Requesting it parses successfully and is reported as unavailable at apply time.
    Dlss,
};

enum class AmbientOcclusion { Off, Ssao, Hbao, Msao, RayTraced };

struct GraphicsSettings {
    Upscaler upscaler = Upscaler::None;
    AmbientOcclusion ambientOcclusion = AmbientOcclusion::Off;
    bool rayTracedReflections = false;
    bool rayTracedDiffuse = false;
    bool screenSpaceReflections = false;
    bool fxaa = false;
    bool bloom = false;
    float exposure = 1.0f;       // 0.05 .. 8
    float sharpen = -1.0f;       // negative leaves the renderer default; 0 .. 2 otherwise
};

// Parses `key=value` pairs separated by ';' or ',' (keys: upscaler, ao, rt_reflections,
// rt_diffuse, ssr, fxaa, bloom, exposure, sharpen). Booleans accept on/off/true/false/1/0.
// Unknown keys, bad values and out-of-range numbers are errors. `settings` is updated
// only on success, starting from its current values.
bool ParseGraphicsSettings(std::string_view text, GraphicsSettings &settings, std::string *error = nullptr);

// Canonical text accepted by ParseGraphicsSettings.
std::string FormatGraphicsSettings(const GraphicsSettings &settings);

// Names of the allowed values per key, for discovery by agents.
std::vector<std::string> DescribeGraphicsOptions();

const char *UpscalerName(Upscaler upscaler);

} // namespace openrepublic::runtime
