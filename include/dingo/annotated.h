#pragma once

#include <utility>

namespace dingo
{
    template < typename T, typename Tag > struct annotated
    {
        annotated(T&& value): value_(std::move(value)) {}

        operator T& () & { return value_; }
        operator T&& () && { return std::move(value_); }

    private:
        T value_;
    };

    template < typename T, typename Tag > struct annotated< T&, Tag >
    {
        annotated(T& value): value_(value) {}
        operator T& () { return value_; }

    private:
        T& value_;
    };

    struct none {};

    template < typename T > struct annotated_type { using type = annotated< T, none >; };
    template < typename T, typename Tag > struct annotated_type< annotated< T, Tag > > { using type = annotated< T, Tag >; };

    template < class T > using annotated_type_t = typename annotated_type< T >::type;

    template < typename T > struct annotated_traits
    {
        using type = T;
    };

    template < typename T, typename Tag > struct annotated_traits < annotated< T, Tag > >
    {
        using type = T;
    };

    template < typename T, typename Tag > struct annotated_traits < annotated< T, Tag >& >
    {
        using type = T&;
    };

    template < typename T, typename Tag > struct annotated_traits < annotated< T, Tag >&& >
    {
        using type = T&&;
    };

    template < typename T, typename Tag > struct annotated_traits < annotated< T, Tag >* >
    {
        using type = T*;
    };

}
