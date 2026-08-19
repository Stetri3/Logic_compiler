#pragma once
#include "CIntegers.h"
#include "CType_traits.h"
#include "CNew.h"

// Dichiarazioni standard C per i fallback e MSVC senza dipendere da <cstring>
#if defined(_MSC_VER) && !defined(__clang__)
extern "C" {
    void* memcpy(void* dst, const void* src, decltype(sizeof(0)) count);
    void* memmove(void* dst, const void* src, decltype(sizeof(0)) count);
    void* memset(void* dst, int val, decltype(sizeof(0)) count);
    int   memcmp(const void* lhs, const void* rhs, decltype(sizeof(0)) count);
}
#pragma intrinsic(memcpy, memmove, memset, memcmp)
#endif

namespace cc {

    // --- Compiler-Agnostic Low-Level Memory Intrinsics ---

    namespace detail {

        inline void* raw_copy(void* dst, const void* src, decltype(sizeof(0)) count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_memcpy(dst, src, count);
#else
            return ::memcpy(dst, src, count);
#endif
        }

        inline void* raw_move(void* dst, const void* src, decltype(sizeof(0)) count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_memmove(dst, src, count);
#else
            return ::memmove(dst, src, count);
#endif
        }

        inline void* raw_set(void* dst, int val, decltype(sizeof(0)) count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_memset(dst, val, count);
#else
            return ::memset(dst, val, count);
#endif
        }

        inline int raw_compare(const void* lhs, const void* rhs, decltype(sizeof(0)) count) noexcept {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_memcmp(lhs, rhs, count);
#else
            return ::memcmp(lhs, rhs, count);
#endif
        }

    } // namespace detail


    // --- Typed Memory Operations ---

    template <typename T, unsigned_integral SizeType>
    constexpr void copy_memory(T* dst, const T* src, SizeType count) noexcept {
        if (count == 0 || dst == src) return;

        if consteval {
            for (SizeType i = 0; i < count; ++i) {
                dst[i] = src[i];
            }
        }
        else {
            detail::raw_copy(static_cast<void*>(dst), static_cast<const void*>(src), count * sizeof(T));
        }
    }

    template <typename T, unsigned_integral SizeType>
    constexpr void move_memory(T* dst, const T* src, SizeType count) noexcept {
        if (count == 0 || dst == src) return;

        if consteval {
            if (dst < src) {
                for (SizeType i = 0; i < count; ++i) {
                    dst[i] = src[i];
                }
            }
            else {
                for (SizeType i = count; i > 0; --i) {
                    dst[i - 1] = src[i - 1];
                }
            }
        }
        else {
            detail::raw_move(static_cast<void*>(dst), static_cast<const void*>(src), count * sizeof(T));
        }
    }

    template <typename T, unsigned_integral SizeType>
    constexpr void fill_bytes(T* dst, uint8_t byte_val, SizeType count) noexcept {
        if (count == 0) return;

        if consteval {
            // reinterpret_cast è illegale a compile-time: assegnazione diretta per tipi byte-like
            // o fallback ad array elementare
            if constexpr (sizeof(T) == 1) {
                for (SizeType i = 0; i < count; ++i) {
                    dst[i] = static_cast<T>(byte_val);
                }
            }
            else {
                for (SizeType i = 0; i < count; ++i) {
                    dst[i] = T{}; // Default zero-init se invocato a compile-time su tipi composti
                }
            }
        }
        else {
            detail::raw_set(static_cast<void*>(dst), byte_val, count * sizeof(T));
        }
    }

    template <typename T, unsigned_integral SizeType>
    constexpr void zero_memory(T* dst, SizeType count) noexcept {
        fill_bytes(dst, 0, count);
    }

    template <typename T, unsigned_integral SizeType>
    [[nodiscard]] constexpr int compare_memory(const T* lhs, const T* rhs, SizeType count) noexcept {
        if (count == 0 || lhs == rhs) return 0;

        if consteval {
            for (SizeType i = 0; i < count; ++i) {
                if (lhs[i] < rhs[i]) return -1;
                if (lhs[i] > rhs[i]) return 1;
            }
            return 0;
        }
        else {
            return detail::raw_compare(static_cast<const void*>(lhs), static_cast<const void*>(rhs), count * sizeof(T));
        }
    }

    template <typename T, unsigned_integral SizeType>
    [[nodiscard]] constexpr bool equal_memory(const T* lhs, const T* rhs, SizeType count) noexcept {
        return compare_memory(lhs, rhs, count) == 0;
    }

} // namespace cc