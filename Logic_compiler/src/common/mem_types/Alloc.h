#pragma once

#include <memory>
#include <cstdint>
#include <vector>
#include <cassert>
#include <bit>
#include <type_traits>

namespace cont {
	template<uint32_t _width, size_t _size>
	class StackDumbArrImpl {
#define A(...) static_assert(sizeof(__VA_ARGS__) == _width, "Error: size not compatible")

		static_assert(_size > 0, "why would you think that's a good idea");
		static constexpr size_t bSize = _size * _width;
		uint8_t raw[bSize];

	public:
		StackDumbArrImpl() {}
		template <typename T>
		StackDumbArrImpl(T init) {
			A(T);
			auto bytes = std::bit_cast<std::array<uint8_t, _width>>(init);
			for (size_t i = 0; i < _size; ++i) {
				for (size_t b = 0; b < _width; ++b) {
					raw[i * _width + b] = bytes[b];
				}
			}
		}
		static constexpr size_t size() { return _size; }
#undef A
	};


}

namespace sparse {

	template <typename T>
	inline constexpr uint32_t default_block_capacity =
		(sizeof(T) <= 64) ? (4096 / sizeof(T)) : 64;
	
	template<typename _T, uint32_t blockCapacity>
	struct Block {
		using T = _T;
		T raw[blockCapacity];
		T* begin() const noexcept {
			return raw;
		}
		T* end() const noexcept {
			return raw + blockCapacity;
		}
		Block() = default;
		constexpr Block(T init) {
			for (uint32_t i = 0; i < blockCapacity; ++i)
				raw[i] = init;
		}
		Block(const T* begin, const T* end) {
			const uint32_t n = static_cast<uint32_t>(end - begin);
			const uint32_t copyCount = n < blockCapacity ? n : blockCapacity;
			for (uint32_t i = 0; i < copyCount; ++i)
				raw[i] = begin[i];
			for (uint32_t i = copyCount; i < blockCapacity; ++i)
				raw[i] = T{};
		}
		template <uint32_t otherCap = blockCapacity>
		Block<T, otherCap> Dupe() const {
			Block<T, otherCap> result;
			constexpr uint32_t n = (otherCap < blockCapacity) ? otherCap : blockCapacity;
			for (uint32_t i = 0; i < n; ++i)
				result.raw[i] = raw[i];
			for (uint32_t i = n; i < otherCap; ++i)
				result.raw[i] = T{};
			return result;
		}
		static constexpr auto size() { return blockCapacity; }
		T* data() const noexcept { return raw; }

		T& operator[](uint32_t index) { return raw[index]; }
        const T& operator[](uint32_t index) const { return raw[index]; }
	};


	template<typename _T, uint32_t blockCapacity = default_block_capacity<_T>, size_t maxStacked = 16>
	class SparseArrImpl {
		static_assert(blockCapacity > 0, "why would you think that's a good idea");
		//static
		using T = _T;
		static constexpr bool noStack = maxStacked == 0;
		//linking
		Block<T, blockCapacity>* indexing[maxStacked + 1] = {};

		inline Block<T, blockCapacity>** getOuter() const {
			return reinterpret_cast<Block<T, blockCapacity>**>(indexing[maxStacked]);
		}

		//control
		uint32_t Num = 0; //Number of allocated blocks

		//utility
		inline Block<T, blockCapacity>& heapBlock(uint32_t index) {
			//Always working, blocks are initialized at construction
			return *(getOuter()[index]);
		}
		T& heapAccess(uint32_t block, uint32_t offset) { //T must be initialized to work
			return heapBlock(block)[offset]; }
		const T& heapAccess(uint32_t block, uint32_t offset)  const { return heapBlock(block)[offset]; }
		
	public:
		SparseArrImpl(size_t size) {
			if (size == 0) size = 1;
			const size_t bigN = static_cast<size_t>(size - 1) / blockCapacity + 1;
			assert(bigN <= 0xFFFFFFFF && "Overflow risk, SparseArr can manage only up to 2^32 blocks");
			Num = static_cast<uint32_t>(bigN);
			const bool heapAlloc = noStack || Num > maxStacked;
			const uint32_t stacked = heapAlloc ? maxStacked : Num;
			for (uint32_t i = 0; i < stacked; ++i)
			{
				indexing[i] = new Block<T, blockCapacity>;
			}
			const uint32_t heaped = heapAlloc ? static_cast<uint32_t>(Num - stacked) : 0;
			if (heapAlloc) {
				indexing[maxStacked] = reinterpret_cast<Block<T, blockCapacity>*>(new Block<T, blockCapacity>*[heaped]);
				
				//Heap blocks allocation
				for (size_t i = 0; i < heaped; ++i)
				{
					getOuter()[i] = new Block<T, blockCapacity>;
				}
			}
		}

		T& operator[](size_t index) {
			const uint32_t blPos = index / blockCapacity;
			const uint32_t off = index % blockCapacity;
			if (noStack || blPos >= maxStacked)
				return heap_access(blPos-maxStacked, off);
			else
				return indexing[blPos][off];
		}

		const T& operator[](std::size_t index) const{
			const uint32_t blPos = index / blockCapacity;
			const uint32_t off = index % blockCapacity;
			if (noStack || blPos >= maxStacked)
				return heap_access(blPos - maxStacked, off);
			else
				return indexing[blPos][off];
		}

		void expand(uint32_t blocksAdded) {
			assert(static_cast<size_t>(blocksAdded) + Num <= UINT32_MAX && "Block Number overflow");
			const bool heaped = noStack || getOuter() == nullptr || Num + blocksAdded <= maxStacked;

		}

		//getters
		uint32_t BlockNum() const noexcept { return Num; }
		size_t size() const noexcept { return static_cast<size_t>(blockCapacity) * Num; }

	};

    namespace { //utility
        template<typename T>
        static constexpr const T& min_val(const T& a, const T& b) noexcept {
            return (b < a) ? b : a;
        }

        template<typename T>
        static void block_copy(T** src_begin, T** src_end, T** dest) noexcept {
            while (src_begin != src_end) {
                *dest++ = *src_begin++;
            }
        }
    }

    template<typename _T, uint32_t blockCapacity = default_block_capacity<_T>, size_t maxStacked = 16>
    class SparseVectorImpl {
        static_assert(blockCapacity > 0, "why would you think that's a good idea");

        using T = _T;
        static constexpr bool noStack = maxStacked == 0;

        // linking
        Block<T, blockCapacity>* indexing[maxStacked + 1] = {};

        inline Block<T, blockCapacity>** getOuter() const noexcept {
            return reinterpret_cast<Block<T, blockCapacity>**>(indexing[maxStacked]);
        }

        inline void setOuter(Block<T, blockCapacity>** ptr) noexcept {
            indexing[maxStacked] = reinterpret_cast<Block<T, blockCapacity>*>(ptr);
        }

        // control
        uint32_t blCap = 0;  // Total allocated blocks (stacked + heaped)
        uint32_t blSize = 0; // Logical count of active elements (in T units)

        // utility
        inline Block<T, blockCapacity>& heapBlock(uint32_t index) noexcept {
            return *(getOuter()[index]);
        }

        inline const Block<T, blockCapacity>& heapBlock(uint32_t index) const noexcept {
            return *(getOuter()[index]);
        }

        T& heapAccess(uint32_t block, uint32_t offset) noexcept {
            return heapBlock(block)[offset];
        }

        const T& heapAccess(uint32_t block, uint32_t offset) const noexcept {
            return heapBlock(block)[offset];
        }

    public:
        SparseVectorImpl(size_t initial_size = 0, const T& init = T()) {
            if (initial_size == 0) return;

            const size_t bigCap = (initial_size - 1) / blockCapacity + 1;
            assert(bigCap <= 0xFFFFFFFF && "Overflow risk, SparseArr can manage only up to 2^32 blocks");

            blCap = static_cast<uint32_t>(bigCap);
            blSize = static_cast<uint32_t>(initial_size);

            const bool hasHeap = !noStack && (blCap > maxStacked);
            const uint32_t stacked = noStack ? 0 : min_val(blCap, static_cast<uint32_t>(maxStacked));
            const uint32_t heaped = hasHeap ? (blCap - maxStacked) : (noStack ? blCap : 0);

            // Scope Guard for strong exception safety during setup
            uint32_t allocatedStacked = 0;
            uint32_t allocatedHeaped = 0;
            bool outerAllocated = false;

            auto cleanupGuard = [&]() {
                for (size_t i = 0; i < allocatedHeaped; ++i) delete getOuter()[i];
                if (outerAllocated) delete[] getOuter();
                for (size_t i = 0; i < allocatedStacked; ++i) delete indexing[i];
                };

            struct Guard {
                decltype(cleanupGuard)& cleanup;
                bool dismissed = false;
                ~Guard() { if (!dismissed) cleanup(); }
            } guard{ cleanupGuard };

            // Allocate stacked blocks
            for (uint32_t i = 0; i < stacked; ++i) {
                indexing[i] = new Block<T, blockCapacity>(init);
                allocatedStacked++;
            }

            // Allocate heap pointer array & heap blocks
            if (heaped > 0) {
                setOuter(new Block<T, blockCapacity>*[heaped]());
                outerAllocated = true;

                for (size_t i = 0; i < heaped; ++i) {
                    getOuter()[i] = new Block<T, blockCapacity>(init);
                    allocatedHeaped++;
                }
            }

            guard.dismissed = true;
        }

        ~SparseVectorImpl() {
            shrink(blCap);
        }

        // Disable raw copy
        SparseVectorImpl(const SparseVectorImpl&) = delete;
        SparseVectorImpl& operator=(const SparseVectorImpl&) = delete;

        // Move Semantics
        SparseVectorImpl(SparseVectorImpl&& other) noexcept {
            for (size_t i = 0; i <= maxStacked; ++i) {
                indexing[i] = other.indexing[i];
                other.indexing[i] = nullptr;
            }
            blCap = other.blCap;
            blSize = other.blSize;
            other.blCap = 0;
            other.blSize = 0;
        }

        T& operator[](size_t index) noexcept {
            const uint32_t blPos = static_cast<uint32_t>(index / blockCapacity);
            const uint32_t off = static_cast<uint32_t>(index % blockCapacity);
            if (!noStack && blPos < maxStacked)
                return indexing[blPos][off];
            else
                return heapAccess(blPos - (noStack ? 0 : static_cast<uint32_t>(maxStacked)), off);
        }

        const T& operator[](size_t index) const noexcept {
            const uint32_t blPos = static_cast<uint32_t>(index / blockCapacity);
            const uint32_t off = static_cast<uint32_t>(index % blockCapacity);
            if (!noStack && blPos < maxStacked)
                return indexing[blPos][off];
            else
                return heapAccess(blPos - (noStack ? 0 : static_cast<uint32_t>(maxStacked)), off);
        }

        T& at(size_t index) {
            assert(index < blSize && "Index out of bounds");
            return (*this)[index];
        }

        const T& at(size_t index) const {
            assert(index < blSize && "Index out of bounds");
            return (*this)[index];
        }

        // Increments capacity by adding 'blocksAdded' empty/default blocks
        void expand(uint32_t blocksAdded, const T& init = T()) {
            if (blocksAdded == 0) return;
            assert(static_cast<uint64_t>(blCap) + blocksAdded <= UINT32_MAX && "Block Number overflow");

            const uint32_t newBlCap = blCap + blocksAdded;
            const uint32_t oldStacked = noStack ? 0 : min_val(blCap, static_cast<uint32_t>(maxStacked));
            const uint32_t newStacked = noStack ? 0 : min_val(newBlCap, static_cast<uint32_t>(maxStacked));

            // Fill remaining stack slots first
            for (uint32_t i = oldStacked; i < newStacked; ++i) {
                indexing[i] = new Block<T, blockCapacity>(init);
            }

            const uint32_t oldHeaped = (blCap > oldStacked) ? (blCap - oldStacked) : 0;
            const uint32_t newHeaped = (newBlCap > newStacked) ? (newBlCap - newStacked) : 0;

            if (newHeaped > 0) {
                if (oldHeaped == 0) {
                    // First heap allocation: create new outer pointer table
                    setOuter(new Block<T, blockCapacity>*[newHeaped]());
                    for (size_t i = 0; i < newHeaped; ++i) {
                        getOuter()[i] = new Block<T, blockCapacity>(init);
                    }
                }
                else if (newHeaped > oldHeaped) {
                    // Reallocate outer pointer array to fit new blocks
                    auto** newOuter = new Block<T, blockCapacity>* [newHeaped]();
                    block_copy(getOuter(), getOuter() + oldHeaped, newOuter);

                    for (size_t i = oldHeaped; i < newHeaped; ++i) {
                        newOuter[i] = new Block<T, blockCapacity>(init);
                    }

                    delete[] getOuter();
                    setOuter(newOuter);
                }
            }

            blCap = newBlCap;
        }

        // Deallocates blocks starting from the back
        void shrink(uint32_t blocksRemoved) {
            if (blocksRemoved == 0 || blCap == 0) return;
            if (blocksRemoved > blCap) blocksRemoved = blCap;

            const uint32_t targetBlCap = blCap - blocksRemoved;
            const uint32_t oldStacked = noStack ? 0 : min_val(blCap, static_cast<uint32_t>(maxStacked));
            const uint32_t targetStacked = noStack ? 0 : min_val(targetBlCap, static_cast<uint32_t>(maxStacked));

            const uint32_t oldHeaped = (blCap > oldStacked) ? (blCap - oldStacked) : 0;
            const uint32_t targetHeaped = (targetBlCap > targetStacked) ? (targetBlCap - targetStacked) : 0;

            // Deallocate heap blocks
            if (oldHeaped > 0) {
                for (uint32_t i = targetHeaped; i < oldHeaped; ++i) {
                    delete getOuter()[i];
                }

                if (targetHeaped == 0) {
                    delete[] getOuter();
                    setOuter(nullptr);
                }
                else if (targetHeaped < oldHeaped) {
                    auto** newOuter = new Block<T, blockCapacity>* [targetHeaped];
                    block_copy(getOuter(), getOuter() + targetHeaped, newOuter);
                    delete[] getOuter();
                    setOuter(newOuter);
                }
            }

            // Deallocate stack blocks
            for (uint32_t i = targetStacked; i < oldStacked; ++i) {
                delete indexing[i];
                indexing[i] = nullptr;
            }

            blCap = targetBlCap;
            if (blSize > capacity()) {
                blSize = static_cast<uint32_t>(capacity());
            }
        }

        void push_back(const T* arr, uint32_t length) {
            if (length == 0) return;

            const size_t neededCapacity = static_cast<size_t>(blSize) + length;
            const size_t currentCapacity = capacity();

            if (neededCapacity > currentCapacity) {
                const size_t missingElems = neededCapacity - currentCapacity;
                const uint32_t blocksToAllocate = static_cast<uint32_t>((missingElems - 1) / blockCapacity + 1);
                expand(blocksToAllocate);
            }

            for (uint32_t i = 0; i < length; ++i) {
                (*this)[blSize++] = arr[i];
            }
        }

        void free(uint32_t elementsFreed) noexcept {
            if (elementsFreed >= blSize) {
                blSize = 0;
            }
            else {
                blSize -= elementsFreed;
            }
        }

        // Getters
        size_t capacity() const noexcept { return static_cast<size_t>(blockCapacity) * blCap; }
        size_t size() const noexcept { return static_cast<size_t>(blSize); }
        uint32_t block_count() const noexcept { return blCap; }
    };
}
template<typename T, uint32_t blockCapacity, size_t maxStacked = 16>
using SparseHeapArr = sparse::SparseArrImpl<T, blockCapacity, maxStacked>;
template<typename T, uint32_t blockCapacity = sparse::default_block_capacity<T>, size_t maxStacked = 16>
using SparseVector = sparse::SparseVectorImpl<T, blockCapacity, maxStacked>;
void alloc_test();
