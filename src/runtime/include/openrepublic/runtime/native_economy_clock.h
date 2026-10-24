#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace openrepublic::runtime {

// Presentation pacing only. Returned ticks must go through the authoritative API.
// Discard long-frame debt so suspension cannot trigger a large simulation burst.
class NativeEconomyClock final {
public:
    void Reset() noexcept { remainder_ = 0.0; }

    std::uint32_t Advance(double elapsedSeconds) noexcept {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0) return 0;
        remainder_ += elapsedSeconds < Period ? elapsedSeconds : Period;
        if (remainder_ < Period) return 0;
        remainder_ -= Period;
        return 1;
    }

    double InterpolationAlpha() const noexcept {
        return std::clamp(remainder_ / Period, 0.0, 1.0);
    }

private:
    static constexpr double Period = 0.25;
    double remainder_ = 0.0;
};

} // namespace openrepublic::runtime
