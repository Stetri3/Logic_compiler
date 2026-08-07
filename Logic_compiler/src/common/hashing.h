#pragma once
#include <string_view>
#include <cstdint>
#include <array>

// FNV-1a hash a 64-bit eseguibile a compile-time
constexpr uint64_t hash_directive(std::string_view sv) noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : sv) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// User-defined literal per scrivere hash a compile-time in modo leggibile
constexpr uint64_t operator"" _h(const char* str, size_t len) noexcept {
    return hash_directive(std::string_view(str, len));
}
namespace {
    //helper to make a lut
    [[nodiscard]] consteval std::array<uint8_t, 256> make_ident_6bit_lut() noexcept {
        std::array<uint8_t, 256> lut{};
        for (int i = 0; i < 256; ++i) {
            const auto c = static_cast<unsigned char>(i);
            if (c >= 'A' && c <= 'Z')      lut[i] = static_cast<uint8_t>(c - 'A' + 1);  // 1..26
            else if (c >= 'a' && c <= 'z') lut[i] = static_cast<uint8_t>(c - 'a' + 27); // 27..52
            else if (c >= '0' && c <= '9') lut[i] = static_cast<uint8_t>(c - '0' + 53); // 53..62
            else if (c == '_')             lut[i] = 63;
            else                           lut[i] = 0;
        }
        return lut;
    }
}
//LUT to store identifier chars in 6 bits
// Stored in .rodata, aligned to a cache line (64 bytes)
constexpr alignas(64) std::array<uint8_t, 256> IDENT_CHAR_LUT = make_ident_6bit_lut();

[[nodiscard]] inline constexpr bool IS_IDENT_CHAR(char c) noexcept {
    return IDENT_CHAR_LUT[static_cast<unsigned char>(c)] != 0;
}

using CharBitLUT = std::array<std::uint64_t, 4>;

namespace {
    [[nodiscard]] consteval std::array<std::uint64_t, 4> make_ident_bitmask() noexcept {
        std::array<std::uint64_t, 4> mask{};
        for (std::size_t i = 0; i < 256; ++i) {
            if (IDENT_CHAR_LUT[i] != 0) {
                mask[i >> 6] |= (1ULL << (i & 63));
            }
        }
        return mask;
    }
}
constexpr alignas(64) std::array<uint64_t, 4> IDENT_CHAR_BITMASK = make_ident_bitmask();

[[nodiscard]] inline constexpr bool is_in(const CharBitLUT& LUT, std::uint8_t c) noexcept {
    return (LUT[c >> 6] & (1ULL << (c & 63))) != 0;
}

