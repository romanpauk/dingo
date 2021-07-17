#pragma once

#include <memory>

namespace dingo
{
    struct runtime_type {};

    template < class T, class U > struct rebind_type { typedef U type; };
    template < class T, class U > struct rebind_type< T&, U > { typedef U& type; };
    template < class T, class U > struct rebind_type< T&&, U > { typedef U&& type; };
    template < class T, class U > struct rebind_type< T*, U > { typedef U* type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >, U > { typedef std::shared_ptr< U > type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >&, U > { typedef std::shared_ptr< U >& type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >&&, U > { typedef std::shared_ptr< U >&& type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >*, U > { typedef std::shared_ptr< U >* type; };

    // TODO: how to rebind those properly?
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >, U > { typedef std::unique_ptr< U, std::default_delete< U > > type; };
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >&, U > { typedef std::unique_ptr< U, std::default_delete< U > >& type; };
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >*, U > { typedef std::unique_ptr< U, std::default_delete< U > >* type; };
}