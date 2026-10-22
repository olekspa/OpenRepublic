#include "openrepublic/runtime/graphics_settings.h"

#include <iostream>
#include <string>

using namespace openrepublic::runtime;

namespace {

int failures = 0;

void Check(bool condition, const std::string &label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    GraphicsSettings settings;
    std::string error;
    Check(ParseGraphicsSettings("upscaler=fsr2_balanced; ao=hbao, rt_reflections=on;exposure=1.5;sharpen=0.5",
                                settings, &error),
          "a mixed list parses: " + error);
    Check(settings.upscaler == Upscaler::Fsr2Balanced && settings.ambientOcclusion == AmbientOcclusion::Hbao &&
              settings.rayTracedReflections && settings.exposure == 1.5f && settings.sharpen == 0.5f,
          "values are applied");

    const std::string text = FormatGraphicsSettings(settings);
    GraphicsSettings again;
    Check(ParseGraphicsSettings(text, again, &error) && FormatGraphicsSettings(again) == text,
          "formatted settings round-trip");

    GraphicsSettings kept = settings;
    const char *bad[] = {"upscaler=magic", "ao", "fxaa=maybe", "exposure=100", "sharpen=-1", "mystery=1",
                         "exposure=abc"};
    for (const char *input : bad) {
        Check(!ParseGraphicsSettings(input, kept, &error) && !error.empty() &&
                  FormatGraphicsSettings(kept) == text,
              std::string("rejected and unchanged: ") + input);
    }
    Check(ParseGraphicsSettings("upscaler=dlss", kept, &error) && kept.upscaler == Upscaler::Dlss,
          "dlss is accepted as a reserved value");
    Check(DescribeGraphicsOptions().size() == 9, "options are discoverable");

    if (failures != 0) {
        std::cerr << failures << " graphics-settings test(s) failed\n";
        return 1;
    }
    std::cout << "All graphics-settings tests passed\n";
    return 0;
}
