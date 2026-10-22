#include "openrepublic/runtime/native_economy_clock.h"

#include <iostream>
#include <limits>

int main() {
    openrepublic::runtime::NativeEconomyClock clock;
    int failures = 0;
    const auto check = [&](bool ok, const char* message) {
        if (!ok) { ++failures; std::cerr << message << '\n'; }
    };
    check(clock.Advance(0.125) == 0, "half period must wait");
    check(clock.Advance(0.125) == 1, "whole period must tick");
    check(clock.Advance(0.0) == 0, "zero time must wait");
    check(clock.Advance(-1.0) == 0, "negative time must wait");
    check(clock.Advance(std::numeric_limits<double>::quiet_NaN()) == 0, "NaN must wait");
    check(clock.Advance(std::numeric_limits<double>::infinity()) == 0, "infinity must wait");
    check(clock.Advance(0.125) == 0, "invalid times must not contaminate clock");
    check(clock.Advance(1000.0) == 1, "long suspension must emit only one tick");
    check(clock.Advance(0.125) == 1, "clamped frame must preserve fractional remainder");
    clock.Advance(0.125);
    clock.Reset();
    check(clock.Advance(0.125) == 0, "pause reset must discard fractional debt");
    clock.Reset();
    unsigned ticks = 0;
    for (int i = 0; i < 1024; ++i) ticks += clock.Advance(1.0 / 128.0);
    check(ticks == 32, "eight seconds at 128Hz must yield 32 ticks");
    for (int i = 0; i < 100; ++i)
        check(clock.Advance(std::numeric_limits<double>::max()) == 1, "bounded catchup");
    return failures == 0 ? 0 : 1;
}
