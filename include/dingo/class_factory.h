#pragma once

#include <type_traits>

#include <dingo/class_constructor.h>

namespace dingo
{
    template < class DisabledType > struct class_factory_argument
    {
        template < typename T, typename = typename std::enable_if_t< !std::is_same_v< DisabledType, typename std::decay_t< T > > > >
        operator T& ();

        template < typename T, typename = typename std::enable_if_t< !std::is_same_v< DisabledType, typename std::decay_t< T > > > >
        operator T && ();
    };

    template <typename T, size_t N = 120, typename = void, typename... Args>
    struct class_factory : class_factory<T, N, void, class_factory_argument<T>, Args...>
    {};

    template <typename T, size_t N, typename... Args>
    struct class_factory < T, N, typename std::enable_if_t< std::is_constructible_v<T, Args...> && sizeof...(Args) < N > , Args... >
    {
        static const size_t arity = sizeof...(Args);

        template < typename Type, typename Arg, typename Context > static Type construct(Context& ctx)
        {
            return class_constructor< Type >::invoke((sizeof(Args), Arg(ctx))...);
        }

        template < typename Type, typename Arg, typename Context > static void construct(Context& ctx, void* ptr)
        {
            class_constructor< Type >::invoke(ptr, (sizeof(Args), Arg(ctx))...);
        }
    };

    template <typename T, size_t N, typename... Args>
    struct class_factory<T, N, typename std::enable_if_t<sizeof...(Args) == N>, Args...>
    {
        static_assert(true, "constructor was not detected");
    };
}