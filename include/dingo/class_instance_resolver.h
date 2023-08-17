#pragma once

#include <dingo/decay.h>
#include <dingo/exceptions.h>
#include <dingo/resettable_i.h>
#include <dingo/type_list.h>
#include <dingo/type_traits.h>

#include <optional>
#include <cstdlib>

namespace dingo
{
    // Required to support references
    template < typename T > struct class_instance_wrapper {
        template < typename... Args > class_instance_wrapper(Args&&... args)
            : instance_(std::forward< Args >(args)...)
        {}

        T& get() { return instance_; }

    private:
        T instance_;
    };

    template < typename T > struct class_recursion_guard_base {
        static bool visited_;
    };
    
    template < typename T > bool class_recursion_guard_base< T >::visited_ = false;

    template < typename T, bool Enabled = !std::is_default_constructible_v<T> > struct class_recursion_guard
        : class_recursion_guard_base< T > 
    {
        class_recursion_guard() {
            if (this->visited_)
                throw type_recursion_exception();
            this->visited_ = true;
        }

        ~class_recursion_guard() {
            this->visited_ = false;
        }
    };

    template < typename T > struct class_recursion_guard<T, false> {};
    template < typename T, bool Enabled = !std::is_trivially_destructible_v<T> > struct class_instance_reset {
        template< typename Context, typename Instance > class_instance_reset(Context& context, Instance* instance) {
            context.register_class_instance(instance);
        }
    };
    
    template < typename T > struct class_instance_reset< T, false > {
        template< typename Context > class_instance_reset(Context&, void*) {}
    };
    
    template < typename T, bool Enabled = !std::is_trivially_destructible_v<T> > struct class_storage_reset {
        template< typename Context, typename Storage > class_storage_reset(Context& context, Storage* storage) {
            if(!storage->is_resolved()) context.register_resettable(storage);
        }
    };
    
    template < typename T > struct class_storage_reset< T, false > {
        template< typename Context > class_storage_reset(Context&, void*) {}
    };
    
    // TODO: All the code here should use rollback/recursion guard only if they are required, eg.
    // the instance will be constructed, that is non-default (so can cause recursion), has throwing constructor etc.
    // Rollback support has a price.
    
    template< typename RTTI, typename TypeInterface, typename Storage, bool IsCaching = Storage::is_caching > struct class_instance_resolver
        : public resettable_i
    {
        // TODO: this is based on the knowledge what unique storage can return, so it is unique-specialization...

        void reset() override { instance_.reset(); }

        template < typename Conversions, typename Context > void* resolve(Context& context, Storage& storage, const typename RTTI::type_index& type) {
            if (!find_type< RTTI >(Conversions{}, type))
                throw type_not_convertible_exception();
            
            class_recursion_guard< decay_t< typename Storage::type > > recursion_guard;
            (recursion_guard);

            class_instance_reset< decay_t< typename Storage::type > > storage_reset(context, this);
            (storage_reset);

            instance_.emplace(storage.resolve(context));

            // TODO: this can't be used as it returns contained type address and for unique storage,
            // contained type addresses are not allowed. The conversion seems to be more part
            // of storage, like this whole class is tailored to unique storage too much
            //
            // return type_traits< TypeInterface >::get_address(instance_->instance);

            if constexpr(std::is_pointer_v< TypeInterface >) {
                // assert(type_traits< TypeInterface >::get_address(instance_->instance) == instance_->instance);
                return instance_->get();
            } else {
                // assert(type_traits< TypeInterface >::get_address(instance_->instance) == &instance_->instance);
                return &instance_->get();
            }
        }
        
    private:
        std::optional< class_instance_wrapper<TypeInterface> > instance_;
    };

    template< typename RTTI, typename TypeInterface, typename Storage > struct class_instance_resolver< RTTI, TypeInterface, Storage, true >
        : public resettable_i
    {
        void reset() override { instance_.reset(); }
    
        template < typename Conversions, typename Context > void* resolve(Context& context, Storage& storage, const typename RTTI::type_index& type) {
            if (!find_type< RTTI >(Conversions{}, type))
                throw type_not_convertible_exception();

            if (!instance_) {
                class_recursion_guard< decay_t< typename Storage::type > > recursion_guard;
                (recursion_guard);

                class_storage_reset< decay_t< typename Storage::type > > storage_reset(context, &storage);
                (storage_reset);
                
                // TODO: not all types will need a rollback
                context.register_resettable(this);
                context.increment();
                instance_.emplace(storage.resolve(context));
                context.decrement();
            }

            // TODO: this is to support very unfortunate casting of smart_ptr(s)<T> to T&...
            if constexpr (type_traits< std::remove_reference_t< TypeInterface > >::is_smart_ptr) {
                if (type.is_smart_ptr())
                    return &instance_->get();
            }
            
            return type_traits< std::remove_reference_t< TypeInterface > >::get_address(instance_->get());
        }
    
    private:
        std::optional< class_instance_wrapper<TypeInterface> > instance_;
    };
}
