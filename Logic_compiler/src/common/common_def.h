#pragma once
#include <cstdint>
#include <type_traits>
#include <bit>

template <typename T>
    requires std::unsigned_integral<T>
inline constexpr T align_two(T in) {
    return static_cast<T>(std::bit_ceil(static_cast<uint64_t>(in)));
}
template <typename T>
    requires std::unsigned_integral<T>
inline constexpr uint64_t align_two_of(T in) {
    return std::bit_ceil(static_cast<uint64_t>(in));
}