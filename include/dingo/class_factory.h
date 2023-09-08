#pragma once

#include <dingo/config.h>

#include <dingo/class_constructor.h>

#include <type_traits>

namespace dingo {
template <class DisabledType> struct class_factory_argument {
    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<DisabledType, typename std::decay_t<T>>>>
    operator T&();

    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<DisabledType, typename std::decay_t<T>>>>
    operator T&&();
};

template <typename T, typename = void, typename... Args> struct is_list_constructible_impl : std::false_type {};

template <typename T, typename... Args>
struct is_list_constructible_impl<T, std::void_t<decltype(T{std::declval<Args>()...})>, Args...> : std::true_type {};

template <typename T, typename... Args> using is_list_constructible = is_list_constructible_impl<T, void, Args...>;

template <typename T, typename... Args>
constexpr bool is_list_constructible_v = is_list_constructible<T, Args...>::value;

template <typename T, typename... Args>
using is_aggregate_constructible = std::conjunction<
    std::is_aggregate<T>, is_list_constructible<T, Args...>,
    std::negation<std::conjunction<std::bool_constant<sizeof...(Args) == 1>,
                                   std::is_same<std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>, T>>>>;

template <typename T, typename... Args>
constexpr bool is_aggregate_constructible_v = is_aggregate_constructible<T, Args...>::value;

//
// Two ways how to instantiate the classes:
// 1) conservative mode - narrow -  will try T{}, T{arg1}, ... T{arg1, arg2, ... argN}
//      Compiles nice and fast, yet does not allow for initializing unmanaged default-initializable
//      structs from the container.
// 2) non-conservative mode - wide - will try T{argN, argN - 1, ... arg1}, T{argN - 1, ... arg1}, ... T{}
//      Upon finding constructor, it will try T{}, T{arg1}... etc using narrow factory.
//      The constructor is unambiguous when narrow and wide arities determined by respective factories
//      are equal.
//      TODO: this is stupid, we can use narrow and simply not terminate the search and just go up:
//          If we find more than one ctor, we are done, otherwise if we found one, we are done, otherwise
//          widest aggregate initializer will be used.
//
//  Args... is growing through recursion until a specialization with is_list_constructible
//  equals true is hit or a limit is reached.
template <typename T, size_t N = DINGO_CLASS_FACTORY_ARGS, typename = void, typename... Args>
struct class_factory_narrow : class_factory_narrow<T, N, void, class_factory_argument<T>, Args...> {};

template <typename T, size_t N, typename... Args>
struct class_factory_narrow<T, N, typename std::enable_if_t<is_list_constructible_v<T, Args...> && sizeof...(Args) < N>,
                            Args...> {
    static constexpr size_t arity = sizeof...(Args);

    template <typename Type, typename Arg, typename Context> static Type construct(Context& ctx) {
        return class_constructor<Type>::invoke(((void)sizeof(Args), Arg(ctx))...);
    }

    template <typename Type, typename Arg, typename Context> static void construct(void* ptr, Context& ctx) {
        class_constructor<Type>::invoke(ptr, ((void)sizeof(Args), Arg(ctx))...);
    }
};

template <typename T, size_t N, typename... Args>
struct class_factory_narrow<T, N, typename std::enable_if_t<sizeof...(Args) == N>, Args...> {
    static_assert(true, "constructor was not detected");
};

// Filters out T(T&).
// TODO: It should be possible to write all into single class
template <typename T, typename... Args> struct is_constructible : is_list_constructible<T, Args...> {};

template <typename T, typename Arg>
struct is_constructible<T, Arg>
    : std::conjunction<is_list_constructible<T, Arg>, std::negation<std::is_same<std::decay_t<Arg>, T>>> {};

template <typename T, typename... Args> constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

template <typename T, bool Assert, size_t N, bool Constructible, typename... Args> struct class_factory_wide_impl;

// Generates N arguments of type class_factory_argument<T>, going 1, 2, 3... N.
template <typename T, bool Assert = true, size_t N = DINGO_CLASS_FACTORY_ARGS, typename = void, typename... Args>
struct class_factory_wide : class_factory_wide<T, Assert, N, void, class_factory_argument<T>, Args...> {};

// Upon reaching N, generates N attempts to instantiate, going N, N-1, N-2... 0 arguments.
// This assures that container will select the construction method with the most arguments as
// it will be the first seen in the hierarchy (this is needed to fill default-constructible
// aggregate types with instances from container).
template <typename T, bool Assert, size_t N, typename... Args>
struct class_factory_wide<T, Assert, N, typename std::enable_if_t<sizeof...(Args) == N>, Args...>
    : class_factory_wide_impl<T, Assert, N, is_constructible_v<T, Args...>, std::tuple<Args...>> {};

// Construction was found. Bail out (by not inheriting anymore).
template <typename T, bool Assert, size_t N, typename... Args>
struct class_factory_wide_impl<T, Assert, N, true, std::tuple<Args...>> {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = true;

    template <typename Type, typename Arg, typename Context> static Type construct(Context& ctx) {
        return class_constructor<Type>::invoke(((void)sizeof(Args), Arg(ctx))...);
    }

    template <typename Type, typename Arg, typename Context> static void construct(void* ptr, Context& ctx) {
        class_constructor<Type>::invoke(ptr, ((void)sizeof(Args), Arg(ctx))...);
    }
};

// Construction was not found. Generate next level of inheritance with one less argument.
template <typename T, bool Assert, size_t N, typename Head, typename... Tail>
struct class_factory_wide_impl<T, Assert, N, false, std::tuple<Head, Tail...>>
    : class_factory_wide_impl<T, Assert, N, is_constructible_v<T, Tail...>, std::tuple<Tail...>> {
    static constexpr bool valid =
        class_factory_wide_impl<T, Assert, N, is_constructible_v<T, Tail...>, std::tuple<Tail...>>::valid;
};

// Construction was not found, and no more arguments can be removed.
template <typename T, bool Assert, size_t N> struct class_factory_wide_impl<T, Assert, N, false, std::tuple<>> {
    static constexpr bool valid = false;
};

#if (DINGO_CLASS_FACTORY_CONSERVATIVE == 1)
template <typename T, typename... Args>
using class_factory = class_factory_narrow<T, DINGO_CLASS_FACTORY_ARGS, Args...>;
#else
template <typename T, typename... Args>
using class_factory = class_factory_wide<T, true, DINGO_CLASS_FACTORY_ARGS, Args...>;
#endif

} // namespace dingo