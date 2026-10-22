#include "openrepublic/content/terrain_blend.h"

#include <cmath>

namespace openrepublic::content {

TerrainBlendResult ComputeTerrainBlend(
    const TerrainBlendSample& sample,
    const TerrainBlendParameters& parameters) {
    // Inputs whose original semantics are UNKNOWN are deliberately not interpreted.
    (void)sample;
    (void)parameters.maskTileSizeMeters;
    (void)parameters.detailTileSizeMeters;
    (void)parameters.detailAlphaStart;

    TerrainBlendResult result;
    result.layerWeights.fill(0.25f);
    for (std::size_t i = 0; i < result.uvScalePerMeter.size(); ++i) {
        const float size = parameters.tileSizeMeters[i];
        result.uvScalePerMeter[i] = std::isfinite(size) && size > 0.0f
            ? 1.0f / size
            : 1.0f;
    }
    return result;
}

}  // namespace openrepublic::content
