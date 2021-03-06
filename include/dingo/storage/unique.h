#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/storage.h>

namespace dingo
{
    class Container;
    
    struct unique {};

    template < typename Type, typename U > struct conversions< unique, Type, U >
    {
        typedef type_list< U > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< U&& > RvalueReferenceTypes;
        typedef type_list<> PointerTypes;
    };

    template < typename Type, typename U > struct conversions< unique, Type*, U >
    {
        typedef type_list< U > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< U&& > RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct conversions< unique, std::shared_ptr< Type >, U >
    {
        typedef type_list< std::shared_ptr< U > > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< std::shared_ptr< U >&& > RvalueReferenceTypes;
        typedef type_list<> PointerTypes;
    };

    template < typename Type, typename U > struct conversions< unique, std::unique_ptr< Type >, U >
    {
        typedef type_list< std::unique_ptr< U > > ValueTypes;
        typedef type_list<> LvalueReferenceTypes;
        typedef type_list< std::unique_ptr< U >&& > RvalueReferenceTypes;
        typedef type_list<> PointerTypes;
    };

    template < typename Type, typename Conversions > class storage< unique, Type, Conversions >
    {
    public:
        static const bool IsCaching = false;

        typedef Conversions Conversions;
        typedef Type Type;

        Type resolve(resolving_context& context)
        {
            return class_factory< decay_t< Type > >::template construct< Type, constructor_argument< decay_t< Type > > >(context);
        }

        bool is_resolved() { return false; }
    };
}