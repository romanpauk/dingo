#pragma once

namespace dingo
{
    template< typename TypeInterface, typename Storage, bool IsCaching > struct class_instance_cache
        : public IResettable
    {
        template < typename Context > class_instance_i* resolve(Context& context, Storage& storage)
        {
            auto allocator = context.template get_allocator< class_instance< TypeInterface, Storage > >();
            auto instance = allocator.allocate(1);
            new (instance) class_instance< TypeInterface, Storage >(storage.Resolve(context));
            context.register_type_instance(instance);
            return instance;
        }

        bool is_resolved() { return false; }
        void Reset() override {}
    };

    template< typename TypeInterface, typename Storage > struct class_instance_cache< TypeInterface, Storage, true >
        : public IResettable
    {
        class_instance_i* resolve(resolving_context& context, Storage& storage)
        {
            if (!instance_)
            {
                if (!storage.IsResolved())
                {
                    context.register_resettable(&storage);
                }

                context.register_resettable(this);

                instance_.emplace(storage.Resolve(context));
            }

            return &*instance_;
        }

        bool is_resolved() { return instance_.operator bool(); }
        void Reset() override { instance_.reset(); }

    private:
        std::optional< class_instance< TypeInterface, Storage > > instance_;
    };
}