#pragma once

#include <dingo/class_recursion_guard.h>
#include <dingo/resettable_i.h>
#include <dingo/decay.h>

#include <optional>

namespace dingo
{
    template< typename RTTI, typename TypeInterface, typename Storage, bool IsCaching = Storage::is_caching > struct class_instance_resolver
        : public resettable_i
    {
        using class_instance_type = class_instance< RTTI, TypeInterface, Storage >;

        template < typename Context > class_instance_type* resolve(Context& context, Storage& storage) {
            // TODO: the cache is based on TypeInterface, yet we should guard against Storage::type,
            // Yet that can't be done using cache's member variable.
            class_recursion_guard< decay_t< typename Storage::type > > guard;

            context.increment();
            instance_.emplace(storage.resolve(context));
            context.decrement();

            // TODO: check that Storage::type's triviallity trait is propagated
            static_assert(std::is_trivially_destructible_v< typename Storage::type > == 
                std::is_trivially_destructible_v< class_instance_type > || std::is_pointer_v< typename Storage::type >);

            if constexpr (!std::is_trivially_destructible_v< class_instance_type >)
                context.register_class_instance(this);
            return &*instance_;
        }

        void reset() override { instance_.reset(); }

    private:
        std::optional< class_instance_type > instance_;
    };

    template< typename RTTI, typename TypeInterface, typename Storage > struct class_instance_resolver< RTTI, TypeInterface, Storage, true >
        : public resettable_i
    {
        using class_instance_type = class_instance< RTTI, TypeInterface, Storage >;

        template < typename Context > class_instance_type* resolve(Context& context, Storage& storage) {
            if (!instance_) {
                class_recursion_guard< decay_t< typename Storage::type > > guard;

                if (!storage.is_resolved())
                    context.register_resettable(&storage);
                
                context.register_resettable(this);
                context.increment();
                instance_.emplace(storage.resolve(context));
                context.decrement();
            }

            return &*instance_;
        }

        void reset() override { instance_.reset(); }

    private:
        std::optional< class_instance_type > instance_;
    };
}