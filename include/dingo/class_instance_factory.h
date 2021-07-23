#pragma once

#include <dingo/memory/arena_allocator.h>
#include <dingo/resolving_context.h>
#include <dingo/class_instance_cache.h>
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
        typedef decltype(storage_.resolve(std::declval< resolving_context< Container >& >())) ResolveType;
        typedef typename rebind_type < ResolveType, TypeInterface >::type InterfaceType;
        class_instance_cache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        class_instance_i* resolve(resolving_context< Container >& context) override
        {
            return cache_.resolve(context, storage_);
        }

        bool is_resolved() override
        {
            return cache_.is_resolved();
        }

        bool is_persistent() override
        {
            return Storage::IsCaching;
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
        typedef decltype(storage_->resolve(std::declval< resolving_context< Container >& >())) ResolveType;
        typedef typename rebind_type < ResolveType, TypeInterface >::type InterfaceType;
        class_instance_cache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        class_instance_factory(std::shared_ptr< Storage > storage)
            : storage_(storage)
        {}

        class_instance_i* resolve(resolving_context< Container >& context) override
        {
            return cache_.resolve(context, *storage_);
        }

        bool is_resolved() override
        {
            return cache_.is_resolved();
        }

        bool is_persistent() override
        {
            return Storage::IsCaching;
        }
    };
}