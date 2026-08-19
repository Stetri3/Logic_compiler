#pragma once
#include "CIntegers.h"
#include "CType_traits.h"
#include "CNew.h"
#include "CMemops.h"

// Dichiarazioni standard C senza dipendere da <cstdlib>
#if defined(_MSC_VER) && !defined(__clang__)
extern "C" {
    void* malloc(decltype(sizeof(0)) size);
    void* realloc(void* ptr, decltype(sizeof(0)) new_size);
    void  free(void* ptr);
}
#else
extern "C" {
    void* malloc(decltype(sizeof(0)) size) noexcept;
    void* realloc(void* ptr, decltype(sizeof(0)) new_size) noexcept;
    void  free(void* ptr) noexcept;
}
#endif

namespace cc {

    // --- Allocator Concept ---

    template <typename A, typename T>
    concept allocator = requires(A a, decltype(sizeof(0)) n, T * ptr) {
        { a.allocate(n) } -> same_as<T*>;
        { a.deallocate(ptr, n) };
    };


    // --- Default Heap Allocator ---

    template <typename T>
    class HeapAllocator {
    public:
        using value_type = T;
        using size_type = decltype(sizeof(0));

        constexpr HeapAllocator() noexcept = default;

        template <typename U>
        constexpr HeapAllocator(const HeapAllocator<U>&) noexcept {}

        [[nodiscard]] static T* allocate(size_type count) noexcept {
            if (count == 0) return nullptr;
            return static_cast<T*>(malloc(count * sizeof(T)));
        }

        [[nodiscard]] static T* reallocate(T* ptr, size_type new_count) noexcept {
            if (new_count == 0) {
                if (ptr) free(static_cast<void*>(ptr));
                return nullptr;
            }
            return static_cast<T*>(realloc(static_cast<void*>(ptr), new_count * sizeof(T)));
        }

        static void deallocate(T* ptr, size_type = 0) noexcept {
            if (ptr) {
                free(static_cast<void*>(ptr));
            }
        }

        template <typename... Args>
        static constexpr T* construct(T* ptr, Args&&... args) noexcept {
            return construct_at(ptr, forward<Args>(args)...);
        }

        static constexpr void destroy(T* ptr) noexcept {
            destroy_at(ptr);
        }

        template <unsigned_integral SizeType>
        static constexpr void destroy(T* first, SizeType count) noexcept {
            destroy_range(first, count);
        }

        constexpr bool operator==(const HeapAllocator&) const noexcept = default;
    };


    // --- Monotonic Arena / Buffer Allocator ---

    template <typename T, decltype(sizeof(0)) Capacity>
    class FixedArenaAllocator {
    public:
        using value_type = T;
        using size_type = decltype(sizeof(0));

        static_assert(Capacity > 0, "FixedArenaAllocator requires Capacity > 0");

    private:
        alignas(alignof(T)) uint8_t _buffer[Capacity * sizeof(T)];
        size_type _allocated_count = 0;

    public:
        constexpr FixedArenaAllocator() noexcept = default;

        // Non copiabile per evitare shallow copies del buffer interno
        FixedArenaAllocator(const FixedArenaAllocator&) = delete;
        FixedArenaAllocator& operator=(const FixedArenaAllocator&) = delete;

        FixedArenaAllocator(FixedArenaAllocator&&) noexcept = default;
        FixedArenaAllocator& operator=(FixedArenaAllocator&&) noexcept = default;

        [[nodiscard]] T* allocate(size_type count) noexcept {
            if (count == 0 || (_allocated_count + count > Capacity)) {
                return nullptr;
            }
            T* result = reinterpret_cast<T*>(_buffer + (_allocated_count * sizeof(T)));
            _allocated_count += count;
            return result;
        }

        // Monotonic arenas non rilasciano blocchi singoli
        void deallocate(T*, size_type = 0) noexcept {}

        void reset() noexcept {
            _allocated_count = 0;
        }

        [[nodiscard]] size_type allocated() const noexcept {
            return _allocated_count;
        }

        [[nodiscard]] static constexpr size_type capacity() noexcept {
            return Capacity;
        }
    };

} // namespace cc