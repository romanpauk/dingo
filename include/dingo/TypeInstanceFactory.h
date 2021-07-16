#pragma once

#include <dingo/arena_allocator.h>
#include <dingo/resolving_context.h>
#include "dingo/ITypeInstanceFactory.h"

namespace dingo
{
    class resolving_context;

    template< typename TypeInterface, typename Storage, bool IsCaching > struct TypeInstanceCache
        : public IResettable
    {
        template < typename Context > ITypeInstance* Resolve(Context& context, Storage& storage)
        {
            auto allocator = context.template get_allocator< TypeInstance< TypeInterface, Storage > >();
            auto instance = allocator.allocate(1);
            new (instance) TypeInstance< TypeInterface, Storage >(storage.Resolve(context));
            context.AddTypeInstance(instance);
            return instance;
        }

        bool IsResolved() { return false; }
        void Reset() override {}
    };

    template< typename TypeInterface, typename Storage > struct TypeInstanceCache< TypeInterface, Storage, true >
        : public IResettable
    {
        ITypeInstance* Resolve(resolving_context& context, Storage& storage)
        {
            if (!instance_)
            {
                if (!storage.IsResolved()) context.AddResettable(&storage);
                context.AddResettable(this);

                instance_.emplace(storage.Resolve(context));
            }

            return &*instance_;
        }

        bool IsResolved() { return instance_.operator bool(); }
        void Reset() override { instance_.reset(); }

    private:
        std::optional< TypeInstance< TypeInterface, Storage > > instance_;
    };

    template <
        typename TypeInterface,
        typename Type,
        typename Storage
    >
        class TypeInstanceFactory
        : public ITypeInstanceFactory
    {
        Storage storage_;
        typedef decltype(storage_.Resolve(std::declval< resolving_context& >())) ResolveType;
        typedef typename RebindType < ResolveType, TypeInterface >::type InterfaceType;
        TypeInstanceCache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        TypeInstanceFactory()
        {}

        ITypeInstance* Resolve(resolving_context& context) override
        {
            return cache_.Resolve(context, storage_);
        }

        bool IsResolved() override
        {
            return cache_.IsResolved();
        }

        bool IsCaching() override
        {
            return Storage::IsCaching;
        }
    };

    template <
        typename TypeInterface,
        typename Type,
        typename Storage
    >
        class TypeInstanceFactory< TypeInterface, Type, std::shared_ptr< Storage > >
        : public ITypeInstanceFactory
    {
        std::shared_ptr< Storage > storage_;
        typedef decltype(storage_->Resolve(std::declval< resolving_context& >())) ResolveType;
        typedef typename RebindType < ResolveType, TypeInterface >::type InterfaceType;
        TypeInstanceCache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        TypeInstanceFactory(std::shared_ptr< Storage > storage)
            : storage_(storage)
        {}

        ITypeInstance* Resolve(resolving_context& context) override
        {
            return cache_.Resolve(context, *storage_);
        }

        bool IsResolved() override
        {
            return cache_.IsResolved();
        }

        bool IsCaching() override
        {
            return Storage::IsCaching;
        }
    };
}