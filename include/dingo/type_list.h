#pragma once

// TODO: remove "Middle"
namespace dingo
{
    template < typename... Types > struct type_list {};

    template < typename T > struct type_list_iterator
    {
        typedef T type;
    };

    template < typename Function > bool for_type(type_list<>*, const std::type_info&, Function&&) { return false; }

    template < typename Function, typename Head > bool for_type(type_list< Head >*, const std::type_info& type, Function&& fn)
    {
        if (typeid(Head) == type)
        {
            fn(type_list_iterator< Head >());
            return true;
        }

        return false;
    }

    template < typename Function, typename Head, typename Middle, typename... Tail > bool for_type(type_list< Head, Middle, Tail...>*, const std::type_info& type, Function&& fn)
    {
        if (typeid(Head) == type)
        {
            fn(type_list_iterator< Head >());
            return true;
        }
        else
        {
            return for_type((type_list< Middle, Tail... >*)0, type, std::forward< Function >(fn));
        }
    }

    template < typename Function > void for_each(type_list<>, Function&&) {}

    template < typename Function, typename Head > void for_each(type_list< Head >*, Function&& fn)
    {
        fn(type_list_iterator< Head >());
    }

    template < typename Function, typename Head, typename Middle, typename... Tail > void for_each(type_list< Head, Middle, Tail...>*, Function&& fn)
    {
        fn(type_list_iterator< Head >());
        for_each((type_list< Middle, Tail... >*)0, std::forward< Function >(fn));
    }
}