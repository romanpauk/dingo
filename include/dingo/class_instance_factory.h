#pragma once

#include <dingo/memory/arena_allocator.h>
#include <dingo/resolving_context.h>
#include <dingo/class_instance_cache.h>
#include <dingo/class_instance_factory_i.h>

namespace dingo
{
    class resolving_context;

    template <
        typename TypeInterface,
        typename Type,
        typename Storage
    >
    class class_instance_factory
        : public class_instance_factory_i
    {
        Storage storage_;
        typedef decltype(storage_.resolve(std::declval< resolving_context& >())) ResolveType;
        typedef typename rebind_type < ResolveType, TypeInterface >::type InterfaceType;
        class_instance_cache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        class_instance_factory()
        {}

        class_instance_i* resolve(resolving_context& context) override
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
        typename TypeInterface,
        typename Type,
        typename Storage
    >
    class class_instance_factory< TypeInterface, Type, std::shared_ptr< Storage > >
        : public class_instance_factory_i
    {
        std::shared_ptr< Storage > storage_;
        typedef decltype(storage_->resolve(std::declval< resolving_context& >())) ResolveType;
        typedef typename rebind_type < ResolveType, TypeInterface >::type InterfaceType;
        class_instance_cache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        class_instance_factory(std::shared_ptr< Storage > storage)
            : storage_(storage)
        {}

        class_instance_i* resolve(resolving_context& context) override
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