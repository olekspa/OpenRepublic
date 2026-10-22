#include "openrepublic/content/terrain_blend.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool Near(float a, float b) {
    return std::abs(a - b) < 1e-6f;
}

}  // namespace

int main() {
    using namespace openrepublic::content;
    TerrainBlendParameters parameters;
    parameters.tileSizeMeters = {{1000.0f, 1000.0f, 750.0f, 500.0f}};

    TerrainBlendSample sample;
    sample.heightMeters = 1200.0f;
    sample.slope = 0.9f;
    sample.maskRgba = {{0.0f, 1.0f, 0.0f, 1.0f}};
    const auto result = ComputeTerrainBlend(sample, parameters);
    float total = 0.0f;
    for (float weight : result.layerWeights) {
        Check(Near(weight, 0.25f), "unknown mask semantics use equal slot weights");
        total += weight;
    }
    Check(Near(total, 1.0f), "weights sum to one");
    Check(Near(result.uvScalePerMeter[0], 0.001f), "slot 1 UV frequency follows tile size");
    Check(Near(result.uvScalePerMeter[2], 1.0f / 750.0f), "slot 3 UV frequency follows tile size");
    Check(Near(result.uvScalePerMeter[3], 0.002f), "slot 4 UV frequency follows tile size");

    TerrainBlendSample other;
    other.heightMeters = -500.0f;
    other.slope = -12.0f;
    other.maskRgba = {{1.0f, 0.0f, 1.0f, 0.0f}};
    const auto neutral = ComputeTerrainBlend(other, parameters);
    Check(neutral.layerWeights == result.layerWeights,
          "height, slope, and mask do not invent material selection rules");

    parameters.tileSizeMeters = {{0.0f, -1.0f,
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}};
    const auto invalidSizes = ComputeTerrainBlend(sample, parameters);
    for (float scale : invalidSizes.uvScalePerMeter) {
        Check(std::isfinite(scale) && Near(scale, 1.0f),
              "invalid tile sizes use finite documented fallback");
    }

    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Terrain blend tests passed\n";
    return EXIT_SUCCESS;
}
