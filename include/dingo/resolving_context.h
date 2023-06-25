#pragma once

#include <dingo/memory/arena_allocator.h>
#include <dingo/resettable_i.h>
#include <dingo/constructible_i.h>

#include <array>
#include <forward_list>
#include <list>

namespace dingo
{
    template < typename T > class ContextTracking
    {
    public:
        ContextTracking() { CurrentContext = static_cast < T* >(this); }
        ~ContextTracking() { CurrentContext = nullptr; }

        static thread_local T* CurrentContext;
    };

    template < typename T > thread_local T* ContextTracking< T >::CurrentContext;

    template< typename Container > class resolving_context
        : public ContextTracking< resolving_context< Container > >
    {
    public:
        resolving_context(Container& container)
            : container_(container)
            , allocator_(arena_, container.get_allocator())
            , type_instances_(allocator_)
            , resettables_(allocator_)
            , constructibles_(allocator_)
        {}

        ~resolving_context()
        {
            if (std::uncaught_exceptions())
            {
                // Rollback changes in container
                for (auto& resettable : resettables_)
                {
                    resettable->reset();
                }
            }

            // Destroy temporary instances
            for (auto& typeInstance : type_instances_)
            {
                typeInstance->~class_instance_i();
            }
        }


        template < typename T > T resolve()
        {
            return this->container_.template resolve< T, false >(*this);
        }

        template < typename T > arena_allocator< T > get_allocator() { return allocator_; }

        void register_type_instance(class_instance_i* instance) { type_instances_.push_front(instance); }
        void register_resettable(resettable_i* ptr) { resettables_.push_front(ptr); }
        void register_constructible(constructible_i< Container >* ptr) { constructibles_.push_back(ptr); }

        bool has_constructible_address(uintptr_t address)
        {
            for (auto& constructible : constructibles_)
            {
                if (constructible->has_address(address))
                {
                    return true;
                }
            }

            return false;
        }

        void finalize()
        {
            // Note that invocation of Construct(_, 0) can grow constructibles_.
            for(auto it = constructibles_.begin(); it != constructibles_.end(); ++it)
            {
                (*it)->construct(*this, 0);
            }

            for (auto& constructible : constructibles_)
            {
                constructible->construct(*this, 1);
            }
        }

    private:
        Container& container_;

        arena< 1024 > arena_;
        arena_allocator< void, typename Container::allocator_type > allocator_;

        // TODO: rebind alloc
        std::forward_list< class_instance_i*, 
            arena_allocator< class_instance_i*, typename std::allocator_traits< typename Container::allocator_type >::template rebind_alloc< class_instance_i* > > > type_instances_;
        std::forward_list< resettable_i*, 
            arena_allocator< resettable_i*, typename std::allocator_traits< typename Container::allocator_type >::template rebind_alloc< resettable_i* > > > resettables_;
        std::list< constructible_i< Container >*, 
            arena_allocator< constructible_i< Container >*, typename std::allocator_traits< typename Container::allocator_type >::template rebind_alloc< constructible_i< Container >* > > > constructibles_;
    };

    template < typename DisabledType, typename Context > class constructor_argument
    {
    public:
        constructor_argument(Context& context)
            : context_(context)
        {}

        template < typename T, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator T* () { return context_.template resolve< T* >(); }
        template < typename T, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator T& () { return context_.template resolve< T& >(); }
        template < typename T, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator T&& () { return context_.template resolve< T&& >(); }
        
        template < typename T, typename Tag, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator annotated< T, Tag > () { return context_.template resolve< annotated< T, Tag > >(); }

    private:
        Context& context_;
    };
}