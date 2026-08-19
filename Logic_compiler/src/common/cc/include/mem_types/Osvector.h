#pragma once
#include "CIntegers.h"
#include "CType_traits.h"
#include "CNew.h"
#include "CMemops.h"
#include "std_types.h"


#ifdef OSTYPE
static_assert(false, "Conflicting macro: OSTYPE");
#endif

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define OSTYPE 1

#elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__POSIX__) || defined(_POSIX_VERSION)
#include <sys/mman.h>
#include <unistd.h>
#define OSTYPE 2

#else
#define OSTYPE 0
#warning "SparseVector: Unsupported target OS. Virtual memory mapping requires Windows or POSIX (Linux/macOS)."
#define VPAGING_NOT_SUPPORTED
#endif

#if OSTYPE != 0

namespace cc::os_mem {

    namespace detail {
#if OSTYPE == 1
        inline decltype(sizeof(0)) get_page_size() noexcept {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            return static_cast<decltype(sizeof(0))>(sysInfo.dwPageSize);
        }

        inline void* reserve(decltype(sizeof(0)) bytes) noexcept {
            return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS);
        }

        inline bool commit(void* addr, decltype(sizeof(0)) bytes) noexcept {
            return VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
        }

        inline bool decommit(void* addr, decltype(sizeof(0)) bytes) noexcept {
            return VirtualFree(addr, bytes, MEM_DECOMMIT) != FALSE;
        }

        inline bool release(void* baseAddr, decltype(sizeof(0))) noexcept {
            return VirtualFree(baseAddr, 0, MEM_RELEASE) != FALSE;
        }
#elif OSTYPE == 2
        inline decltype(sizeof(0)) get_page_size() noexcept {
            long sz = sysconf(_SC_PAGESIZE);
            return sz > 0 ? static_cast<decltype(sizeof(0))>(sz) : 4096;
        }

        inline void* reserve(decltype(sizeof(0)) bytes) noexcept {
            void* ptr = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            return (ptr == MAP_FAILED) ? nullptr : ptr;
        }

        inline bool commit(void* addr, decltype(sizeof(0)) bytes) noexcept {
            return mprotect(addr, bytes, PROT_READ | PROT_WRITE) == 0;
        }

        inline bool decommit(void* addr, decltype(sizeof(0)) bytes) noexcept {
#if defined(MADV_DONTNEED)
            madvise(addr, bytes, MADV_DONTNEED);
#endif
            return mprotect(addr, bytes, PROT_NONE) == 0;
        }

        inline bool release(void* baseAddr, decltype(sizeof(0)) reservedBytes) noexcept {
            return munmap(baseAddr, reservedBytes) == 0;
        }
#endif
    } // namespace detail

    // Unified 0-Overhead API
    inline decltype(sizeof(0)) get_page_size() noexcept { return detail::get_page_size(); }
    inline void* reserve(decltype(sizeof(0)) bytes) noexcept { return detail::reserve(bytes); }
    inline bool commit(void* addr, decltype(sizeof(0)) bytes) noexcept { return detail::commit(addr, bytes); }
    inline bool decommit(void* addr, decltype(sizeof(0)) bytes) noexcept { return detail::decommit(addr, bytes); }
    inline bool release(void* baseAddr, decltype(sizeof(0)) reservedBytes) noexcept { return detail::release(baseAddr, reservedBytes); }

    inline decltype(sizeof(0)) page_size() noexcept {
        static const decltype(sizeof(0)) cached_sz = detail::get_page_size();
        return cached_sz;
    }

} // namespace cc::os_mem

namespace cc::osvector {

    namespace detail {
        template <typename T, typename U = T>
        constexpr T exchange(T& obj, U&& new_val) noexcept {
            T old_val = cc::move(obj);
            obj = cc::forward<U>(new_val);
            return old_val;
        }
    } // namespace detail

    template <typename _T, decltype(sizeof(0)) _maxCap = (1ULL << 32)>
    class OsPagedVectorImpl {
        static_assert(_maxCap <= max_integral_v<decltype(sizeof(0))> / sizeof(_T),
            "Overflow warning: max capacity exceeds addressable size limit");

    public:
        using value_type = _T;
        using size_type = decltype(sizeof(0));
        using difference_type = decltype(static_cast<_T*>(nullptr) - static_cast<_T*>(nullptr));
        using reference = _T&;
        using const_reference = const _T&;
        using pointer = _T*;
        using const_pointer = const _T*;
        using iterator = _T*;
        using const_iterator = const _T*;

        // --- Iterators ---
        [[nodiscard]] constexpr iterator begin() noexcept { return _data; }
        [[nodiscard]] constexpr const_iterator begin() const noexcept { return _data; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return _data; }

        [[nodiscard]] constexpr iterator end() noexcept { return _data + _size; }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return _data + _size; }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return _data + _size; }

        // --- Element Access ---
        [[nodiscard]] constexpr reference front() noexcept { return _data[0]; }
        [[nodiscard]] constexpr const_reference front() const noexcept { return _data[0]; }

        [[nodiscard]] constexpr reference back() noexcept { return _data[_size - 1]; }
        [[nodiscard]] constexpr const_reference back() const noexcept { return _data[_size - 1]; }

        [[nodiscard]] constexpr const _T& operator[](size_type index) const noexcept { return _data[index]; }
        [[nodiscard]] constexpr _T& operator[](size_type index) noexcept { return _data[index]; }

        // --- CSpan Conversion ---
        [[nodiscard]] constexpr CSpan<_T> span() noexcept {
            return CSpan<_T>(_data, _size);
        }

        [[nodiscard]] constexpr CSpan<const _T> span() const noexcept {
            return CSpan<const _T>(_data, _size);
        }

        constexpr operator CSpan<_T>() noexcept {
            return span();
        }

        constexpr operator CSpan<const _T>() const noexcept {
            return span();
        }

    private:
        static constexpr size_type _el_size = sizeof(_T);
        _T* _data = nullptr;
        size_type _size = 0;
        size_type _capacity = 0; // In numero di elementi

        inline static const size_type _page_size = os_mem::page_size();

        static size_type _align_to_page(size_type bytes) noexcept {
            return (bytes + _page_size - 1) & ~(_page_size - 1);
        }

        size_type _page_count() const noexcept {
            return _align_to_page(_capacity * _el_size) / _page_size;
        }

        void _destroy_elements(size_type from_idx, size_type to_idx) noexcept {
            if constexpr (!is_scalar_v<_T>) {
                destroy_range(_data + from_idx, to_idx - from_idx);
            }
        }

    public:
        // Default Constructor
        OsPagedVectorImpl() noexcept {
            _data = static_cast<_T*>(os_mem::reserve(_maxCap * _el_size));
        }

        // Delegating Constructor
        OsPagedVectorImpl(size_type initSize, const _T& initVal = _T{}) : OsPagedVectorImpl() {
            if (_data && initSize > 0) {
                resize(initSize, initVal);
            }
        }

        // Destructor
        ~OsPagedVectorImpl() noexcept {
            if (_data) {
                _destroy_elements(0, _size);
                os_mem::release(_data, _maxCap * _el_size);
                _data = nullptr;
            }
        }

        // Non-copyable
        OsPagedVectorImpl(const OsPagedVectorImpl&) = delete;
        OsPagedVectorImpl& operator=(const OsPagedVectorImpl&) = delete;

        // Move Semantics
        constexpr OsPagedVectorImpl(OsPagedVectorImpl&& other) noexcept
            : _data(detail::exchange(other._data, nullptr)),
            _size(detail::exchange(other._size, 0)),
            _capacity(detail::exchange(other._capacity, 0)) {}

        constexpr OsPagedVectorImpl& operator=(OsPagedVectorImpl&& other) noexcept {
            if (this != &other) {
                if (_data) {
                    _destroy_elements(0, _size);
                    os_mem::release(_data, _maxCap * _el_size);
                }

                _data = detail::exchange(other._data, nullptr);
                _size = detail::exchange(other._size, 0);
                _capacity = detail::exchange(other._capacity, 0);
            }
            return *this;
        }

        // Explicit physical expansion by N pages
        int expand(size_type pgAdded) noexcept {
            if (pgAdded == 0) return 0;
            size_type new_bytes = (_page_count() + pgAdded) * _page_size;
            size_type target_capacity = new_bytes / _el_size;
            return reserve(target_capacity);
        }

        // Explicit physical shrink by N pages
        int shrink(size_type pgRemoved) noexcept {
            size_type current_pages = _page_count();
            if (pgRemoved == 0 || current_pages == 0) return 0;

            size_type keep_pages = (pgRemoved >= current_pages) ? 0 : current_pages - pgRemoved;
            size_type new_committed_bytes = keep_pages * _page_size;
            size_type old_committed_bytes = _align_to_page(_capacity * _el_size);

            if (old_committed_bytes > new_committed_bytes) {
                size_type decommit_bytes = old_committed_bytes - new_committed_bytes;
                uint8_t* decommit_addr = reinterpret_cast<uint8_t*>(_data) + new_committed_bytes;

                size_type new_capacity = new_committed_bytes / _el_size;
                if (_size > new_capacity) {
                    _destroy_elements(new_capacity, _size);
                    _size = new_capacity;
                }

                if (!os_mem::decommit(decommit_addr, decommit_bytes)) [[unlikely]] {
                    return -1;
                }
                _capacity = new_capacity;
            }
            return 0;
        }

        // Commit RAM up to newCap elements
        int reserve(size_type newCap) noexcept {
            if (!_data || newCap > _maxCap) [[unlikely]] {
                return -1;
            }
            if (newCap <= _capacity) return 0;

            size_type current_committed_bytes = _align_to_page(_capacity * _el_size);
            size_type target_committed_bytes = _align_to_page(newCap * _el_size);

            if (target_committed_bytes > current_committed_bytes) {
                size_type commit_bytes = target_committed_bytes - current_committed_bytes;
                uint8_t* commit_addr = reinterpret_cast<uint8_t*>(_data) + current_committed_bytes;

                if (!os_mem::commit(commit_addr, commit_bytes)) [[unlikely]] {
                    return -1;
                }
            }

            _capacity = target_committed_bytes / _el_size;
            return 0;
        }

        // Resize logical element count
        int resize(size_type newSize, const _T& initVal = _T{}) {
            if (newSize > _maxCap) [[unlikely]] {
                return -1;
            }

            if (newSize > _capacity) {
                if (reserve(newSize) != 0) return -1;
            }

            if (newSize > _size) {
                for (size_type i = _size; i < newSize; ++i) {
                    construct_at(_data + i, initVal);
                }
            }
            else if (newSize < _size) {
                _destroy_elements(newSize, _size);
            }

            _size = newSize;
            return 0;
        }

        // Push Back (Copy)
        int push_back(const _T& val) {
            return emplace_back(val);
        }

        // Push Back (Move)
        int push_back(_T&& val) {
            return emplace_back(cc::move(val));
        }

        // In-place Back Construction
        template <typename... Args>
        int emplace_back(Args&&... args) {
            if (_size == _capacity) [[unlikely]] {
                size_type double_cap = _capacity * 2;
                size_type inc_cap = _capacity + 1;
                size_type max_growth = (double_cap > inc_cap) ? double_cap : inc_cap;
                size_type next_cap = (_capacity == 0) ? (_page_size / _el_size) : max_growth;
                if (reserve(next_cap) != 0) return -1;
            }

            construct_at(_data + _size, cc::forward<Args>(args)...);
            ++_size;
            return 0;
        }

        // Observers
        [[nodiscard]] constexpr size_type size() const noexcept { return _size; }
        [[nodiscard]] constexpr size_type capacity() const noexcept { return _capacity; }
        [[nodiscard]] static constexpr size_type max_capacity() noexcept { return _maxCap; }
        [[nodiscard]] constexpr bool empty() const noexcept { return _size == 0; }
        [[nodiscard]] constexpr _T* data() noexcept { return _data; }
        [[nodiscard]] constexpr const _T* data() const noexcept { return _data; }
        [[nodiscard]] size_type page_count() const noexcept { return _page_count(); }

        // Clear elements without decommitting pages
        void clear() noexcept {
            _destroy_elements(0, _size);
            _size = 0;
        }
    };

    extern template class OsPagedVectorImpl<char>;
    extern template class OsPagedVectorImpl<char8_t>;
} // namespace cc::osvector

//publi
namespace cc {

    template <typename T, decltype(sizeof(0)) MaxVirtualCap = (1ULL << 32)>
    using OsPagedVector = osvector::OsPagedVectorImpl<T, MaxVirtualCap>;

} // namespace cc

#endif // OSTYPE != 0
#undef OSTYPE