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

template <typename T, typename... Args> struct is_constructible : is_list_constructible<T, Args...> {};

// Filters out T(T&).
// TODO: It should be possible to write all into single class
template <typename T, typename Arg>
struct is_constructible<T, Arg>
    : std::conjunction<is_list_constructible<T, Arg>, std::negation<std::is_same<std::decay_t<Arg>, T>>> {};

template <typename T, typename... Args> constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

template <typename T, typename Arg, bool Assert, size_t N, bool Constructible, typename... Args>
struct class_factory_impl;

// Generates N arguments of type class_factory_argument<T>, going 1, 2, 3... N.
template <typename T, typename Arg, bool Assert = true, size_t N = DINGO_CLASS_FACTORY_ARGS, typename = void,
          typename... Args>
struct class_factory : class_factory<T, Arg, Assert, N, void, class_factory_argument<T>, Args...> {};

// Upon reaching N, generates N attempts to instantiate, going N, N-1, N-2... 0 arguments.
// This assures that container will select the construction method with the most arguments as
// it will be the first seen in the hierarchy (this is needed to fill default-constructible
// aggregate types with instances from container).
template <typename T, typename Arg, bool Assert, size_t N, typename... Args>
struct class_factory<T, Arg, Assert, N, typename std::enable_if_t<sizeof...(Args) == N>, Args...>
    : class_factory_impl<T, Arg, Assert, N, is_constructible_v<T, Args...>, std::tuple<Args...>> {};

// Construction was found. Bail out (by not inheriting anymore).
template <typename T, typename Arg, bool Assert, size_t N, typename... Args>
struct class_factory_impl<T, Arg, Assert, N, true, std::tuple<Args...>> {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = true;

    template <typename Type, typename Context> static Type construct(Context& ctx) {
        return class_constructor<Type>::invoke(((void)sizeof(Args), Arg(ctx))...);
    }

    template <typename Type, typename Context> static void construct(void* ptr, Context& ctx) {
        class_constructor<Type>::invoke(ptr, ((void)sizeof(Args), Arg(ctx))...);
    }
};

// Construction was not found. Generate next level of inheritance with one less argument.
template <typename T, typename Arg, bool Assert, size_t N, typename Head, typename... Tail>
struct class_factory_impl<T, Arg, Assert, N, false, std::tuple<Head, Tail...>>
    : class_factory_impl<T, Arg, Assert, N, is_constructible_v<T, Tail...>, std::tuple<Tail...>> {};

// Construction was not found, and no more arguments can be removed.
template <typename T, typename Arg, bool Assert, size_t N>
struct class_factory_impl<T, Arg, Assert, N, false, std::tuple<>> {
    static constexpr bool valid = false;
    static_assert(!Assert || valid, "class T construction not detected or ambiguous");
};

template <typename T, typename... Args> struct constructor {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = std::is_constructible_v<T, Args...>;

    template <typename Type, typename Context> static Type construct(Context& ctx) {
        return class_constructor<Type>::invoke(ctx.template resolve<Args>()...);
    }

    template <typename Type, typename Context> static void construct(void* ptr, Context& ctx) {
        class_constructor<Type>::invoke(ptr, ctx.template resolve<Args>()...);
    }
};

} // namespace dingo