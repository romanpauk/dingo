#pragma once

#include "dingo/ArenaAllocator.h"
#include "dingo/Context.h"
#include "dingo/ITypeInstanceFactory.h"

namespace dingo
{
    class Context;

    template< typename TypeInterface, typename Storage, bool IsCaching > struct TypeInstanceCache
        : public IResettable
    {
        template < typename Context > ITypeInstance* Resolve(Context& context, Storage& storage)
        {
            auto allocator = context.template GetAllocator< TypeInstance< TypeInterface, Storage > >();
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
        ITypeInstance* Resolve(Context& context, Storage& storage)
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
        typedef decltype(storage_.Resolve(std::declval< Context& >())) ResolveType;
        typedef typename RebindType < ResolveType, TypeInterface >::type InterfaceType;
        TypeInstanceCache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        TypeInstanceFactory()
        {}

        ITypeInstance* Resolve(Context& context) override
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
        typedef decltype(storage_->Resolve(std::declval< Context& >())) ResolveType;
        typedef typename RebindType < ResolveType, TypeInterface >::type InterfaceType;
        TypeInstanceCache< InterfaceType, Storage, Storage::IsCaching > cache_;

    public:
        TypeInstanceFactory(std::shared_ptr< Storage > storage)
            : storage_(storage)
        {}

        ITypeInstance* Resolve(Context& context) override
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