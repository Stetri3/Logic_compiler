#pragma once 
#include "CIntegers.h"
#include "builtin_traits.h"
#include "CType_traits.h"

namespace cc {

    // Helper per garantire zero-overhead su tutti i compilatori (incluso MSVC cl.exe)
#if defined(_MSC_VER) && !defined(__clang__)
#define CC_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define CC_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

    template <unsigned_integral sizeType>
    class CStrViewImpl {
        static constexpr uint8_t sizeSize = sizeof(sizeType);
        static_assert(sizeSize <= 8, "sizeType must be at most 8 bytes");

    public:
        static constexpr uint8_t extra_capacity = 8 - sizeSize;

    private:
        struct EmptyExtra {};
        using ExtraStorage = conditional_t<extra_capacity == 0, EmptyExtra, uint8_t[extra_capacity > 0 ? extra_capacity : 1]>;

        const char* _ptr = nullptr;
        sizeType _size = 0;
        CC_NO_UNIQUE_ADDRESS ExtraStorage _extra{};

        template <unsigned_integral OtherSize>
        using this_templ = CStrViewImpl<OtherSize>;
        using this_type = CStrViewImpl<sizeType>;

        static constexpr sizeType Cstrlen(const char* s) noexcept {
            sizeType len = 0;
            if (s) {
                while (s[len] != '\0') ++len;
            }
            return len;
        }

    public:
        static constexpr sizeType npos = max_integral_v<sizeType>;

        constexpr CStrViewImpl() noexcept = default;
        constexpr CStrViewImpl(const char* ptr, sizeType size) noexcept : _ptr(ptr), _size(size) {}
        constexpr CStrViewImpl(const char* ptr) noexcept : CStrViewImpl(ptr, Cstrlen(ptr)) {}
        constexpr CStrViewImpl(const this_type& other) noexcept = default;

        template <unsigned_integral OtherSize>
        explicit constexpr CStrViewImpl(const CStrViewImpl<OtherSize>& other) noexcept
            : _ptr(other.data()), _size(static_cast<sizeType>(other.size())) {}

        [[nodiscard]] constexpr const char& operator[](sizeType index) const noexcept {
            return *(_ptr + index);
        }

        template <unsigned_integral OtherSize>
        constexpr this_type& operator=(const this_templ<OtherSize>& other) noexcept {
            _ptr = other.data();
            _size = static_cast<sizeType>(other.size());
            return *this;
        }

        template <unsigned_integral OtherSize>
        [[nodiscard]] constexpr bool operator==(const CStrViewImpl<OtherSize>& other) const noexcept {
            if (_size != other.size()) return false;
            if (_ptr == other.data()) return true;
            if (!(_ptr && other.data())) return false;

            for (sizeType i = 0; i < _size; ++i) {
                if ((*this)[i] != other[i]) return false;
            }
            return true;
        }

        template <unsigned_integral OtherSize>
        [[nodiscard]] constexpr bool operator!=(const CStrViewImpl<OtherSize>& other) const noexcept {
            return !(*this == other);
        }

        [[nodiscard]] constexpr this_type substr(sizeType pos, sizeType count) const noexcept {
            return this_type(_ptr + pos, count);
        }

        constexpr void remove_prefix(sizeType n) noexcept {
            _ptr += n;
            _size -= n;
        }

        constexpr void remove_suffix(sizeType n) noexcept {
            _size -= n;
        }

        template <unsigned_integral OtherSize>
        [[nodiscard]] constexpr bool startsWith(this_templ<OtherSize> str) const noexcept {
            if (!str.data()) return false;
            if (str.size() > _size) return false;
            if (str.size() == _size) return *this == str;

            for (sizeType i = 0; i < str.size(); ++i) {
                if ((*this)[i] != str[i]) return false;
            }
            return true;
        }

        template <unsigned_integral OtherSize>
        [[nodiscard]] constexpr bool endsWith(this_templ<OtherSize> str) const noexcept {
            if (!str.data()) return false;
            if (str.size() > _size) return false;
            if (str.size() == _size) return *this == str;

            sizeType offset = _size - static_cast<sizeType>(str.size());
            for (sizeType i = 0; i < str.size(); ++i) {
                if ((*this)[offset + i] != str[i]) return false;
            }
            return true;
        }

        template <unsigned_integral OtherSize>
        [[nodiscard]] constexpr bool hasSubstr(this_templ<OtherSize> str) const noexcept {
            if (!str.data()) return false;
            if (str.size() > _size) return false;
            if (str.empty()) return true;
            if (str.size() == _size) return *this == str;

            sizeType maxStart = _size - static_cast<sizeType>(str.size());
            for (sizeType i = 0; i <= maxStart; ++i) {
                bool match = true;
                for (sizeType j = 0; j < str.size(); ++j) {
                    if ((*this)[i + j] != str[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return true;
            }
            return false;
        }

        template <unsigned_integral OtherSize>
        [[nodiscard]] constexpr sizeType findNoCheck(this_templ<OtherSize> str) const noexcept {
            const sizeType sSize = static_cast<sizeType>(str.size());

            if (sSize == 1) {
                const char target = str[0];
                for (sizeType i = 0; i < _size; ++i) {
                    if (_ptr[i] == target) return i;
                }
                return npos;
            }

            const char first = str[0];
            const char last = str[sSize - 1];
            const sizeType maxStart = _size - sSize;

            for (sizeType i = 0; i <= maxStart; ++i) {
                if (_ptr[i] == first && _ptr[i + sSize - 1] == last) {
                    sizeType j = 1;
                    const sizeType innerEnd = sSize - 1;
                    while (j < innerEnd && _ptr[i + j] == str[j]) {
                        ++j;
                    }
                    if (j == innerEnd) return i;
                }
            }
            return npos;
        }

        template <unsigned_integral OtherSize>
        [[nodiscard]] constexpr sizeType find(this_templ<OtherSize> str) const noexcept {
            const sizeType sSize = static_cast<sizeType>(str.size());
            if (sSize == 0) return 0;
            if (!str.data() || sSize > _size) return npos;
            if (sSize == _size) return (*this == str) ? 0 : npos;

            return findNoCheck(str);
        }

        [[nodiscard]] constexpr const char* data() const noexcept { return _ptr; }
        [[nodiscard]] constexpr sizeType size() const noexcept { return _size; }
        [[nodiscard]] constexpr bool empty() const noexcept { return _size == 0; }

        // --- Extra Bytes Interface ---

        [[nodiscard]] static constexpr uint8_t extraSize() noexcept {
            return extra_capacity;
        }

        [[nodiscard]] constexpr uint8_t getExtra(uint8_t index) const noexcept {
            if constexpr (extra_capacity > 0) {
                return _extra[index];
            }
            else {
                return 0;
            }
        }

        constexpr void setExtra(uint8_t index, uint8_t value) noexcept {
            if constexpr (extra_capacity > 0) {
                _extra[index] = value;
            }
        }

        [[nodiscard]] constexpr const uint8_t* extraData() const noexcept {
            if constexpr (extra_capacity > 0) {
                return _extra;
            }
            else {
                return nullptr;
            }
        }

        [[nodiscard]] constexpr uint8_t* extraData() noexcept {
            if constexpr (extra_capacity > 0) {
                return _extra;
            }
            else {
                return nullptr;
            }
        }

        // --- Iterators ---

        using const_iterator = const char*;
        using iterator = const_iterator;

        [[nodiscard]] constexpr const_iterator begin() const noexcept { return _ptr; }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return _ptr + _size; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return _ptr; }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return _ptr + _size; }
    };

#undef CC_NO_UNIQUE_ADDRESS

    using CStrView8 = CStrViewImpl<uint8_t>;
    using CStrView16 = CStrViewImpl<uint16_t>;
    using CStrView32 = CStrViewImpl<uint32_t>;
    using CStrView64 = CStrViewImpl<uint64_t>;
    using CStringView = CStrView32;

    extern template class CStrViewImpl<uint8_t>;
    extern template class CStrViewImpl<uint16_t>;
    extern template class CStrViewImpl<uint32_t>;
    extern template class CStrViewImpl<uint64_t>;

    // Static Assertions su layout a 16 byte
    static_assert(sizeof(CStrView8) == 16);
    static_assert(sizeof(CStrView16) == 16);
    static_assert(sizeof(CStrView32) == 16);
    static_assert(sizeof(CStrView64) == 16);

    // --- Literal Operators ---

    // 1. Literal standard: ritorna CStrView (32-bit di default)
    [[nodiscard]] constexpr CStringView operator""_Csv(const char* str, decltype(sizeof(0)) len) noexcept {
        return CStringView(str, static_cast<uint32_t>(len));
    }

    // 2. Compile-Time Smallest-Type Literal Wrapper
    template <decltype(sizeof(0)) N>
    struct FixedStringLiteral {
        char value[N];
        static constexpr decltype(sizeof(0)) length = N - 1;

        constexpr FixedStringLiteral(const char(&str)[N]) noexcept {
            for (decltype(sizeof(0)) i = 0; i < N; ++i) {
                value[i] = str[i];
            }
        }
    };

    // Ritorna esattamente il CStrView con il sizeType più piccolo possibile
    template <FixedStringLiteral Lit>
    [[nodiscard]] constexpr auto operator""_Csvs() noexcept {
        constexpr auto len = Lit.length;
        if constexpr (len <= 0xFF) {
            return CStrView8(Lit.value, static_cast<uint8_t>(len));
        }
        else if constexpr (len <= 0xFFFF) {
            return CStrView16(Lit.value, static_cast<uint16_t>(len));
        }
        else if constexpr (len <= 0xFFFFFFFF) {
            return CStrView32(Lit.value, static_cast<uint32_t>(len));
        }
        else {
            return CStrView64(Lit.value, static_cast<uint64_t>(len));
        }
    }

} // namespace cc
using cc::operator""_Csv;
using cc::operator""_Csvs;