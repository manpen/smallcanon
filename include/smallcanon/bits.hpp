#pragma once
#include <concepts>
#include <cstddef>

template <std::unsigned_integral T>
constexpr T round_up_power_of_two(T v) {
    v--;

    for (std::size_t i = 1; i < 8 * sizeof(T); i *= 2) {
        v |= v >> i;
    }

    return ++v;
}
