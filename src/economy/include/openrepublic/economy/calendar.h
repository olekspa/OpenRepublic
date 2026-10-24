#pragma once
#include "openrepublic/economy/economy.h"

namespace openrepublic::economy {
struct CalendarDate { std::uint32_t year{}, day{}, month{}, season{}; };
// OpenRepublic synthetic policy: 360-day years, 12 x 30-day months, four 90-day seasons.
CalendarDate CalendarAt(const CalendarDef &calendar, std::uint64_t tick) noexcept;
const char *CalendarSeasonName(std::uint32_t season) noexcept;
std::uint64_t ApplySeasonPercent(std::uint64_t amount, std::uint32_t percent) noexcept;
}
