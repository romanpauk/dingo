#pragma once

#include <dingo/memory/arena_allocator.h>
#include <dingo/resolving_context.h>
#include <dingo/class_instance_resolver.h>
#include <dingo/class_instance_factory_i.h>

namespace dingo
{
    template <
        typename Container,
        typename TypeInterface,
        typename Type,
        typename Storage
    >
    class class_instance_factory
        : public class_instance_factory_i< Container >
    {
        Storage storage_;
        using ResolveType = decltype(storage_.resolve(std::declval< resolving_context< Container >& >()));
        using InterfaceType = typename rebind_type < ResolveType, TypeInterface >::type;
        class_instance_resolver< typename Container::rtti_type, InterfaceType, Storage > resolver_;

    public:
        template < typename... Args > class_instance_factory(Args&&... args)
            : storage_(std::forward< Args >(args)...)
        {}

        class_instance_i< typename Container::rtti_type >* resolve(resolving_context< Container >& context) override {
            return resolver_.resolve(context, storage_);
        }
    };

    template <
        typename Container,
        typename TypeInterface,
        typename Type,
        typename Storage
    >
    class class_instance_factory< Container, TypeInterface, Type, std::shared_ptr< Storage > >
        : public class_instance_factory_i< Container >
    {
        std::shared_ptr< Storage > storage_;
        using ResolveType = decltype(storage_->resolve(std::declval< resolving_context< Container >& >()));
        using InterfaceType = typename rebind_type < ResolveType, TypeInterface >::type;
        class_instance_resolver< typename Container::rtti_type, InterfaceType, Storage > resolver_;

    public:
        class_instance_factory(std::shared_ptr< Storage > storage)
            : storage_(storage)
        {}

        class_instance_i< typename Container::rtti_type >* resolve(resolving_context< Container >& context) override {
            return resolver_.resolve(context, *storage_);
        }
    };
}