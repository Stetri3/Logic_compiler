#pragma once
#include "CIntegers.h"
#include "CType_traits.h"
#include "CNew.h"
#include "CAllocators.h"

namespace cc {

    // --- Default Deleters ---

    template <typename T>
    struct DefaultDelete {
        constexpr DefaultDelete() noexcept = default;

        template <typename U>
            requires is_base_of_v<T, U> || is_same_v<T, U>
        constexpr DefaultDelete(const DefaultDelete<U>&) noexcept {}

        constexpr void operator()(T* ptr) const noexcept {
            static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
            if (ptr) {
                destroy_at(ptr);
                HeapAllocator<T>::deallocate(ptr, 1);
            }
        }
    };

    template <typename T>
    struct DefaultDelete<T[]> {
        constexpr DefaultDelete() noexcept = default;

        template <typename U>
            requires is_base_of_v<T, U> || is_same_v<T, U>
        constexpr DefaultDelete(const DefaultDelete<U[]>&) noexcept {}

        constexpr void operator()(T* ptr) const noexcept {
            static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
            if (ptr) {
                // Nota: per array non tracciati si assume deallocazione grezza dell'heap.
                // Se T non è banale nei range dinamici, usare i container dedicati.
                destroy_at(ptr);
                HeapAllocator<T>::deallocate(ptr);
            }
        }
    };


    // --- Owning Pointer (Single Object & Array) ---

    template <typename T, typename Deleter = DefaultDelete<T>>
    class OwningPtr {
    public:
        using element_type = T;
        using deleter_type = Deleter;
        using pointer = T*;

    private:
        pointer _ptr = nullptr;
        [[no_unique_address]] deleter_type _deleter{};

    public:
        // Costruttori
        constexpr OwningPtr() noexcept = default;
        constexpr OwningPtr(decltype(nullptr)) noexcept : _ptr(nullptr), _deleter() {}

        explicit constexpr OwningPtr(pointer p) noexcept : _ptr(p), _deleter() {}

        constexpr OwningPtr(pointer p, conditional_t<is_reference_v<Deleter>, Deleter, const Deleter&> d) noexcept
            : _ptr(p), _deleter(d) {}

        constexpr OwningPtr(pointer p, remove_reference_t<Deleter>&& d) noexcept
            : _ptr(p), _deleter(move(d)) {}

        // Distruttore
        constexpr ~OwningPtr() noexcept {
            reset();
        }

        // Move semantics
        constexpr OwningPtr(OwningPtr&& other) noexcept
            : _ptr(other.release()), _deleter(forward<Deleter>(other._deleter)) {}

        template <typename U, typename E>
            requires (is_base_of_v<T, U> || is_same_v<T, U>) && is_assignable_v<Deleter&, E&&>
        constexpr OwningPtr(OwningPtr<U, E>&& other) noexcept
            : _ptr(other.release()), _deleter(forward<E>(other.get_deleter())) {}

        constexpr OwningPtr& operator=(OwningPtr&& other) noexcept {
            if (this != &other) {
                reset(other.release());
                _deleter = forward<Deleter>(other._deleter);
            }
            return *this;
        }

        template <typename U, typename E>
            requires (is_base_of_v<T, U> || is_same_v<T, U>) && is_assignable_v<Deleter&, E&&>
        constexpr OwningPtr& operator=(OwningPtr<U, E>&& other) noexcept {
            reset(other.release());
            _deleter = forward<E>(other.get_deleter());
            return *this;
        }

        constexpr OwningPtr& operator=(decltype(nullptr)) noexcept {
            reset();
            return *this;
        }

        // No copy
        OwningPtr(const OwningPtr&) = delete;
        OwningPtr& operator=(const OwningPtr&) = delete;

        // Osservatori
        [[nodiscard]] constexpr pointer get() const noexcept { return _ptr; }
        [[nodiscard]] constexpr deleter_type& get_deleter() noexcept { return _deleter; }
        [[nodiscard]] constexpr const deleter_type& get_deleter() const noexcept { return _deleter; }

        [[nodiscard]] explicit constexpr operator bool() const noexcept { return _ptr != nullptr; }

        [[nodiscard]] constexpr add_lvalue_reference_t<T> operator*() const noexcept { return *_ptr; }
        [[nodiscard]] constexpr pointer operator->() const noexcept { return _ptr; }

        // Modificatori
        constexpr pointer release() noexcept {
            pointer p = _ptr;
            _ptr = nullptr;
            return p;
        }

        constexpr void reset(pointer p = nullptr) noexcept {
            pointer old_ptr = _ptr;
            _ptr = p;
            if (old_ptr) {
                _deleter(old_ptr);
            }
        }

        constexpr void swap(OwningPtr& other) noexcept {
            pointer tmp_p = _ptr;
            _ptr = other._ptr;
            other._ptr = tmp_p;

            Deleter tmp_d = move(_deleter);
            _deleter = move(other._deleter);
            other._deleter = move(tmp_d);
        }

        // Confronti
        constexpr bool operator==(const OwningPtr& other) const noexcept { return _ptr == other._ptr; }
        constexpr bool operator==(decltype(nullptr)) const noexcept { return _ptr == nullptr; }
    };


    // --- Owning Pointer Specialization for Unbounded Arrays (T[]) ---

    template <typename T, typename Deleter>
    class OwningPtr<T[], Deleter> {
    public:
        using element_type = T;
        using deleter_type = Deleter;
        using pointer = T*;

    private:
        pointer _ptr = nullptr;
        [[no_unique_address]] deleter_type _deleter{};

    public:
        constexpr OwningPtr() noexcept = default;
        constexpr OwningPtr(decltype(nullptr)) noexcept : _ptr(nullptr), _deleter() {}

        template <typename U>
            requires is_same_v<U, pointer> || is_null_pointer_v<U>
        explicit constexpr OwningPtr(U p) noexcept : _ptr(p), _deleter() {}

        constexpr ~OwningPtr() noexcept {
            reset();
        }

        // Move semantics
        constexpr OwningPtr(OwningPtr&& other) noexcept
            : _ptr(other.release()), _deleter(forward<Deleter>(other._deleter)) {}

        constexpr OwningPtr& operator=(OwningPtr&& other) noexcept {
            if (this != &other) {
                reset(other.release());
                _deleter = forward<Deleter>(other._deleter);
            }
            return *this;
        }

        constexpr OwningPtr& operator=(decltype(nullptr)) noexcept {
            reset();
            return *this;
        }

        // No copy
        OwningPtr(const OwningPtr&) = delete;
        OwningPtr& operator=(const OwningPtr&) = delete;

        // Accesso indicizzato
        [[nodiscard]] constexpr T& operator[](decltype(sizeof(0)) index) const noexcept {
            return _ptr[index];
        }

        [[nodiscard]] constexpr pointer get() const noexcept { return _ptr; }
        [[nodiscard]] constexpr deleter_type& get_deleter() noexcept { return _deleter; }
        [[nodiscard]] constexpr const deleter_type& get_deleter() const noexcept { return _deleter; }

        [[nodiscard]] explicit constexpr operator bool() const noexcept { return _ptr != nullptr; }

        constexpr pointer release() noexcept {
            pointer p = _ptr;
            _ptr = nullptr;
            return p;
        }

        constexpr void reset(pointer p = nullptr) noexcept {
            pointer old_ptr = _ptr;
            _ptr = p;
            if (old_ptr) {
                _deleter(old_ptr);
            }
        }

        constexpr void swap(OwningPtr& other) noexcept {
            pointer tmp_p = _ptr;
            _ptr = other._ptr;
            other._ptr = tmp_p;

            Deleter tmp_d = move(_deleter);
            _deleter = move(other._deleter);
            other._deleter = move(tmp_d);
        }

        constexpr bool operator==(const OwningPtr& other) const noexcept { return _ptr == other._ptr; }
        constexpr bool operator==(decltype(nullptr)) const noexcept { return _ptr == nullptr; }
    };


    // --- Factory Helpers ---

    template <typename T, typename... Args>
        requires (!is_array_v<T>)
    [[nodiscard]] constexpr OwningPtr<T> make_owning(Args&&... args) {
        T* memory = HeapAllocator<T>::allocate(1);
        if (!memory) return nullptr;
        construct_at(memory, forward<Args>(args)...);
        return OwningPtr<T>(memory);
    }

    template <typename T>
        requires (is_array_v<T>&& extent_v<T> == 0) // Solo unbounded arrays T[]
    [[nodiscard]] constexpr OwningPtr<T> make_owning(decltype(sizeof(0)) count) {
        using Element = remove_extent_t<T>;
        Element* memory = HeapAllocator<Element>::allocate(count);
        if (!memory) return nullptr;
        uninitialized_default_construct_n(memory, count);
        return OwningPtr<T>(memory);
    }

    template <typename T>
        requires (!is_array_v<T>)
    [[nodiscard]] constexpr OwningPtr<T> make_owning_for_overwrite() {
        T* memory = HeapAllocator<T>::allocate(1);
        if (!memory) return nullptr;
        if constexpr (!is_trivially_default_constructible_v<T>) {
            construct_at(memory);
        }
        return OwningPtr<T>(memory);
    }


    // --- Non-Owning Observer Pointer ---

    template <typename T>
    class ObserverPtr {
    private:
        T* _ptr = nullptr;

    public:
        constexpr ObserverPtr() noexcept = default;
        constexpr ObserverPtr(decltype(nullptr)) noexcept : _ptr(nullptr) {}
        constexpr ObserverPtr(T* ptr) noexcept : _ptr(ptr) {}

        template <typename U>
            requires is_base_of_v<T, U> || is_same_v<T, U>
        constexpr ObserverPtr(const ObserverPtr<U>& other) noexcept : _ptr(other.get()) {}

        template <typename U, typename D>
            requires is_base_of_v<T, U> || is_same_v<T, U>
        constexpr ObserverPtr(const OwningPtr<U, D>& owning) noexcept : _ptr(owning.get()) {}

        [[nodiscard]] constexpr T* get() const noexcept { return _ptr; }
        [[nodiscard]] explicit constexpr operator bool() const noexcept { return _ptr != nullptr; }

        [[nodiscard]] constexpr add_lvalue_reference_t<T> operator*() const noexcept { return *_ptr; }
        [[nodiscard]] constexpr T* operator->() const noexcept { return _ptr; }

        constexpr void reset(T* p = nullptr) noexcept { _ptr = p; }

        constexpr bool operator==(const ObserverPtr& other) const noexcept = default;
        constexpr bool operator==(decltype(nullptr)) const noexcept { return _ptr == nullptr; }
    };

} // namespace cc