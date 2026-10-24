#include "openrepublic/economy/calendar.h"
#include <limits>
#include <algorithm>

namespace openrepublic::economy {
CalendarDate CalendarAt(const CalendarDef &calendar, std::uint64_t tick) noexcept {
    if (!calendar.ticksPerDay) return {};
    const std::uint64_t elapsed = tick / calendar.ticksPerDay;
    const std::uint64_t dayIndex = static_cast<std::uint64_t>(calendar.startDay - 1) + (elapsed % 360);
    const std::uint64_t years = elapsed / 360 + dayIndex / 360;
    const std::uint64_t year = static_cast<std::uint64_t>(calendar.startYear) + years;
    const auto boundedYear = static_cast<std::uint32_t>(std::min<std::uint64_t>(year, std::numeric_limits<std::uint32_t>::max()));
    const std::uint32_t day = static_cast<std::uint32_t>(dayIndex % 360) + 1;
    return {boundedYear, day, (day - 1) / 30 + 1, (day - 1) / 90};
}
const char *CalendarSeasonName(std::uint32_t season) noexcept {
    static constexpr const char *names[] = {"spring", "summer", "autumn", "winter"};
    return season < 4 ? names[season] : "unknown";
}
std::uint64_t ApplySeasonPercent(std::uint64_t amount, std::uint32_t percent) noexcept {
    const auto whole = amount / 100;
    const auto remainder = amount % 100;
    if (percent && whole > std::numeric_limits<std::uint64_t>::max() / percent)
        return std::numeric_limits<std::uint64_t>::max();
    const auto base = whole * percent;
    const auto tail = remainder * percent / 100;
    return tail > std::numeric_limits<std::uint64_t>::max() - base
        ? std::numeric_limits<std::uint64_t>::max() : base + tail;
}
}
