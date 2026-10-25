#include "openrepublic/runtime/synthetic_terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace openrepublic::runtime {
namespace {

std::uint32_t Hash(std::int32_t x, std::int32_t z, std::uint32_t seed) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343u ^
        static_cast<std::uint32_t>(z) * 0xd8163841u ^ seed * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16; h *= 0xc2b2ae35u; h ^= h >> 16;
    return h;
}

double Fade(double t) noexcept { return t * t * (3.0 - 2.0 * t); }

double Noise(double x, double z, std::uint32_t seed) noexcept {
    const auto ix = static_cast<std::int32_t>(std::floor(x));
    const auto iz = static_cast<std::int32_t>(std::floor(z));
    const double fx = Fade(x - ix), fz = Fade(z - iz);
    const auto value = [seed](std::int32_t a, std::int32_t b) {
        return static_cast<double>(Hash(a, b, seed) & 0x00ffffffu) / 16777215.0 * 2.0 - 1.0;
    };
    const double a = value(ix, iz) + (value(ix + 1, iz) - value(ix, iz)) * fx;
    const double b = value(ix, iz + 1) + (value(ix + 1, iz + 1) - value(ix, iz + 1)) * fx;
    return a + (b - a) * fz;
}

double Fbm(double x, double z, std::uint32_t seed) noexcept {
    double sum = 0.0, amplitude = 1.0, normalizer = 0.0;
    for (std::uint32_t octave = 0; octave < 5; ++octave) {
        sum += Noise(x, z, seed + octave * 1013u) * amplitude;
        normalizer += amplitude;
        x *= 2.0; z *= 2.0; amplitude *= 0.5;
    }
    return sum / normalizer;
}

double Smooth(double lo, double hi, double value) noexcept {
    if (!(hi > lo)) return value >= hi ? 1.0 : 0.0;
    const double t = std::clamp((value - lo) / (hi - lo), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double LakeRadius(double x, double z) noexcept {
    const double dx = (x - 38.0) / 17.0;
    const double dz = (z + 27.0) / 12.0;
    return std::sqrt(dx * dx + dz * dz);
}
}

double SyntheticTerrain::HeightAt(double x, double z) const noexcept {
    if (!std::isfinite(x) || !std::isfinite(z) || std::abs(x) > 1.0e6 || std::abs(z) > 1.0e6) return 0.0;
    // A broad flat starting district joins low rolling land through a smooth apron.
    const double distance = std::hypot(x, z);
    const double apron = Smooth(18.0, 34.0, distance);
    const double hills = 5.2 * Fbm(x / 58.0, z / 58.0, seed) +
        1.8 * Fbm(x / 17.0 + 19.0, z / 17.0 - 7.0, seed ^ 0x9e3779b9u);
    double height = apron * hills;
    // Lake basin is separate from the start district; its surface is a fixed renderer policy.
    const double lake = LakeRadius(x, z);
    height -= 5.0 * (1.0 - Smooth(0.76, 1.05, lake));
    return height;
}

double SyntheticTerrain::SlopeAt(double x, double z) const noexcept {
    if (!std::isfinite(x) || !std::isfinite(z)) return 0.0;
    constexpr double step = 0.5;
    const double dx = (HeightAt(x + step, z) - HeightAt(x - step, z)) / (2.0 * step);
    const double dz = (HeightAt(x, z + step) - HeightAt(x, z - step)) / (2.0 * step);
    return std::hypot(dx, dz);
}

bool SyntheticTerrain::IsBuildable(double centerX, double centerZ, double width,
    double depth, double maxSlope) const noexcept {
    if (!std::isfinite(centerX) || !std::isfinite(centerZ) || !std::isfinite(width) ||
        !std::isfinite(depth) || !std::isfinite(maxSlope) || width <= 0.0 || depth <= 0.0 || maxSlope < 0.0)
        return false;
    if (std::abs(centerX) + width > 1.0e6 || std::abs(centerZ) + depth > 1.0e6) return false;
    const double left = centerX - width * 0.5, right = centerX + width * 0.5;
    const double top = centerZ - depth * 0.5, bottom = centerZ + depth * 0.5;
    for (int iz = 0; iz <= 4; ++iz) {
        const double z = top + (bottom - top) * iz / 4.0;
        for (int ix = 0; ix <= 4; ++ix) {
            const double x = left + (right - left) * ix / 4.0;
            if (IsWater(x, z) || SlopeAt(x, z) > maxSlope) return false;
        }
    }
    return true;
}

bool SyntheticTerrain::IsWater(double x, double z) const noexcept {
    return std::isfinite(x) && std::isfinite(z) && LakeRadius(x, z) < 0.82 && HeightAt(x, z) < waterLevel;
}

} // namespace openrepublic::runtime
