#pragma once
#include "builtin_traits.h"

namespace cc {

    // --- Logical Conjunction / Disjunction / Negation ---

    template <typename... Bools>
    inline constexpr bool conjunction_v = (bool(Bools::value) && ...);

    template <typename... Bools>
    struct conjunction : bool_constant<conjunction_v<Bools...>> {};

    template <typename... Bools>
    inline constexpr bool disjunction_v = (bool(Bools::value) || ...);

    template <typename... Bools>
    struct disjunction : bool_constant<disjunction_v<Bools...>> {};

    template <typename B>
    struct negation : bool_constant<!bool(B::value)> {};

    template <typename B>
    inline constexpr bool negation_v = !bool(B::value);


    // --- SFINAE & Logic ---

    template <bool B, typename T = void>
    struct enable_if {};

    template <typename T>
    struct enable_if<true, T> { using type = T; };

    template <bool B, typename T = void>
    using enable_if_t = typename enable_if<B, T>::type;

    template <typename...>
    using void_t = void;


    // --- Const-Volatile Queries & Modifiers ---

    template <typename T> struct is_const : false_type {};
    template <typename T> struct is_const<const T> : true_type {};
    template <typename T> inline constexpr bool is_const_v = is_const<T>::value;

    template <typename T> struct is_volatile : false_type {};
    template <typename T> struct is_volatile<volatile T> : true_type {};
    template <typename T> inline constexpr bool is_volatile_v = is_volatile<T>::value;

    template <typename T> struct remove_const { using type = T; };
    template <typename T> struct remove_const<const T> { using type = T; };
    template <typename T> using remove_const_t = typename remove_const<T>::type;

    template <typename T> struct remove_volatile { using type = T; };
    template <typename T> struct remove_volatile<volatile T> { using type = T; };
    template <typename T> using remove_volatile_t = typename remove_volatile<T>::type;

    template <typename T> using add_const_t = const T;
    template <typename T> using add_volatile_t = volatile T;
    template <typename T> using add_cv_t = const volatile T;


    // --- Function & Member Pointers ---

    namespace detail {
        template <typename T>
        struct is_function_helper : false_type {};

        // Funzioni libere e qualificatori const/volatile/noexcept/varargs
        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...)> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...) noexcept> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...)> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...) noexcept> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...) const> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...) const noexcept> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...) volatile> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...) volatile noexcept> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...) const volatile> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args...) const volatile noexcept> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...) const> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...) const noexcept> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...) volatile> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...) volatile noexcept> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...) const volatile> : true_type {};

        template <typename Ret, typename... Args>
        struct is_function_helper<Ret(Args..., ...) const volatile noexcept> : true_type {};
    } // namespace detail

    template <typename T>
    struct is_function : detail::is_function_helper<T> {};

    template <typename T>
    inline constexpr bool is_function_v = is_function<T>::value;

    template <typename T> struct is_member_pointer_helper : false_type {};
    template <typename T, typename U> struct is_member_pointer_helper<T U::*> : true_type {};

    template <typename T>
    struct is_member_pointer : is_member_pointer_helper<remove_cv_t<T>> {};

    template <typename T>
    inline constexpr bool is_member_pointer_v = is_member_pointer<T>::value;


    // --- Reference Modifiers ---

    template <typename T>
    struct add_lvalue_reference {
        using type = conditional_t<is_void_v<T>, void, T&>;
    };
    template <typename T>
    using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

    template <typename T>
    struct add_rvalue_reference {
        using type = conditional_t<is_void_v<T>, void, T&&>;
    };
    template <typename T>
    using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;


    // --- Pointer & Array Modifiers ---

    template <typename T> struct remove_pointer { using type = T; };
    template <typename T> struct remove_pointer<T*> { using type = T; };
    template <typename T> struct remove_pointer<T* const> { using type = T; };
    template <typename T> struct remove_pointer<T* volatile> { using type = T; };
    template <typename T> struct remove_pointer<T* const volatile> { using type = T; };
    template <typename T> using remove_pointer_t = typename remove_pointer<T>::type;

    template <typename T>
    struct add_pointer {
        using type = conditional_t<
            is_void_v<T>,
            void*,
            conditional_t<is_reference_v<T>, remove_reference_t<T>*, T*>
        >;
    };
    template <typename T>
    using add_pointer_t = typename add_pointer<T>::type;

    template <typename T> struct remove_extent { using type = T; };
    template <typename T> struct remove_extent<T[]> { using type = T; };
    template <typename T, decltype(sizeof(0)) N> struct remove_extent<T[N]> { using type = T; };
    template <typename T> using remove_extent_t = typename remove_extent<T>::type;

    template <typename T> struct remove_all_extents { using type = T; };
    template <typename T> struct remove_all_extents<T[]> { using type = typename remove_all_extents<T>::type; };
    template <typename T, decltype(sizeof(0)) N> struct remove_all_extents<T[N]> { using type = typename remove_all_extents<T>::type; };
    template <typename T> using remove_all_extents_t = typename remove_all_extents<T>::type;

    template <typename T, unsigned N = 0>
    struct extent : integral_constant<decltype(sizeof(0)), 0> {};

    template <typename T>
    struct extent<T[], 0> : integral_constant<decltype(sizeof(0)), 0> {};

    template <typename T, unsigned N>
    struct extent<T[], N> : extent<T, N - 1> {};

    template <typename T, decltype(sizeof(0)) I>
    struct extent<T[I], 0> : integral_constant<decltype(sizeof(0)), I> {};

    template <typename T, decltype(sizeof(0)) I, unsigned N>
    struct extent<T[I], N> : extent<T, N - 1> {};

    template <typename T, unsigned N = 0>
    inline constexpr decltype(sizeof(0)) extent_v = extent<T, N>::value;


    // --- Decay ---

    template <typename T>
    struct decay {
    private:
        using U = remove_reference_t<T>;
    public:
        using type = conditional_t<
            is_array_v<U>,
            remove_extent_t<U>*,
            conditional_t<
            is_function_v<U>,
            add_pointer_t<U>,
            remove_cv_t<U>
            >
        >;
    };

    template <typename T>
    using decay_t = typename decay<T>::type;


    // --- Type Categories & Relations (Compiler Builtins) ---

    template <typename T> struct is_union : bool_constant<__is_union(T)> {};
    template <typename T> inline constexpr bool is_union_v = __is_union(T);

    template <typename T> struct is_class : bool_constant<__is_class(T)> {};
    template <typename T> inline constexpr bool is_class_v = __is_class(T);

    template <typename T> struct is_empty : bool_constant<__is_empty(T)> {};
    template <typename T> inline constexpr bool is_empty_v = __is_empty(T);

    template <typename T> struct is_final : bool_constant<__is_final(T)> {};
    template <typename T> inline constexpr bool is_final_v = __is_final(T);

    template <typename T> struct is_standard_layout : bool_constant<__is_standard_layout(T)> {};
    template <typename T> inline constexpr bool is_standard_layout_v = __is_standard_layout(T);

    template <typename Base, typename Derived>
    struct is_base_of : bool_constant<__is_base_of(Base, Derived)> {};
    template <typename Base, typename Derived>
    inline constexpr bool is_base_of_v = __is_base_of(Base, Derived);


    // --- Triviality ---

    template <typename T> struct is_trivial : bool_constant<__is_trivial(T)> {};
    template <typename T> inline constexpr bool is_trivial_v = __is_trivial(T);

    template <typename T> struct is_trivially_copyable : bool_constant<__is_trivially_copyable(T)> {};
    template <typename T> inline constexpr bool is_trivially_copyable_v = __is_trivially_copyable(T);

    template <typename T> struct is_trivially_destructible : bool_constant<__is_trivially_destructible(T)> {};
    template <typename T> inline constexpr bool is_trivially_destructible_v = __is_trivially_destructible(T);

    template <typename T, typename... Args>
    struct is_trivially_constructible : bool_constant<__is_trivially_constructible(T, Args...)> {};
    template <typename T, typename... Args>
    inline constexpr bool is_trivially_constructible_v = __is_trivially_constructible(T, Args...);

    template <typename T>
    inline constexpr bool is_trivially_default_constructible_v = __is_trivially_constructible(T);

    template <typename T>
    inline constexpr bool is_trivially_copy_constructible_v = __is_trivially_constructible(T, add_lvalue_reference_t<const T>);

    template <typename T>
    inline constexpr bool is_trivially_move_constructible_v = __is_trivially_constructible(T, add_rvalue_reference_t<T>);

    template <typename T, typename U>
    struct is_trivially_assignable : bool_constant<__is_trivially_assignable(T, U)> {};
    template <typename T, typename U>
    inline constexpr bool is_trivially_assignable_v = __is_trivially_assignable(T, U);

    template <typename T>
    inline constexpr bool is_trivially_copy_assignable_v = __is_trivially_assignable(add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>);

    template <typename T>
    inline constexpr bool is_trivially_move_assignable_v = __is_trivially_assignable(add_lvalue_reference_t<T>, add_rvalue_reference_t<T>);


    // --- Constructibility & Assignability ---

    template <typename T, typename... Args>
    struct is_constructible : bool_constant<__is_constructible(T, Args...)> {};
    template <typename T, typename... Args>
    inline constexpr bool is_constructible_v = __is_constructible(T, Args...);

    template <typename T>
    inline constexpr bool is_default_constructible_v = __is_constructible(T);

    template <typename T>
    inline constexpr bool is_copy_constructible_v = __is_constructible(T, add_lvalue_reference_t<const T>);

    template <typename T>
    inline constexpr bool is_move_constructible_v = __is_constructible(T, add_rvalue_reference_t<T>);

    template <typename T, typename U>
    struct is_assignable : bool_constant<__is_assignable(T, U)> {};
    template <typename T, typename U>
    inline constexpr bool is_assignable_v = __is_assignable(T, U);

    template <typename T>
    inline constexpr bool is_copy_assignable_v = __is_assignable(add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>);

    template <typename T>
    inline constexpr bool is_move_assignable_v = __is_assignable(add_lvalue_reference_t<T>, add_rvalue_reference_t<T>);

    template <typename T>
    struct is_destructible : bool_constant<__is_destructible(T)> {};
    template <typename T>
    inline constexpr bool is_destructible_v = __is_destructible(T);


    // --- Nothrow Operations ---

    template <typename T, typename... Args>
    struct is_nothrow_constructible : bool_constant<__is_nothrow_constructible(T, Args...)> {};
    template <typename T, typename... Args>
    inline constexpr bool is_nothrow_constructible_v = __is_nothrow_constructible(T, Args...);

    template <typename T>
    inline constexpr bool is_nothrow_default_constructible_v = __is_nothrow_constructible(T);

    template <typename T>
    inline constexpr bool is_nothrow_copy_constructible_v = __is_nothrow_constructible(T, add_lvalue_reference_t<const T>);

    template <typename T>
    inline constexpr bool is_nothrow_move_constructible_v = __is_nothrow_constructible(T, add_rvalue_reference_t<T>);

    template <typename T, typename U>
    struct is_nothrow_assignable : bool_constant<__is_nothrow_assignable(T, U)> {};
    template <typename T, typename U>
    inline constexpr bool is_nothrow_assignable_v = __is_nothrow_assignable(T, U);

    template <typename T>
    inline constexpr bool is_nothrow_copy_assignable_v = __is_nothrow_assignable(add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>);

    template <typename T>
    inline constexpr bool is_nothrow_move_assignable_v = __is_nothrow_assignable(add_lvalue_reference_t<T>, add_rvalue_reference_t<T>);

    template <typename T>
    struct is_nothrow_destructible : bool_constant<__is_nothrow_destructible(T)> {};
    template <typename T>
    inline constexpr bool is_nothrow_destructible_v = __is_nothrow_destructible(T);


    // --- Utility Functions ---

    template <typename T>
    add_rvalue_reference_t<T> declval() noexcept;

} // namespace cc