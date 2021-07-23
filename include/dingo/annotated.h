#pragma once

namespace dingo
{
    template < typename T, typename Tag > struct annotated: public T {};
    
    struct none {};

    template < typename T > struct annotated_type { using type = annotated< T, none >; };
    template < typename T, typename Tag > struct annotated_type< annotated< T, Tag > > { using type = annotated< T, Tag >; };

    template < class T > using annotated_type_t = typename annotated_type< T >::type;

    template < typename T > struct unannotated_type { using type = T; };
    template < typename T, typename Tag > struct unannotated_type< annotated< T, Tag > > { using type = T; };
    template < typename T, typename Tag > struct unannotated_type< annotated< T, Tag >& > { using type = T&; };
    // template < typename T, typename Tag > struct unannotated_type< annotated< T, Tag >&& > { using type = T; };

    template < class T > using unannotated_type_t = typename unannotated_type< T >::type;
}
