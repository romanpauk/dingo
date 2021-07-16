#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include "dingo/Storage.h"

namespace dingo
{
    class Container;

    template < typename Type, typename U > struct Conversions< Unique, Type, U >
    {
        typedef type_list< U > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< U&& > RvalueReferenceTypes;
        typedef type_list<> PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, Type*, U >
    {
        typedef type_list< U > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< U&& > RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, std::shared_ptr< Type >, U >
    {
        typedef type_list< std::shared_ptr< U > > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< std::shared_ptr< U >&& > RvalueReferenceTypes;
        typedef type_list<> PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, std::unique_ptr< Type >, U >
    {
        typedef type_list< std::unique_ptr< U > > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< std::unique_ptr< U >&& > RvalueReferenceTypes;
        typedef type_list<> PointerTypes;
    };

    template < typename Type, typename Conversions > class Storage< Unique, Type, Conversions >
    {
    public:
        static const bool IsCaching = false;

        typedef Conversions Conversions;
        typedef Type Type;

        Type Resolve(resolving_context& context)
        {
            return class_factory< decay_t< Type > >::template construct< Type, container::ConstructorArgument< decay_t< Type > > >(context);
        }

        bool IsResolved() { return false; }
    };
}