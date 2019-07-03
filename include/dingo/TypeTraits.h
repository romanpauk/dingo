#pragma once

#include "TypeDecay.h"

#include <list>
#include <vector>
#include <set>

namespace dingo
{
    namespace detail
    {
        template < class T > struct IsContainer { static const bool value = false; };
        template < class T, class Allocator > struct IsContainer< std::vector< T, Allocator > > { static const bool value = true; };
        template < class T, class Allocator > struct IsContainer< std::list< T, Allocator > > { static const bool value = true; };
        template < class T, class Compare, class Allocator > struct IsContainer< std::set< T, Compare, Allocator > > { static const bool value = true; };

        template < class T > struct ContainerTraits {};

        template < class T, class Allocator > struct ContainerTraits< std::vector< T, Allocator > > 
        {
            static void Reserve(std::vector< T, Allocator >& v, size_t size) { v.reserve(size); }
            static void Add(std::vector< T, Allocator >& v, T&& value) { v.emplace_back(std::forward< T >(value)); }
        };

        template < class T, class Allocator > struct ContainerTraits< std::list< T, Allocator > >
        {
            static void Reserve(std::list< T, Allocator >& v, size_t size) {}
            static void Add(std::list< T, Allocator >& v, T&& value) { v.emplace_back(std::forward< T >(value)); }
        };
        
        template < class T, class Compare, class Allocator > struct ContainerTraits< std::set< T, Compare, Allocator > >
        {
            static void Reserve(std::set< T, Compare, Allocator >& v, size_t size) {}
            static void Add(std::set< T, Compare, Allocator >& v, T&& value) { v.emplace(std::forward< T >(value)); }
        };
    }

    template < class T > struct IsContainer : detail::IsContainer< typename TypeDecay< T >::type > {};
    template < class T > struct ContainerTraits: detail::ContainerTraits< typename TypeDecay< T >::type > {};
}
