#pragma once
#include "dingo/TypeConstructor.h"

namespace dingo
{
    template < class DisabledType > struct TypeArgument
    {
        template < typename T, typename = typename std::enable_if_t< !std::is_same_v< DisabledType, typename std::decay_t< T > > > >
        operator T& ();

        template < typename T, typename = typename std::enable_if_t< !std::is_same_v< DisabledType, typename std::decay_t< T > > > >
        operator T && ();
    };

    template <typename T, size_t N = 120, typename = void, typename... Args>
    struct TypeFactory : TypeFactory<T, N, void, TypeArgument<T>, Args...>
    {};

    template <typename T, size_t N, typename... Args>
    struct TypeFactory < T, N, typename std::enable_if_t< std::is_constructible_v<T, Args...> && sizeof...(Args) < N > , Args... >
    {
        static const size_t arity = sizeof...(Args);

        template < typename Type, typename Arg, typename Context > static Type Construct(Context& ctx)
        {
            return TypeConstructor< Type >::Construct((sizeof(Args), Arg(ctx))...);
        }

        template < typename Type, typename Arg, typename Context > static Type Construct(Context& ctx, void* ptr)
        {
            return TypeConstructor< Type >::Construct(ptr, (sizeof(Args), Arg(ctx))...);
        }
    };

    template <typename T, size_t N, typename... Args>
    struct TypeFactory<T, N, typename std::enable_if_t<sizeof...(Args) == N>, Args...>
    {
        static_assert(true, "constructor was not detected");
    };
}