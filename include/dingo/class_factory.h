#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>
#include <dingo/class_constructor.h>
#include <dingo/decay.h>

#include <type_traits>

namespace dingo {
template <class DisabledType> struct class_factory_argument {
    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<DisabledType, typename std::decay_t<T>>>>
    operator T&();

    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<DisabledType, typename std::decay_t<T>>>>
    operator T&&();
};

template <typename DisabledType, typename Context> class class_constructor_argument {
  public:
    class_constructor_argument(Context& context) : context_(context) {}

    template <typename T, typename = std::enable_if_t<!std::is_same_v<DisabledType, std::decay_t<T>>>> operator T*() {
        return context_.template resolve<T*>();
    }
    template <typename T, typename = std::enable_if_t<!std::is_same_v<DisabledType, std::decay_t<T>>>> operator T&() {
        return context_.template resolve<T&>();
    }
    template <typename T, typename = std::enable_if_t<!std::is_same_v<DisabledType, std::decay_t<T>>>> operator T&&() {
        return context_.template resolve<T&&>();
    }

    template <typename T, typename Tag, typename = std::enable_if_t<!std::is_same_v<DisabledType, std::decay_t<T>>>>
    operator annotated<T, Tag>() {
        return context_.template resolve<annotated<T, Tag>>();
    }

  private:
    Context& context_;
};
template <typename T, typename = void, typename... Args> struct is_list_constructible_impl : std::false_type {};

template <typename T, typename... Args>
struct is_list_constructible_impl<T, std::void_t<decltype(T{std::declval<Args>()...})>, Args...> : std::true_type {};

template <typename T, typename... Args> using is_list_constructible = is_list_constructible_impl<T, void, Args...>;

template <typename T, typename... Args>
static constexpr bool is_list_constructible_v = is_list_constructible<T, Args...>::value;

template <typename T, typename... Args> struct is_constructible : is_list_constructible<T, Args...> {};

// Filters out T(T&).
// TODO: It should be possible to write all into single class
template <typename T, typename Arg>
struct is_constructible<T, Arg>
    : std::conjunction<is_list_constructible<T, Arg>, std::negation<std::is_same<std::decay_t<Arg>, T>>> {};

template <typename T, typename... Args> static constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

template <typename T, bool Assert, size_t N, bool Constructible, typename... Args> struct class_factory_impl;

// Generates N arguments of type class_factory_argument<T>, going 1, 2, 3... N.
template <typename T, bool Assert = true, size_t N = DINGO_CLASS_FACTORY_ARGS, typename = void, typename... Args>
struct class_factory : class_factory<T, Assert, N, void, class_factory_argument<T>, Args...> {};

// Upon reaching N, generates N attempts to instantiate, going N, N-1, N-2... 0 arguments.
// This assures that container will select the construction method with the most arguments as
// it will be the first seen in the hierarchy (this is needed to fill default-constructible
// aggregate types with instances from container).
template <typename T, bool Assert, size_t N, typename... Args>
struct class_factory<T, Assert, N, typename std::enable_if_t<sizeof...(Args) == N>, Args...>
    : class_factory_impl<T, Assert, N, is_constructible_v<T, Args...>, std::tuple<Args...>> {};

// Construction was found. Bail out (by not inheriting anymore).
template <typename T, bool Assert, size_t N, typename... Args>
struct class_factory_impl<T, Assert, N, true, std::tuple<Args...>> {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = true;

    template <typename Type, typename Context> static Type construct(Context& ctx) {
        return class_constructor<Type>::invoke(((void)sizeof(Args), class_constructor_argument<T, Context>(ctx))...);
    }

    template <typename Type, typename Context> static void construct(void* ptr, Context& ctx) {
        class_constructor<Type>::invoke(ptr, ((void)sizeof(Args), class_constructor_argument<T, Context>(ctx))...);
    }
};

// Construction was not found. Generate next level of inheritance with one less argument.
template <typename T, bool Assert, size_t N, typename Head, typename... Tail>
struct class_factory_impl<T, Assert, N, false, std::tuple<Head, Tail...>>
    : class_factory_impl<T, Assert, N, is_constructible_v<T, Tail...>, std::tuple<Tail...>> {};

// Construction was not found, and no more arguments can be removed.
template <typename T, bool Assert, size_t N> struct class_factory_impl<T, Assert, N, false, std::tuple<>> {
    static constexpr bool valid = false;
    static_assert(!Assert || valid, "class T construction not detected or ambiguous");
};

template <typename...> struct constructor;

template <typename T, typename... Args> struct constructor<T, Args...> {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = std::is_constructible_v<T, Args...>;

    template <typename Type, typename Context> static Type construct(Context& ctx) {
        return class_constructor<Type>::invoke(ctx.template resolve<Args>()...);
    }

    template <typename Type, typename Context> static void construct(void* ptr, Context& ctx) {
        class_constructor<Type>::invoke(ptr, ctx.template resolve<Args>()...);
    }
};

template <typename T, typename... Args> struct constructor<T(Args...)> : constructor<T, Args...> {};

template <typename T> struct function_impl;

template <typename T, typename... Args> struct function_impl<T (*)(Args...)> {
    template <typename Fn, typename Context> static T construct(Fn fn, Context& ctx) {
        return fn(ctx.template resolve<Args>()...);
    }
};

template <typename T, typename... Args> struct function_impl<T(Args...)> {
    template <typename Fn, typename Context> static T construct(Fn fn, Context& ctx) {
        return fn(ctx.template resolve<Args>()...);
    }
};

template <typename R, typename T, typename... Args> struct function_impl<R (T::*)(Args...) const> {
    template <typename Fn, typename Context> static R construct(Fn fn, Context& ctx) {
        return fn(ctx.template resolve<Args>()...);
    }
};

template <typename T, T fn> struct function_decl {
    template <typename Type, typename Context> static Type construct(Context& ctx) {
        return function_impl<T>::construct(fn, ctx);
    }
};

template <auto fn> struct function : function_decl<decltype(fn), fn> {};

template <typename T> struct callable {
    callable(T fn) : fn_(fn) {}

    template <typename Type, typename Context> Type construct(Context& ctx) {
        return function_impl<decltype(&T::operator())>::construct(fn_, ctx);
    }

    template <typename Type, typename Context> void construct(void* ptr, Context& ctx) {
        // TODO
        new (ptr) decay_t<Type>(function_impl<decltype(&T::operator())>::construct(fn_, ctx));
    }

  private:
    T fn_;
};

} // namespace dingo