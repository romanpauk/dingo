#pragma once

namespace dingo
{
    template < class T > struct decay : std::decay< T > {};
    template < class T > struct decay< T* > : decay< T > {};
    template < class T > struct decay< T&& > : decay< T > {};
    template < class T, class Deleter > struct decay< std::unique_ptr< T, Deleter > > : std::decay< T > {};
    template < class T, class Deleter > struct decay< std::unique_ptr< T, Deleter >& > : std::decay< T > {};
    template < class T > struct decay< std::shared_ptr< T > > : std::decay< T > {};
    template < class T > struct decay< std::shared_ptr< T >& > : std::decay< T > {};

    template < class T > using decay_t = typename decay< T >::type;
    
}