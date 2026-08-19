#pragma once
#include "CIntegers.h"
#include "builtin_traits.h"

// Placement new definitions without <new>
#if !defined(__cpp_lib_placement_new) && !defined(_NEW) && !defined(_NEW_) && !defined(__PLACEMENT_NEW_INLINE)
#define __PLACEMENT_NEW_INLINE

[[nodiscard]] inline void* operator new(decltype(sizeof(0)), void* ptr) noexcept {
    return ptr;
}

inline void operator delete(void*, void*) noexcept {}

#if !defined(__cpp_lib_placement_new)
#define __cpp_lib_placement_new 200110L
#endif

#endif

namespace cc {

    // --- Forward & Move Utilities ---

    template <typename T>
    [[nodiscard]] constexpr remove_reference_t<T>&& move(T&& t) noexcept {
        return static_cast<remove_reference_t<T>&&>(t);
    }

    template <typename T>
    [[nodiscard]] constexpr T&& forward(remove_reference_t<T>& t) noexcept {
        return static_cast<T&&>(t);
    }

    template <typename T>
    [[nodiscard]] constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
        static_assert(!is_lvalue_reference_v<T>, "Cannot forward an rvalue as an lvalue");
        return static_cast<T&&>(t);
    }


    // --- In-place Object Lifecycle Management ---

    template <typename T, typename... Args>
    constexpr T* construct_at(T* ptr, Args&&... args) noexcept(noexcept(::new(static_cast<void*>(ptr)) T(forward<Args>(args)...))) {
        return ::new(static_cast<void*>(ptr)) T(forward<Args>(args)...);
    }

    template <typename T>
    constexpr void destroy_at(T* ptr) noexcept {
        if constexpr (!is_scalar_v<T>) {
            ptr->~T();
        }
    }

    template <typename T, unsigned_integral SizeType>
    constexpr void destroy_range(T* first, SizeType count) noexcept {
        if constexpr (!is_scalar_v<T>) {
            for (SizeType i = 0; i < count; ++i) {
                (first + i)->~T();
            }
        }
    }

} // namespace cc