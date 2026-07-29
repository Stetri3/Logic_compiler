#pragma once
#include <string_view>
#include <cstdint>

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