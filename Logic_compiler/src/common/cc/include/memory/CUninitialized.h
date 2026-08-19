#pragma once
#include "CIntegers.h"
#include "CType_traits.h"
#include "CNew.h"

namespace cc {

    // --- Uninitialized Copy ---

    template <typename InputIt, typename ForwardIt, unsigned_integral SizeType>
    constexpr ForwardIt uninitialized_copy_n(InputIt first, SizeType count, ForwardIt d_first) {
        using ValueType = remove_cvref_t<decltype(*d_first)>;
        if constexpr (pointer<InputIt> && pointer<ForwardIt> && is_trivially_copyable_v<ValueType>) {
            copy_memory(d_first, first, count);
            return d_first + count;
        }
        else {
            ForwardIt current = d_first;
            for (SizeType i = 0; i < count; ++i) {
                construct_at(&*current, *first);
                ++first;
                ++current;
            }
            return current;
        }
    }

    template <typename InputIt, typename ForwardIt>
    constexpr ForwardIt uninitialized_copy(InputIt first, InputIt last, ForwardIt d_first) {
        using ValueType = remove_cvref_t<decltype(*d_first)>;
        if constexpr (pointer<InputIt> && pointer<ForwardIt> && is_trivially_copyable_v<ValueType>) {
            auto count = static_cast<decltype(sizeof(0))>(last - first);
            copy_memory(d_first, first, count);
            return d_first + count;
        }
        else {
            ForwardIt current = d_first;
            for (; first != last; ++first, ++current) {
                construct_at(&*current, *first);
            }
            return current;
        }
    }


    // --- Uninitialized Move ---

    template <typename InputIt, typename ForwardIt, unsigned_integral SizeType>
    constexpr ForwardIt uninitialized_move_n(InputIt first, SizeType count, ForwardIt d_first) {
        using ValueType = remove_cvref_t<decltype(*d_first)>;
        if constexpr (pointer<InputIt> && pointer<ForwardIt> && is_trivially_copyable_v<ValueType>) {
            copy_memory(d_first, first, count);
            return d_first + count;
        }
        else {
            ForwardIt current = d_first;
            for (SizeType i = 0; i < count; ++i) {
                construct_at(&*current, move(*first));
                ++first;
                ++current;
            }
            return current;
        }
    }

    template <typename InputIt, typename ForwardIt>
    constexpr ForwardIt uninitialized_move(InputIt first, InputIt last, ForwardIt d_first) {
        using ValueType = remove_cvref_t<decltype(*d_first)>;
        if constexpr (pointer<InputIt> && pointer<ForwardIt> && is_trivially_copyable_v<ValueType>) {
            auto count = static_cast<decltype(sizeof(0))>(last - first);
            copy_memory(d_first, first, count);
            return d_first + count;
        }
        else {
            ForwardIt current = d_first;
            for (; first != last; ++first, ++current) {
                construct_at(&*current, move(*first));
            }
            return current;
        }
    }


    // --- Uninitialized Fill / Construction ---

    template <typename ForwardIt, unsigned_integral SizeType, typename T>
    constexpr ForwardIt uninitialized_fill_n(ForwardIt first, SizeType count, const T& value) {
        ForwardIt current = first;
        for (SizeType i = 0; i < count; ++i) {
            construct_at(&*current, value);
            ++current;
        }
        return current;
    }

    template <typename ForwardIt, unsigned_integral SizeType>
    constexpr ForwardIt uninitialized_default_construct_n(ForwardIt first, SizeType count) {
        using ValueType = remove_cvref_t<decltype(*first)>;
        if constexpr (!is_trivially_default_constructible_v<ValueType>) {
            ForwardIt current = first;
            for (SizeType i = 0; i < count; ++i) {
                construct_at(&*current);
                ++current;
            }
            return current;
        }
        return first + count;
    }

    template <typename ForwardIt, unsigned_integral SizeType>
    constexpr ForwardIt uninitialized_value_construct_n(ForwardIt first, SizeType count) {
        using ValueType = remove_cvref_t<decltype(*first)>;
        if constexpr (pointer<ForwardIt> && is_scalar_v<ValueType>) {
            zero_memory(first, count);
            return first + count;
        }
        else {
            ForwardIt current = first;
            for (SizeType i = 0; i < count; ++i) {
                construct_at(&*current, ValueType{});
                ++current;
            }
            return current;
        }
    }


    // --- Destructive Move / Relocation ---

    template <typename T, unsigned_integral SizeType>
    constexpr void uninitialized_relocate_n(T* src, SizeType count, T* dst) noexcept {
        if (count == 0 || src == dst) return;

        if constexpr (is_trivially_copyable_v<T>) {
            move_memory(dst, src, count);
        }
        else {
            if (dst < src) {
                for (SizeType i = 0; i < count; ++i) {
                    construct_at(dst + i, move(src[i]));
                    destroy_at(src + i);
                }
            }
            else {
                for (SizeType i = count; i > 0; --i) {
                    construct_at(dst + (i - 1), move(src[i - 1]));
                    destroy_at(src + (i - 1));
                }
            }
        }
    }

} // namespace cc