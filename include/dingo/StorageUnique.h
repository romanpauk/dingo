#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/Type.h"
#include "dingo/Storage.h"

namespace dingo
{
    class Container;

    template < typename Type, typename U > struct Conversions< Unique, Type, U >
    {
        typedef std::tuple< U > ValueTypes;
        typedef std::tuple<> LvalueReferenceTypes;
        typedef std::tuple< U&& > RvalueReferenceTypes;
        typedef std::tuple<> PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, Type*, U >
    {
        typedef std::tuple< U > ValueTypes;
        typedef std::tuple<> LvalueReferenceTypes;
        typedef std::tuple< U&& > RvalueReferenceTypes;
        typedef std::tuple< U* > PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, std::shared_ptr< Type >, U >
    {
        typedef std::tuple< std::shared_ptr< U > > ValueTypes;
        typedef std::tuple<> LvalueReferenceTypes;
        typedef std::tuple< std::shared_ptr< U >&& > RvalueReferenceTypes;
        typedef std::tuple<> PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Unique, std::unique_ptr< Type >, U >
    {
        typedef std::tuple< std::unique_ptr< U > > ValueTypes;
        typedef std::tuple<> LvalueReferenceTypes;
        typedef std::tuple< std::unique_ptr< U >&& > RvalueReferenceTypes;
        typedef std::tuple<> PointerTypes;
    };

    template < typename Type, typename Conversions > class Storage< Unique, Type, Conversions >
    {
    public:
        static const bool Destroyable = true;

        typedef Conversions Conversions;
        typedef Type Type;

        Type Resolve(Context& context)
        {
            return TypeFactory< typename TypeDecay< Type >::type >::template Construct< Type, Container::ConstructorArgument< TypeDecay< Type >::type > >(context);
        }
    };
}