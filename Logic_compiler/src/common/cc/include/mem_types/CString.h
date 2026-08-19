#pragma once
#include "CIntegers.h"
#include "CType_traits.h"
#include "CNew.h"
#include "CMemory.h"
#include "CAllocators.h"
#include "CUninitialized.h"

namespace cc {

    template <typename CharT = char, typename Alloc = HeapAllocator<CharT>>
    class BasicCString {
    public:
        using value_type = CharT;
        using size_type = decltype(sizeof(0));
        using difference_type = decltype(static_cast<CharT*>(nullptr) - static_cast<CharT*>(nullptr));
        using reference = CharT&;
        using const_reference = const CharT&;
        using pointer = CharT*;
        using const_pointer = const CharT*;
        using allocator_type = Alloc;

        static constexpr size_type npos = static_cast<size_type>(-1);

    private:
        // Capacità SSO calcolata per occupare esattamente la stessa memoria della struttura Heap (24 byte su x64)
        struct HeapRepr {
            pointer   data;
            size_type size;
            size_type capacity;
        };

        static constexpr size_type SSO_CAPACITY = (sizeof(HeapRepr) / sizeof(value_type)) > 1
            ? (sizeof(HeapRepr) / sizeof(value_type)) - 1
            : 1;

        struct SsoRepr {
            value_type buffer[SSO_CAPACITY + 1];
        };

        union Storage {
            HeapRepr heap;
            SsoRepr  sso;
        } _storage;

        bool _is_sso = true;
        [[no_unique_address]] allocator_type _alloc{};

        // --- Helper Interni ---

        static constexpr size_type string_length(const_pointer s) noexcept {
            if (!s) return 0;
            size_type len = 0;
            while (s[len] != value_type(0)) {
                ++len;
            }
            return len;
        }

        constexpr void set_sso_size(size_type sz) noexcept {
            _storage.sso.buffer[sz] = value_type(0);
        }

    public:
        // --- Costruttori & Distruttore ---

        constexpr BasicCString() noexcept {
            _is_sso = true;
            _storage.sso.buffer[0] = value_type(0);
        }

        constexpr BasicCString(const_pointer s) {
            _is_sso = true;
            _storage.sso.buffer[0] = value_type(0);
            if (s) {
                assign(s, string_length(s));
            }
        }

        constexpr BasicCString(const_pointer s, size_type count) {
            _is_sso = true;
            _storage.sso.buffer[0] = value_type(0);
            assign(s, count);
        }

        constexpr BasicCString(size_type count, value_type ch) {
            _is_sso = true;
            _storage.sso.buffer[0] = value_type(0);
            assign(count, ch);
        }

        constexpr BasicCString(const BasicCString& other) {
            _is_sso = true;
            _storage.sso.buffer[0] = value_type(0);
            assign(other.data(), other.size());
        }

        constexpr BasicCString(BasicCString&& other) noexcept {
            if (other._is_sso) {
                _is_sso = true;
                copy_memory(_storage.sso.buffer, other._storage.sso.buffer, SSO_CAPACITY + 1);
            }
            else {
                _is_sso = false;
                _storage.heap = other._storage.heap;
            }
            other._is_sso = true;
            other._storage.sso.buffer[0] = value_type(0);
        }

        constexpr ~BasicCString() noexcept {
            clear_and_deallocate();
        }

        // --- Assegnazioni ---

        constexpr BasicCString& operator=(const BasicCString& other) {
            if (this != &other) {
                assign(other.data(), other.size());
            }
            return *this;
        }

        constexpr BasicCString& operator=(BasicCString&& other) noexcept {
            if (this != &other) {
                clear_and_deallocate();
                if (other._is_sso) {
                    _is_sso = true;
                    copy_memory(_storage.sso.buffer, other._storage.sso.buffer, SSO_CAPACITY + 1);
                }
                else {
                    _is_sso = false;
                    _storage.heap = other._storage.heap;
                }
                other._is_sso = true;
                other._storage.sso.buffer[0] = value_type(0);
            }
            return *this;
        }

        constexpr BasicCString& operator=(const_pointer s) {
            return assign(s, string_length(s));
        }

        // --- Modificatori ---

        constexpr BasicCString& assign(const_pointer s, size_type count) {
            reserve(count);
            pointer d = data_mut();
            if (count > 0 && s) {
                copy_memory(d, s, count);
            }
            d[count] = value_type(0);
            if (!_is_sso) {
                _storage.heap.size = count;
            }
            return *this;
        }

        constexpr BasicCString& assign(size_type count, value_type ch) {
            reserve(count);
            pointer d = data_mut();
            if (count > 0) {
                if constexpr (sizeof(value_type) == 1) {
                    fill_bytes(d, static_cast<uint8_t>(ch), count);
                }
                else {
                    for (size_type i = 0; i < count; ++i) d[i] = ch;
                }
            }
            d[count] = value_type(0);
            if (!_is_sso) {
                _storage.heap.size = count;
            }
            return *this;
        }

        constexpr void reserve(size_type new_cap) {
            if (new_cap <= capacity()) return;

            // Richiesta allocazione Heap
            size_type alloc_size = new_cap + 1;
            pointer new_buf = _alloc.allocate(alloc_size);
            if (!new_buf) return;

            size_type current_sz = size();
            if (current_sz > 0) {
                copy_memory(new_buf, data(), current_sz);
            }
            new_buf[current_sz] = value_type(0);

            if (!_is_sso) {
                _alloc.deallocate(_storage.heap.data, _storage.heap.capacity + 1);
            }

            _is_sso = false;
            _storage.heap.data = new_buf;
            _storage.heap.size = current_sz;
            _storage.heap.capacity = new_cap;
        }

        constexpr void push_back(value_type ch) {
            append(&ch, 1);
        }

        constexpr BasicCString& append(const_pointer s, size_type count) {
            if (count == 0 || !s) return *this;
            size_type current_sz = size();
            size_type needed_cap = current_sz + count;

            if (needed_cap > capacity()) {
                // Growth policy: geometric doubling
                size_type new_cap = capacity() * 2;
                if (new_cap < needed_cap) new_cap = needed_cap;
                reserve(new_cap);
            }

            pointer d = data_mut();
            copy_memory(d + current_sz, s, count);
            d[needed_cap] = value_type(0);

            if (!_is_sso) {
                _storage.heap.size = needed_cap;
            }
            return *this;
        }

        constexpr BasicCString& append(const BasicCString& other) {
            return append(other.data(), other.size());
        }

        constexpr BasicCString& append(const_pointer s) {
            return append(s, string_length(s));
        }

        constexpr BasicCString& operator+=(const BasicCString& other) { return append(other); }
        constexpr BasicCString& operator+=(const_pointer s) { return append(s); }
        constexpr BasicCString& operator+=(value_type ch) { push_back(ch); return *this; }

        constexpr void clear() noexcept {
            if (_is_sso) {
                _storage.sso.buffer[0] = value_type(0);
            }
            else {
                _storage.heap.size = 0;
                _storage.heap.data[0] = value_type(0);
            }
        }

        // --- Osservatori ---

        [[nodiscard]] constexpr size_type size() const noexcept {
            if (_is_sso) {
                return string_length(_storage.sso.buffer);
            }
            return _storage.heap.size;
        }

        [[nodiscard]] constexpr size_type length() const noexcept { return size(); }
        [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

        [[nodiscard]] constexpr size_type capacity() const noexcept {
            return _is_sso ? SSO_CAPACITY : _storage.heap.capacity;
        }

        [[nodiscard]] constexpr const_pointer c_str() const noexcept { return data(); }
        [[nodiscard]] constexpr const_pointer data() const noexcept {
            return _is_sso ? _storage.sso.buffer : _storage.heap.data;
        }

        [[nodiscard]] constexpr pointer data() noexcept { return data_mut(); }

        [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept {
            return data()[index];
        }

        [[nodiscard]] constexpr reference operator[](size_type index) noexcept {
            return data_mut()[index];
        }

        [[nodiscard]] constexpr const_reference front() const noexcept { return data()[0]; }
        [[nodiscard]] constexpr reference front() noexcept { return data_mut()[0]; }
        [[nodiscard]] constexpr const_reference back() const noexcept { return data()[size() - 1]; }
        [[nodiscard]] constexpr reference back() noexcept { return data_mut()[size() - 1]; }

        // --- Confronti ---

        [[nodiscard]] constexpr int compare(const BasicCString& other) const noexcept {
            return compare(other.data());
        }

        [[nodiscard]] constexpr int compare(const_pointer s) const noexcept {
            if (!s) return 1;
            const_pointer p1 = data();
            while (*p1 && (*p1 == *s)) {
                ++p1;
                ++s;
            }
            return static_cast<int>(static_cast<typename make_unsigned<value_type>::type>(*p1)) -
                static_cast<int>(static_cast<typename make_unsigned<value_type>::type>(*s));
        }

        [[nodiscard]] constexpr bool operator==(const BasicCString& other) const noexcept {
            if (size() != other.size()) return false;
            return equal_memory(data(), other.data(), size());
        }

        [[nodiscard]] constexpr bool operator==(const_pointer s) const noexcept {
            return compare(s) == 0;
        }

        [[nodiscard]] constexpr bool operator<(const BasicCString& other) const noexcept {
            return compare(other) < 0;
        }

    private:
        constexpr pointer data_mut() noexcept {
            return _is_sso ? _storage.sso.buffer : _storage.heap.data;
        }

        constexpr void clear_and_deallocate() noexcept {
            if (!_is_sso && _storage.heap.data) {
                _alloc.deallocate(_storage.heap.data, _storage.heap.capacity + 1);
            }
        }
    };

    // --- Alias Standard CString ---

    using CString = BasicCString<char>;
    using CWString = BasicCString<wchar_t>;
    using CString16 = BasicCString<char16_t>;
    using CString32 = BasicCString<char32_t>;

} // namespace cc