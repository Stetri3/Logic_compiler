#pragma once


#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>
#include <iterator>

#ifdef OSTYPE
static_assert(false, "Conflicting macro: OSTYPE");
#endif // OSTYPE

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
namespace { constexpr bool OSTYPE_W = 2 - OSTYPE; }
namespace os_mem {

    namespace detail {
#if OSTYPE == 1
        inline size_t get_page_size() noexcept {
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            return static_cast<size_t>(sysInfo.dwPageSize);
        }

        inline void* reserve(size_t bytes) noexcept {
            return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS);
        }

        inline bool commit(void* addr, size_t bytes) noexcept {
            return VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
        }

        inline bool decommit(void* addr, size_t bytes) noexcept {
            return VirtualFree(addr, bytes, MEM_DECOMMIT) != FALSE;
        }

        inline bool release(void* baseAddr, size_t) noexcept {
            return VirtualFree(baseAddr, 0, MEM_RELEASE) != FALSE;
        }
#elif OSTYPE == 2
        inline size_t get_page_size() noexcept {
            long sz = sysconf(_SC_PAGESIZE);
            return sz > 0 ? static_cast<size_t>(sz) : 4096;
        }

        inline void* reserve(size_t bytes) noexcept {
            void* ptr = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            return (ptr == MAP_FAILED) ? nullptr : ptr;
        }

        inline bool commit(void* addr, size_t bytes) noexcept {
            return mprotect(addr, bytes, PROT_READ | PROT_WRITE) == 0;
        }

        inline bool decommit(void* addr, size_t bytes) noexcept {
#if defined(MADV_DONTNEED)
            madvise(addr, bytes, MADV_DONTNEED);
#endif
            return mprotect(addr, bytes, PROT_NONE) == 0;
        }

        inline bool release(void* baseAddr, size_t reservedBytes) noexcept {
            return munmap(baseAddr, reservedBytes) == 0;
        }
#endif
    } // namespace detail

    // Unified 0-Overhead API
    inline size_t get_page_size() noexcept { return detail::get_page_size(); }
    inline void* reserve(size_t bytes) noexcept { return detail::reserve(bytes); }
    inline bool commit(void* addr, size_t bytes) noexcept { return detail::commit(addr, bytes); }
    inline bool decommit(void* addr, size_t bytes) noexcept { return detail::decommit(addr, bytes); }
    inline bool release(void* baseAddr, size_t reservedBytes) noexcept { return detail::release(baseAddr, reservedBytes); }

    inline size_t page_size() noexcept {
        static const size_t cached_sz = detail::get_page_size();
        return cached_sz;
    }

} // namespace os_mem

namespace sparse {

    template<typename _T, size_t _maxCap = 1ULL << 32>
    class OsPagedVectorImpl {
        static_assert(_maxCap <= SIZE_MAX / sizeof(_T), "Overflow warning: max capacity over SIZE_MAX / sizeof(_T) maximum");

    public:
        using T = _T;
        // Standard Container Type Aliases
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        // --- Iterators ---
        [[nodiscard]] iterator begin() noexcept { return _data; }
        [[nodiscard]] const_iterator begin() const noexcept { return _data; }
        [[nodiscard]] const_iterator cbegin() const noexcept { return _data; }

        [[nodiscard]] iterator end() noexcept { return _data + _size; }
        [[nodiscard]] const_iterator end() const noexcept { return _data + _size; }
        [[nodiscard]] const_iterator cend() const noexcept { return _data + _size; }

        [[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        [[nodiscard]] const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

        [[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
        [[nodiscard]] const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        [[nodiscard]] const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

        // --- Element Access ---
        [[nodiscard]] reference front() noexcept { return _data[0]; }
        [[nodiscard]] const_reference front() const noexcept { return _data[0]; }

        [[nodiscard]] reference back() noexcept { return _data[_size - 1]; }
        [[nodiscard]] const_reference back() const noexcept { return _data[_size - 1]; }

    private:
        static constexpr size_t _el_size = sizeof(T);
        T* _data = nullptr;
        size_t _size = 0;
        size_t _capacity = 0; // Committed capacity (in elements)

        inline static const size_t _page_size = os_mem::page_size();

        // Helper: Convert element count to page-aligned byte count
        static size_t _align_to_page(size_t bytes) noexcept {
            return (bytes + _page_size - 1) & ~(_page_size - 1);
        }

        // Helper: Calculate total committed pages
        size_t _page_count() const noexcept {
            return _align_to_page(_capacity * _el_size) / _page_size;
        }

        // Destroys active elements in range [_size, old_size)
        void _destroy_elements(size_t from_idx, size_t to_idx) noexcept {
            for (size_t i = from_idx; i < to_idx; ++i) {
                _data[i].~T();
            }
        }

    public:
        // Default Constructor: Reserve maximum address space
        OsPagedVectorImpl() {
            _data = static_cast<T*>(os_mem::reserve(_maxCap * _el_size));
            if (_data == nullptr) [[unlikely]] {
                throw std::bad_alloc();
            }
        }

        // Delegating Constructor: Initializes with size & default value
        OsPagedVectorImpl(size_t initSize, const T& initVal = T{}) : OsPagedVectorImpl() {
            if (initSize > 0) {
                resize(initSize, initVal);
            }
        }

        // Destructor: Run element destructors and release virtual address mapping
        ~OsPagedVectorImpl() {
            if (_data) {
                _destroy_elements(0, _size);
                os_mem::release(_data, _maxCap * _el_size);
                _data = nullptr;
            }
        }

        // Disable copy operations (Virtual mapping is unique to this object)
        OsPagedVectorImpl(const OsPagedVectorImpl&) = delete;
        OsPagedVectorImpl& operator=(const OsPagedVectorImpl&) = delete;

        // --- Move Constructor ---
        OsPagedVectorImpl(OsPagedVectorImpl&& other) noexcept
            : _data(std::exchange(other._data, nullptr)),
            _size(std::exchange(other._size, 0)),
            _capacity(std::exchange(other._capacity, 0)) {}

        // --- Move Assignment Operator ---
        OsPagedVectorImpl& operator=(OsPagedVectorImpl&& other) noexcept {
            if (this != &other) {
                // Liberiamo le risorse correnti
                if (_data) {
                    _destroy_elements(0, _size);
                    os_mem::release(_data, _maxCap * _el_size);
                }

                // Trasferiamo la ownership da 'other'
                _data = std::exchange(other._data, nullptr);
                _size = std::exchange(other._size, 0);
                _capacity = std::exchange(other._capacity, 0);
            }
            return *this;
        }

        // Fast Unchecked Element Access
        [[nodiscard]] const T& operator[](size_t index) const noexcept { return _data[index]; }
        [[nodiscard]] T& operator[](size_t index) noexcept { return _data[index]; }

        // Bounds-Checked Element Access
        [[nodiscard]] T& at(size_t index) {
            if (index >= _size) [[unlikely]] {
                throw std::out_of_range("OsPagedVectorImpl::at index out of bounds");
            }
            return _data[index];
        }

        [[nodiscard]] const T& at(size_t index) const {
            if (index >= _size) [[unlikely]] {
                throw std::out_of_range("OsPagedVectorImpl::at index out of bounds");
            }
            return _data[index];
        }

        // Explicitly expand physical capacity by N pages
        int expand(size_t pgAdded) {
            if (pgAdded == 0) return 0;
            size_t new_bytes = (_page_count() + pgAdded) * _page_size;
            size_t target_capacity = new_bytes / _el_size;
            return reserve(target_capacity);
        }

        // Explicitly shrink physical capacity by N pages (decommits trailing RAM)
        int shrink(size_t pgRemoved) {
            size_t current_pages = _page_count();
            if (pgRemoved == 0 || current_pages == 0) return 0;

            size_t keep_pages = (pgRemoved >= current_pages) ? 0 : current_pages - pgRemoved;
            size_t new_committed_bytes = keep_pages * _page_size;
            size_t old_committed_bytes = _align_to_page(_capacity * _el_size);

            if (old_committed_bytes > new_committed_bytes) {
                size_t decommit_bytes = old_committed_bytes - new_committed_bytes;
                uint8_t* decommit_addr = reinterpret_cast<uint8_t*>(_data) + new_committed_bytes;

                // Truncate size if elements were wiped by page decommit
                size_t new_capacity = new_committed_bytes / _el_size;
                if (_size > new_capacity) {
                    _destroy_elements(new_capacity, _size);
                    _size = new_capacity;
                }

                if (!os_mem::decommit(decommit_addr, decommit_bytes)) [[unlikely]] {
                    return -1; // OS Decommit failed
                }
                _capacity = new_capacity;
            }
            return 0;
        }

        // Reserve physical RAM capacity up to newCap elements
        int reserve(size_t newCap) {
            if (newCap > _maxCap) [[unlikely]] {
                return -1; // Exceeds max virtual address range
            }
            if (newCap <= _capacity) return 0;

            size_t current_committed_bytes = _align_to_page(_capacity * _el_size);
            size_t target_committed_bytes = _align_to_page(newCap * _el_size);

            if (target_committed_bytes > current_committed_bytes) {
                size_t commit_bytes = target_committed_bytes - current_committed_bytes;
                uint8_t* commit_addr = reinterpret_cast<uint8_t*>(_data) + current_committed_bytes;

                if (!os_mem::commit(commit_addr, commit_bytes)) [[unlikely]] {
                    return -1; // OS Commit failed
                }
            }

            _capacity = target_committed_bytes / _el_size;
            return 0;
        }

        // Resize logical element count (constructs or destructs elements as needed)
        int resize(size_t newSize, const T& initVal = T{}) {
            if (newSize > _maxCap) [[unlikely]] {
                return -1;
            }

            if (newSize > _capacity) {
                if (reserve(newSize) != 0) return -1;
            }

            if (newSize > _size) {
                // Construct new elements in committed memory
                for (size_t i = _size; i < newSize; ++i) {
                    new (static_cast<void*>(_data + i)) T(initVal);
                }
            }
            else if (newSize < _size) {
                // Destruct trailing elements
                _destroy_elements(newSize, _size);
            }

            _size = newSize;
            return 0;
        }

        // push_back (Copy)
        int push_back(const T& val) {
            if (_size == _capacity) [[unlikely]] {
                // Geometric page growth via ternary max
                size_t double_cap = _capacity * 2;
                size_t inc_cap = _capacity + 1;
                size_t max_growth = (double_cap > inc_cap) ? double_cap : inc_cap;
                size_t next_cap = (_capacity == 0) ? (_page_size / _el_size) : max_growth;
                if (reserve(next_cap) != 0) return -1;
            }

            new (static_cast<void*>(_data + _size)) T(val);
            ++_size;
            return 0;
        }

        // push_back (Move)
        int push_back(T&& val) {
            return emplace_back(std::move(val));
        }

        // emplace_back (In-place Construction)
        template <typename... Args>
        int emplace_back(Args&&... args) {
            if (_size == _capacity) [[unlikely]] {
                size_t double_cap = _capacity * 2;
                size_t inc_cap = _capacity + 1;
                size_t max_growth = (double_cap > inc_cap) ? double_cap : inc_cap;
                size_t next_cap = (_capacity == 0) ? (_page_size / _el_size) : max_growth;
                if (reserve(next_cap) != 0) return -1;
            }

            new (static_cast<void*>(_data + _size)) T(std::forward<Args>(args)...);
            ++_size;
            return 0;
        }

        // Getters
        [[nodiscard]] size_t size() const noexcept { return _size; }
        [[nodiscard]] size_t capacity() const noexcept { return _capacity; }
        [[nodiscard]] static constexpr size_t max_capacity() noexcept { return _maxCap; }
        [[nodiscard]] bool empty() const noexcept { return _size == 0; }
        [[nodiscard]] T* data() noexcept { return _data; }
        [[nodiscard]] const T* data() const noexcept { return _data; }
        [[nodiscard]] size_t page_count() const noexcept { return _page_count(); }

        // Clear elements without decommitting pages
        void clear() noexcept {
            _destroy_elements(0, _size);
            _size = 0;
        }
    };
}
template<typename T, size_t max_virtual = 1ULL << 32>
using OsPagedVector = sparse::OsPagedVectorImpl<T, max_virtual>;

#endif // OSTYPE != 0
#undef OSTYPE