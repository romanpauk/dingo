#pragma once

#include "TypeDecay.h"

#include <list>
#include <vector>
#include <set>

namespace dingo
{
    namespace detail
    {
        template < class T > struct ContainerTraits 
        {
            static const bool IsContainer = false;
        };

        template < class T, class Allocator > struct ContainerTraits< std::vector< T, Allocator > > 
        {
            static const bool IsContainer = true;
            static void Reserve(std::vector< T, Allocator >& v, size_t size) { v.reserve(size); }
            template < typename U > static void Add(std::vector< T, Allocator >& v, U&& value) { v.emplace_back(std::forward< U >(value)); }
        };

        template < class T, class Allocator > struct ContainerTraits< std::list< T, Allocator > >
        {
            static const bool IsContainer = true;
            static void Reserve(std::list< T, Allocator >& v, size_t size) {}
            template < typename U > static void Add(std::list< T, Allocator >& v, U&& value) { v.emplace_back(std::forward< U >(value)); }
        };
        
        template < class T, class Compare, class Allocator > struct ContainerTraits< std::set< T, Compare, Allocator > >
        {
            static const bool IsContainer = true;
            static void Reserve(std::set< T, Compare, Allocator >& v, size_t size) {}
            template < typename U > static void Add(std::set< T, Compare, Allocator >& v, U&& value) { v.emplace(std::forward< U >(value)); }
        };
    }

    template < class T > struct ContainerTraits: detail::ContainerTraits< TypeDecay_t< T > > {};
}
