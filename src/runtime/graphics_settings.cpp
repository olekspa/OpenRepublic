#include "openrepublic/runtime/graphics_settings.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace openrepublic::runtime {
namespace {

bool Fail(std::string *error, const std::string &message) {
    if (error) *error = message;
    return false;
}

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
    return std::string(text.substr(begin, end - begin));
}

bool ParseBool(const std::string &value, bool &out) {
    if (value == "on" || value == "true" || value == "1") {
        out = true;
        return true;
    }
    if (value == "off" || value == "false" || value == "0") {
        out = false;
        return true;
    }
    return false;
}

bool ParseNumber(const std::string &value, float low, float high, float &out) {
    errno = 0;
    char *end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (value.empty() || end != value.c_str() + value.size() || errno == ERANGE || !std::isfinite(parsed) ||
        parsed < low || parsed > high)
        return false;
    out = static_cast<float>(parsed);
    return true;
}

struct UpscalerEntry {
    const char *name;
    Upscaler value;
};

constexpr UpscalerEntry kUpscalers[] = {
    {"none", Upscaler::None},
    {"fsr2_quality", Upscaler::Fsr2Quality},
    {"fsr2_balanced", Upscaler::Fsr2Balanced},
    {"fsr2_performance", Upscaler::Fsr2Performance},
    {"fsr2_ultra_performance", Upscaler::Fsr2UltraPerformance},
    {"dlss", Upscaler::Dlss},
};

struct AoEntry {
    const char *name;
    AmbientOcclusion value;
};

constexpr AoEntry kAo[] = {
    {"off", AmbientOcclusion::Off},   {"ssao", AmbientOcclusion::Ssao}, {"hbao", AmbientOcclusion::Hbao},
    {"msao", AmbientOcclusion::Msao}, {"rtao", AmbientOcclusion::RayTraced},
};

const char *AoName(AmbientOcclusion ao) {
    for (const auto &entry : kAo) {
        if (entry.value == ao) return entry.name;
    }
    return "off";
}

} // namespace

const char *UpscalerName(Upscaler upscaler) {
    for (const auto &entry : kUpscalers) {
        if (entry.value == upscaler) return entry.name;
    }
    return "none";
}

bool ParseGraphicsSettings(std::string_view text, GraphicsSettings &settings, std::string *error) {
    GraphicsSettings next = settings;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t stop = text.find_first_of(";,", start);
        if (stop == std::string_view::npos) stop = text.size();
        const std::string pair = Trim(text.substr(start, stop - start));
        start = stop + 1;
        if (pair.empty()) continue;
        const std::size_t equals = pair.find('=');
        if (equals == std::string::npos) return Fail(error, "expected key=value, got '" + pair + "'");
        const std::string key = Trim(pair.substr(0, equals));
        const std::string value = Trim(pair.substr(equals + 1));
        bool ok = false;
        if (key == "upscaler") {
            for (const auto &entry : kUpscalers) {
                if (value == entry.name) {
                    next.upscaler = entry.value;
                    ok = true;
                }
            }
        } else if (key == "ao") {
            for (const auto &entry : kAo) {
                if (value == entry.name) {
                    next.ambientOcclusion = entry.value;
                    ok = true;
                }
            }
        } else if (key == "rt_reflections") {
            ok = ParseBool(value, next.rayTracedReflections);
        } else if (key == "rt_diffuse") {
            ok = ParseBool(value, next.rayTracedDiffuse);
        } else if (key == "ssr") {
            ok = ParseBool(value, next.screenSpaceReflections);
        } else if (key == "fxaa") {
            ok = ParseBool(value, next.fxaa);
        } else if (key == "bloom") {
            ok = ParseBool(value, next.bloom);
        } else if (key == "exposure") {
            ok = ParseNumber(value, 0.05f, 8.0f, next.exposure);
        } else if (key == "sharpen") {
            ok = ParseNumber(value, 0.0f, 2.0f, next.sharpen);
        } else {
            return Fail(error, "unknown graphics key '" + key + "'");
        }
        if (!ok) return Fail(error, "invalid value '" + value + "' for graphics key '" + key + "'");
    }
    settings = next;
    if (error) error->clear();
    return true;
}

std::string FormatGraphicsSettings(const GraphicsSettings &settings) {
    std::ostringstream out;
    out << "upscaler=" << UpscalerName(settings.upscaler) << ";ao=" << AoName(settings.ambientOcclusion)
        << ";rt_reflections=" << (settings.rayTracedReflections ? "on" : "off")
        << ";rt_diffuse=" << (settings.rayTracedDiffuse ? "on" : "off")
        << ";ssr=" << (settings.screenSpaceReflections ? "on" : "off")
        << ";fxaa=" << (settings.fxaa ? "on" : "off") << ";bloom=" << (settings.bloom ? "on" : "off")
        << ";exposure=" << settings.exposure;
    if (settings.sharpen >= 0.0f) out << ";sharpen=" << settings.sharpen;
    return out.str();
}

std::vector<std::string> DescribeGraphicsOptions() {
    std::string upscalers;
    for (const auto &entry : kUpscalers) upscalers += std::string(upscalers.empty() ? "" : "|") + entry.name;
    std::string ao;
    for (const auto &entry : kAo) ao += std::string(ao.empty() ? "" : "|") + entry.name;
    return {"upscaler=" + upscalers + " (dlss is reserved: reported unavailable until Streamline is integrated)",
            "ao=" + ao,
            "rt_reflections=on|off (needs hardware ray tracing)",
            "rt_diffuse=on|off (needs hardware ray tracing)",
            "ssr=on|off",
            "fxaa=on|off",
            "bloom=on|off",
            "exposure=0.05..8",
            "sharpen=0..2"};
}

} // namespace openrepublic::runtime
