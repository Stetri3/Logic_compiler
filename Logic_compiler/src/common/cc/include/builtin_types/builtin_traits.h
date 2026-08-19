#pragma once
#include "CIntegers.h"

namespace cc {

    // --- Foundation: integral_constant ---

    template <typename T, T Val>
    struct integral_constant {
        static constexpr T value = Val;
        using value_type = T;
        using type = integral_constant;
        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };

    template <bool B>
    using bool_constant = integral_constant<bool, B>;

    using true_type = bool_constant<true>;
    using false_type = bool_constant<false>;


    // --- Transformation: conditional ---

    template <bool B, typename T, typename F>
    struct conditional {
        using type = T;
    };

    template <typename T, typename F>
    struct conditional<false, T, F> {
        using type = F;
    };

    template <bool B, typename T, typename F>
    using conditional_t = typename conditional<B, T, F>::type;


    // --- Transformation: remove_cv / remove_reference ---

    template <typename T> struct remove_cv { using type = T; };
    template <typename T> struct remove_cv<const T> { using type = T; };
    template <typename T> struct remove_cv<volatile T> { using type = T; };
    template <typename T> struct remove_cv<const volatile T> { using type = T; };

    template <typename T>
    using remove_cv_t = typename remove_cv<T>::type;

    template <typename T> struct remove_reference { using type = T; };
    template <typename T> struct remove_reference<T&> { using type = T; };
    template <typename T> struct remove_reference<T&&> { using type = T; };

    template <typename T>
    using remove_reference_t = typename remove_reference<T>::type;

    template <typename T>
    using remove_cvref_t = remove_cv_t<remove_reference_t<T>>;


    // --- Comparison: is_same ---

    template <typename T, typename U>
    struct is_same : false_type {};

    template <typename T>
    struct is_same<T, T> : true_type {};

    template <typename T, typename U>
    inline constexpr bool is_same_v = is_same<T, U>::value;


    // --- void trait ---

    template <typename T>
    struct is_void : false_type {};

    template <> struct is_void<void> : true_type {};

    template <typename T>
    inline constexpr bool is_void_v = is_void<remove_cv_t<T>>::value;


    // --- null_pointer trait ---

    template <typename T>
    struct is_null_pointer : false_type {};

    template <> struct is_null_pointer<decltype(nullptr)> : true_type {};

    template <typename T>
    inline constexpr bool is_null_pointer_v = is_null_pointer<remove_cv_t<T>>::value;


    // --- Integral Traits ---

    namespace detail {
        template <typename T> struct is_integral_helper : false_type {};

        template <> struct is_integral_helper<bool> : true_type {};
        template <> struct is_integral_helper<char> : true_type {};
        template <> struct is_integral_helper<signed char> : true_type {};
        template <> struct is_integral_helper<unsigned char> : true_type {};
#if defined(__cpp_char8_t)
        template <> struct is_integral_helper<char8_t> : true_type {};
#endif
        template <> struct is_integral_helper<char16_t> : true_type {};
        template <> struct is_integral_helper<char32_t> : true_type {};
        template <> struct is_integral_helper<wchar_t> : true_type {};
        template <> struct is_integral_helper<short> : true_type {};
        template <> struct is_integral_helper<unsigned short> : true_type {};
        template <> struct is_integral_helper<int> : true_type {};
        template <> struct is_integral_helper<unsigned int> : true_type {};
        template <> struct is_integral_helper<long> : true_type {};
        template <> struct is_integral_helper<unsigned long> : true_type {};
        template <> struct is_integral_helper<long long> : true_type {};
        template <> struct is_integral_helper<unsigned long long> : true_type {};
#if defined(__SIZEOF_INT128__)
        template <> struct is_integral_helper<__int128_t> : true_type {};
        template <> struct is_integral_helper<__uint128_t> : true_type {};
#endif
    } // namespace detail

    template <typename T>
    struct is_integral : detail::is_integral_helper<remove_cv_t<T>> {};

    template <typename T>
    inline constexpr bool is_integral_v = is_integral<T>::value;


    // --- Floating Point Traits ---

    namespace detail {
        template <typename T> struct is_floating_point_helper : false_type {};

        template <> struct is_floating_point_helper<float> : true_type {};
        template <> struct is_floating_point_helper<double> : true_type {};
        template <> struct is_floating_point_helper<long double> : true_type {};
#if defined(__FLOAT128__) || defined(__SIZEOF_FLOAT128__)
        template <> struct is_floating_point_helper<__float128> : true_type {};
#endif
    } // namespace detail

    template <typename T>
    struct is_floating_point : detail::is_floating_point_helper<remove_cv_t<T>> {};

    template <typename T>
    inline constexpr bool is_floating_point_v = is_floating_point<T>::value;


    // --- Arithmetic (Integral + Floating) ---

    template <typename T>
    struct is_arithmetic : bool_constant<is_integral_v<T> || is_floating_point_v<T>> {};

    template <typename T>
    inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;


    // --- Signed / Unsigned Arithmetic & Integrals ---

    template <typename T>
    struct is_signed : bool_constant<is_arithmetic_v<T> && (static_cast<remove_cv_t<T>>(-1) < static_cast<remove_cv_t<T>>(0))> {};

    template <typename T>
    inline constexpr bool is_signed_v = is_signed<T>::value;

    template <typename T>
    struct is_unsigned : bool_constant<is_arithmetic_v<T> && !is_signed_v<T> && !is_same_v<remove_cv_t<T>, bool>> {};

    template <typename T>
    inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

    template <typename T>
    struct is_signed_integral : bool_constant<is_integral_v<T>&& is_signed_v<T>> {};

    template <typename T>
    inline constexpr bool is_signed_integral_v = is_signed_integral<T>::value;

    template <typename T>
    struct is_unsigned_integral : bool_constant<is_integral_v<T> && !is_signed_v<T> && !is_same_v<remove_cv_t<T>, bool>> {};

    template <typename T>
    inline constexpr bool is_unsigned_integral_v = is_unsigned_integral<T>::value;


    // --- Sign Modifiers (make_unsigned / make_signed) ---

    namespace detail {
        template <typename T> struct make_unsigned_helper {};

        template <> struct make_unsigned_helper<signed char> { using type = unsigned char; };
        template <> struct make_unsigned_helper<unsigned char> { using type = unsigned char; };
        template <> struct make_unsigned_helper<char> { using type = unsigned char; };
        template <> struct make_unsigned_helper<short> { using type = unsigned short; };
        template <> struct make_unsigned_helper<unsigned short> { using type = unsigned short; };
        template <> struct make_unsigned_helper<int> { using type = unsigned int; };
        template <> struct make_unsigned_helper<unsigned int> { using type = unsigned int; };
        template <> struct make_unsigned_helper<long> { using type = unsigned long; };
        template <> struct make_unsigned_helper<unsigned long> { using type = unsigned long; };
        template <> struct make_unsigned_helper<long long> { using type = unsigned long long; };
        template <> struct make_unsigned_helper<unsigned long long> { using type = unsigned long long; };
#if defined(__cpp_char8_t)
        template <> struct make_unsigned_helper<char8_t> { using type = char8_t; };
#endif
        template <> struct make_unsigned_helper<char16_t> { using type = char16_t; };
        template <> struct make_unsigned_helper<char32_t> { using type = char32_t; };
        template <> struct make_unsigned_helper<wchar_t> {
            using type = conditional_t<sizeof(wchar_t) == sizeof(unsigned short), unsigned short,
                conditional_t<sizeof(wchar_t) == sizeof(unsigned int), unsigned int, unsigned long>>;
        };
#if defined(__SIZEOF_INT128__)
        template <> struct make_unsigned_helper<__int128_t> { using type = __uint128_t; };
        template <> struct make_unsigned_helper<__uint128_t> { using type = __uint128_t; };
#endif

        template <typename T> struct make_signed_helper {};

        template <> struct make_signed_helper<signed char> { using type = signed char; };
        template <> struct make_signed_helper<unsigned char> { using type = signed char; };
        template <> struct make_signed_helper<char> { using type = signed char; };
        template <> struct make_signed_helper<short> { using type = short; };
        template <> struct make_signed_helper<unsigned short> { using type = short; };
        template <> struct make_signed_helper<int> { using type = int; };
        template <> struct make_signed_helper<unsigned int> { using type = int; };
        template <> struct make_signed_helper<long> { using type = long; };
        template <> struct make_signed_helper<unsigned long> { using type = long; };
        template <> struct make_signed_helper<long long> { using type = long long; };
        template <> struct make_signed_helper<unsigned long long> { using type = long long; };
        template <> struct make_signed_helper<wchar_t> {
            using type = conditional_t<sizeof(wchar_t) == sizeof(short), short,
                conditional_t<sizeof(wchar_t) == sizeof(int), int, long>>;
        };
#if defined(__SIZEOF_INT128__)
        template <> struct make_signed_helper<__int128_t> { using type = __int128_t; };
        template <> struct make_signed_helper<__uint128_t> { using type = __int128_t; };
#endif
    } // namespace detail

    template <typename T>
    struct make_unsigned {
        using type = typename detail::make_unsigned_helper<remove_cv_t<T>>::type;
    };

    template <typename T>
    using make_unsigned_t = typename make_unsigned<T>::type;

    template <typename T>
    struct make_signed {
        using type = typename detail::make_signed_helper<remove_cv_t<T>>::type;
    };

    template <typename T>
    using make_signed_t = typename make_signed<T>::type;


    // --- Integral Min / Max Limits ---

    namespace detail {
        template <typename T>
        constexpr T compute_max_integral() noexcept {
            using CleanT = remove_cv_t<T>;
            if constexpr (is_same_v<CleanT, bool>) {
                return true;
            }
            else if constexpr (is_unsigned_integral_v<CleanT>) {
                return static_cast<CleanT>(~static_cast<CleanT>(0));
            }
            else {
                constexpr int bits = sizeof(CleanT) * 8;
                return static_cast<CleanT>((static_cast<unsigned long long>(1) << (bits - 1)) - 1);
            }
        }

        template <typename T>
        constexpr T compute_min_integral() noexcept {
            using CleanT = remove_cv_t<T>;
            if constexpr (is_same_v<CleanT, bool> || is_unsigned_integral_v<CleanT>) {
                return static_cast<CleanT>(0);
            }
            else {
                return static_cast<CleanT>(-compute_max_integral<CleanT>() - 1);
            }
        }
    } // namespace detail

    template <typename T>
        requires is_integral_v<T>
    inline constexpr T max_integral_v = detail::compute_max_integral<T>();

    template <typename T>
        requires is_integral_v<T>
    inline constexpr T min_integral_v = detail::compute_min_integral<T>();


    // --- Pointer, Reference, Array & Enum Intrinsics ---

    template <typename T> struct is_pointer_helper : false_type {};
    template <typename T> struct is_pointer_helper<T*> : true_type {};

    template <typename T>
    struct is_pointer : is_pointer_helper<remove_cv_t<T>> {};

    template <typename T>
    inline constexpr bool is_pointer_v = is_pointer<T>::value;

    template <typename T> struct is_lvalue_reference : false_type {};
    template <typename T> struct is_lvalue_reference<T&> : true_type {};

    template <typename T>
    inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

    template <typename T> struct is_rvalue_reference : false_type {};
    template <typename T> struct is_rvalue_reference<T&&> : true_type {};

    template <typename T>
    inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

    template <typename T>
    struct is_reference : bool_constant<is_lvalue_reference_v<T> || is_rvalue_reference_v<T>> {};

    template <typename T>
    inline constexpr bool is_reference_v = is_reference<T>::value;

    template <typename T> struct is_array : false_type {};
    template <typename T> struct is_array<T[]> : true_type {};
    template <typename T, decltype(sizeof(0)) N> struct is_array<T[N]> : true_type {};

    template <typename T>
    inline constexpr bool is_array_v = is_array<T>::value;

    template <typename T>
    struct is_enum : bool_constant<__is_enum(T)> {};

    template <typename T>
    inline constexpr bool is_enum_v = is_enum<T>::value;


    // --- Fundamental & Scalar Combinations ---

    template <typename T>
    struct is_fundamental : bool_constant<
        is_arithmetic_v<T> ||
        is_void_v<T> ||
        is_null_pointer_v<T>
    > {};

    template <typename T>
    inline constexpr bool is_fundamental_v = is_fundamental<T>::value;

    template <typename T>
    struct is_scalar : bool_constant<
        is_arithmetic_v<T> ||
        is_enum_v<T> ||
        is_pointer_v<T> ||
        is_null_pointer_v<T>
    > {};

    template <typename T>
    inline constexpr bool is_scalar_v = is_scalar<T>::value;


    // --- Concepts ---

    template <typename T>
    concept integral = is_integral_v<T>;

    template <typename T>
    concept signed_integral = is_signed_integral_v<T>;

    template <typename T>
    concept unsigned_integral = is_unsigned_integral_v<T>;

    template <typename T>
    concept floating_point = is_floating_point_v<T>;

    template <typename T>
    concept arithmetic = is_arithmetic_v<T>;

    template <typename T>
    concept pointer = is_pointer_v<T>;

    template <typename T>
    concept reference = is_reference_v<T>;

    template <typename T, typename U>
    concept same_as = is_same_v<T, U>;

} // namespace cc