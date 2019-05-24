#pragma once

namespace dingo {
    template < class T > struct ApplyIterator
    {
        typedef T type;
    };

    template < typename Function > bool ApplyType(std::tuple<>*, const std::type_info&, Function) { return false; }

    template < typename Function, typename Head > bool ApplyType(std::tuple< Head >*, const std::type_info& type, Function fn)
    {
        if (typeid(Head) == type)
        {
            fn(ApplyIterator< Head >());
            return true;
        }

        return false;
    }

    template < typename Function, typename Head, typename... Tail > bool ApplyType(std::tuple< Head, Tail...>*, const std::type_info& type, Function fn)
    {
        if (typeid(Head) == type)
        {
            fn(ApplyIterator< Head >());
            return true;
        }
        else
        {
            return ApplyType((std::tuple< Tail... >*)0, type, fn);
        }
    }

    template < typename Function > void Apply(std::tuple<>*, Function) { return false; }

    template < typename Function, typename Head > void Apply(std::tuple< Head >*, Function fn)
    {
        fn(ApplyIterator< Head >());
    }

    template < typename Function, typename Head, typename... Tail > void Apply(std::tuple< Head, Tail...>*, Function fn)
    {
        fn(ApplyIterator< Head >());
        return Apply((std::tuple< Tail... >*)0, fn);
    }

    template <typename T, typename... Args> struct Contains;
    template <typename T> struct Contains<std::tuple<>, T > : std::false_type {};
    template <typename T, typename... Args> struct Contains<std::tuple< T, Args... >, T > : std::true_type {};
    template <typename T, typename A, typename... Args> struct Contains<std::tuple< A, Args...>, T > : Contains<std::tuple< Args... >, T > {};

    template <typename A, typename B> struct Concat { typedef decltype(std::tuple_cat(std::declval< A >(), std::declval< B >())) type; };

}