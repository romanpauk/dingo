#pragma once

#include <dingo/resolving_context.h>
#include <dingo/class_instance_resolver.h>
#include <dingo/class_instance_factory_i.h>
#include <dingo/rebind_type.h>

namespace dingo {
    // TODO: this is bit convoluted, ideally merge resolver with factory
    template< typename T > struct class_instance_storage_traits {
        using type = T;
        using conversions = typename T::conversions;

        static T& get(type& storage) { return storage; }
    };

    template< typename T > struct class_instance_storage_traits< std::shared_ptr< T > > {
        using type = std::shared_ptr< T >;
        using conversions = typename T::conversions;

        static T& get(type& storage) { return *storage; }
    };

    template<
        typename Container,
        typename TypeInterface,
        typename Type,
        typename Storage 
    >
    class class_instance_factory
        : public class_instance_factory_i< Container >
    {
        using storage_traits = class_instance_storage_traits< Storage >;
        typename storage_traits::type storage_;
        using ResolveType = decltype(storage_traits::get(storage_).resolve(std::declval< resolving_context< Container >& >()));
        using InterfaceType = rebind_type_t < ResolveType, TypeInterface >;
        class_instance_resolver< typename Container::rtti_type, InterfaceType, decay_t< Storage > > resolver_;

    public:
        template < typename... Args > class_instance_factory(Args&&... args)
            : storage_(std::forward< Args >(args)...)
        {}

        // TODO: we will need two pairs of calls - context-aware and context-less
        // First, resolution will be attempted without context.
        //      Sufficient for trivial types or already resolved types (or types composed from those...), simply anything, that does not
        //      throw during construction.
        // Second, resolution will continue with context
        //      Could that use pointer to context, so in resolver we check if context is needed and if so, continue with pointer to the 
        //      stack?

        void* get_value(resolving_context< Container >& context, const typename Container::rtti_type::type_index& type) override {
            return resolver_.template resolve< typename storage_traits::conversions::value_types >(context, storage_traits::get(storage_), type);
        }

        void* get_lvalue_reference(resolving_context< Container >& context, const typename Container::rtti_type::type_index& type) override {
            return resolver_.template resolve< typename storage_traits::conversions::lvalue_reference_types >(context, storage_traits::get(storage_), type);
        }

        void* get_rvalue_reference(resolving_context< Container >& context, const typename Container::rtti_type::type_index& type) override {
            return resolver_.template resolve< typename storage_traits::conversions::rvalue_reference_types >(context, storage_traits::get(storage_), type);
        }

        void* get_pointer(resolving_context< Container >& context, const typename Container::rtti_type::type_index& type) override {
            return resolver_.template resolve< typename storage_traits::conversions::pointer_types >(context, storage_traits::get(storage_), type);
        }
    };
}