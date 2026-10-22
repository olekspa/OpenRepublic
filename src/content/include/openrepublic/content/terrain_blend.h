#pragma once

#include <array>

namespace openrepublic::content {

// Raw shader inputs are retained even though their semantics are unresolved.
struct TerrainBlendSample {
    float heightMeters = 0.0f;
    float slope = 0.0f;
    std::array<float, 4> maskRgba{};
};

// Values correspond by ordinal to $TILESIZE1..4 and $MASKTILESIZE1..4.
struct TerrainBlendParameters {
    std::array<float, 4> tileSizeMeters{{1000.0f, 1000.0f, 750.0f, 500.0f}};
    std::array<float, 4> maskTileSizeMeters{};
    float detailTileSizeMeters = 0.0f;
    float detailAlphaStart = 0.0f;
};

struct TerrainBlendResult {
    // These are four configured tile layers, not a proven MTL sampler mapping.
    std::array<float, 4> layerWeights{};
    // INFERRED: cycles per world meter, calculated as 1 / $TILESIZE layer.
    std::array<float, 4> uvScalePerMeter{};
};

// VERIFIED: the inspected build's shader container exposes four layer tile-size
// parameters; package script.ini files expose matching numbered parameters.
// UNKNOWN: mask-channel-to-slot mapping, actual blend math, mask/detail transforms,
// and any height/slope-driven material selection. Therefore mask, height and slope
// do not bias the result: each of four configured layers receives an equal
// normalized weight. This deterministic fallback is a preview policy, not WRSR
// parity; the four layers are not established as four MTL sampler slots.
// INFERRED: a positive finite tile size describes one texture repeat's world-space
// span, so UV frequency is its reciprocal. Invalid sizes fall back to 1 cycle/m.
TerrainBlendResult ComputeTerrainBlend(
    const TerrainBlendSample& sample,
    const TerrainBlendParameters& parameters);

}  // namespace openrepublic::content
