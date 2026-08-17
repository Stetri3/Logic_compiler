#pragma once

#include <cstddef>
#include <stdexcept>

namespace cc { //common types
    //Non owning span
    template <typename T>
    class CSpan {
    private:
        T* ptr_ = nullptr;
        std::size_t size_ = 0;

    public:
        constexpr CSpan() noexcept = default;
        constexpr CSpan(const T* ptr, std::size_t size) noexcept : ptr_(ptr), size_(size) {}
        constexpr CSpan(T* ptr, std::size_t size) noexcept : ptr_(ptr), size_(size) {}

        template <std::size_t N>
        constexpr CSpan(T(&arr)[N]) noexcept : ptr_(arr), size_(N) {}

        // Allow CSpan<T> -> CSpan<const T> conversion
        constexpr operator CSpan<const T>() const noexcept {
            return CSpan<const T>(ptr_, size_);
        }

        // Observers
        [[nodiscard]] constexpr T* data() const noexcept { return ptr_; }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
        [[nodiscard]] constexpr std::size_t size_bytes() const noexcept { return size_ * sizeof(T); }
        [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

        // Element Access
        [[nodiscard]] constexpr T& operator[](std::size_t idx) const noexcept {
            return ptr_[idx];
        }

        [[nodiscard]] constexpr T& at(std::size_t idx) const {
            if (idx >= size_) {
                throw std::out_of_range("CSpan::at out of range");
            }
            return ptr_[idx];
        }

        [[nodiscard]] constexpr T& front() const noexcept { return ptr_[0]; }
        [[nodiscard]] constexpr T& back() const noexcept { return ptr_[size_ - 1]; }

        // Bare pointers for range-for support
        [[nodiscard]] constexpr T* begin() const noexcept { return ptr_; }
        [[nodiscard]] constexpr T* end() const noexcept { return ptr_ + size_; }

        // Slicing
        [[nodiscard]] constexpr CSpan subspan(std::size_t offset, std::size_t count) const noexcept {
            return CSpan(ptr_ + offset, count);
        }

        [[nodiscard]] constexpr CSpan first(std::size_t count) const noexcept {
            return CSpan(ptr_, count);
        }

        [[nodiscard]] constexpr CSpan last(std::size_t count) const noexcept {
            return CSpan(ptr_ + (size_ - count), count);
        }
    };

    // CTAD
    template <typename T, std::size_t N>
    CSpan(T(&)[N]) -> CSpan<T>;

    template <typename T>
    CSpan(T*, std::size_t) -> CSpan<T>;

    //Owning array c style
    template <typename T, std::size_t N>
    class CArray {
    private:
        T data_[N ? N : 1]{};

    public:
        // Aggregate-style fill
        constexpr void fill(const T& value) {
            for (std::size_t i = 0; i < N; ++i) {
                data_[i] = value;
            }
        }

        // Observers
        [[nodiscard]] constexpr T* data() noexcept { return data_; }
        [[nodiscard]] constexpr const T* data() const noexcept { return data_; }

        [[nodiscard]] static constexpr std::size_t size() noexcept { return N; }
        [[nodiscard]] static constexpr std::size_t size_bytes() noexcept { return N * sizeof(T); }
        [[nodiscard]] static constexpr bool empty() noexcept { return N == 0; }

        // Element Access
        [[nodiscard]] constexpr T& operator[](std::size_t idx) noexcept {
            return data_[idx];
        }

        [[nodiscard]] constexpr const T& operator[](std::size_t idx) const noexcept {
            return data_[idx];
        }

        [[nodiscard]] constexpr T& at(std::size_t idx) {
            if (idx >= N) {
                throw std::out_of_range("CArray::at out of range");
            }
            return data_[idx];
        }

        [[nodiscard]] constexpr const T& at(std::size_t idx) const {
            if (idx >= N) {
                throw std::out_of_range("CArray::at out of range");
            }
            return data_[idx];
        }

        [[nodiscard]] constexpr T& front() noexcept { return data_[0]; }
        [[nodiscard]] constexpr const T& front() const noexcept { return data_[0]; }

        [[nodiscard]] constexpr T& back() noexcept { return data_[N > 0 ? N - 1 : 0]; }
        [[nodiscard]] constexpr const T& back() const noexcept { return data_[N > 0 ? N - 1 : 0]; }

        // Bare pointers for range-for and C-APIs
        [[nodiscard]] constexpr T* begin() noexcept { return data_; }
        [[nodiscard]] constexpr const T* begin() const noexcept { return data_; }
        [[nodiscard]] constexpr T* end() noexcept { return data_ + N; }
        [[nodiscard]] constexpr const T* end() const noexcept { return data_ + N; }

        // Seamless conversion to CSpan
        template <template <typename> class SpanType>
        [[nodiscard]] constexpr SpanType<T> span() noexcept {
            return SpanType<T>(data_, N);
        }

        template <template <typename> class SpanType>
        [[nodiscard]] constexpr SpanType<const T> span() const noexcept {
            return SpanType<const T>(data_, N);
        }
    };

    // CTAD for variadic arguments: CArray a{1, 2, 3}; -> CArray<int, 3>
    template <typename First, typename... Rest>
    CArray(First, Rest...) -> CArray<First, 1 + sizeof...(Rest)>;
}