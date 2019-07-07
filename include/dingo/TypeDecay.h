#pragma once

namespace dingo
{
    template < class T > struct TypeDecay : std::decay< T > {};
    template < class T > struct TypeDecay< T* > : TypeDecay< T > {};
    template < class T > struct TypeDecay< T&& > : TypeDecay< T > {};
    template < class T, class Deleter > struct TypeDecay< std::unique_ptr< T, Deleter > > : std::decay< T > {};
    template < class T, class Deleter > struct TypeDecay< std::unique_ptr< T, Deleter >& > : std::decay< T > {};
    template < class T > struct TypeDecay< std::shared_ptr< T > > : std::decay< T > {};
    template < class T > struct TypeDecay< std::shared_ptr< T >& > : std::decay< T > {};

    template < class T> using TypeDecay_t = typename TypeDecay< T >::type;
    
}