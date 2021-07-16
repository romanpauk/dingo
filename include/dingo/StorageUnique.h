#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include "dingo/Storage.h"

namespace dingo
{
    class Container;

    template < typename Type, typename U > struct Conversions< Unique, Type, U >
    {
        typedef TypeList< U > ValueTypes;
        typedef TypeList<> LvalueReferenceTypes;
        typedef TypeList< U&& > RvalueReferenceTypes;
        typedef TypeList<> PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, Type*, U >
    {
        typedef TypeList< U > ValueTypes;
        typedef TypeList<> LvalueReferenceTypes;
        typedef TypeList< U&& > RvalueReferenceTypes;
        typedef TypeList< U* > PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, std::shared_ptr< Type >, U >
    {
        typedef TypeList< std::shared_ptr< U > > ValueTypes;
        typedef TypeList<> LvalueReferenceTypes;
        typedef TypeList< std::shared_ptr< U >&& > RvalueReferenceTypes;
        typedef TypeList<> PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, std::unique_ptr< Type >, U >
    {
        typedef TypeList< std::unique_ptr< U > > ValueTypes;
        typedef TypeList<> LvalueReferenceTypes;
        typedef TypeList< std::unique_ptr< U >&& > RvalueReferenceTypes;
        typedef TypeList<> PointerTypes;
    };

    template < typename Type, typename Conversions > class Storage< Unique, Type, Conversions >
    {
    public:
        static const bool IsCaching = false;

        typedef Conversions Conversions;
        typedef Type Type;

        Type Resolve(Context& context)
        {
            return class_factory< decay_t< Type > >::template construct< Type, Container::ConstructorArgument< decay_t< Type > > >(context);
        }

        bool IsResolved() { return false; }
    };
}