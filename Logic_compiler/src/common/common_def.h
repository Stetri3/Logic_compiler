#pragma once
#include <cstdint>
#include <type_traits>
#include <bit>

//find the first power of two higher than in
template <typename T>
    requires std::unsigned_integral<T>
inline constexpr T align_two(T in) {
    return static_cast<T>(std::bit_ceil(static_cast<uint64_t>(in)));
}
template <typename T> //overflow safe (up to 2^64-1)
    requires std::unsigned_integral<T>
inline constexpr uint64_t align_two_of(T in) {
    return std::bit_ceil(static_cast<uint64_t>(in));
}

template <typename T>
consteval T unsigned_max_v() {
    static_assert(static_cast<T>(-1) > static_cast<T>(0), "Max<T> is only valid for unsigned types.");
    return static_cast<T>(-1);
}