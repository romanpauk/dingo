#pragma once

#include <memory>

namespace dingo
{
    struct runtime_type {};

    template < class T, class U > struct rebind_type { using type = U; };
    template < class T, class U > struct rebind_type< T&, U > { using type = U&; };
    template < class T, class U > struct rebind_type< T&&, U > { using type = U&&; };
    template < class T, class U > struct rebind_type< T*, U > { using type = U*; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >, U > { using type = std::shared_ptr< U >; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >&, U > { using type = std::shared_ptr< U >&; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >&&, U > { using type = std::shared_ptr< U >&&; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >*, U > { using type = std::shared_ptr< U >*; };

    // TODO: how to rebind those properly?
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >, U > { using type = std::unique_ptr< U, std::default_delete< U > >; };
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >&, U > { using type = std::unique_ptr< U, std::default_delete< U > >&; };
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >*, U > { using type = std::unique_ptr< U, std::default_delete< U > >*; };

    template < typename T, typename U > using rebind_type_t = typename rebind_type< T, U >::type;
}