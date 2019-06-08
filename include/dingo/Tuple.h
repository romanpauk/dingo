#pragma once

namespace dingo
{
    template < typename... Types > struct TypeList {};

    template < typename T > struct ApplyIterator
    {
        typedef T type;
    };

    template < typename Function > bool ApplyType(TypeList<>*, const std::type_info&, Function) { return false; }

    template < typename Function, typename Head > bool ApplyType(TypeList< Head >*, const std::type_info& type, Function fn)
    {
        if (typeid(Head) == type)
        {
            fn(ApplyIterator< Head >());
            return true;
        }

        return false;
    }

    template < typename Function, typename Head, typename... Tail > bool ApplyType(TypeList< Head, Tail...>*, const std::type_info& type, Function fn)
    {
        if (typeid(Head) == type)
        {
            fn(ApplyIterator< Head >());
            return true;
        }
        else
        {
            return ApplyType((TypeList< Tail... >*)0, type, fn);
        }
    }

    template < typename Function > void Apply(TypeList<>, Function) { return false; }

    template < typename Function, typename Head > void Apply(TypeList< Head >*, Function fn)
    {
        fn(ApplyIterator< Head >());
    }

    template < typename Function, typename Head, typename... Tail > void Apply(TypeList< Head, Tail...>*, Function fn)
    {
        fn(ApplyIterator< Head >());
        return Apply((TypeList< Tail... >*)0, fn);
    }
}