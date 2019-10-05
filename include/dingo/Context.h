#pragma once

#include "dingo/ArenaAllocator.h"
#include "dingo/IResettable.h"
#include "dingo/IConstructible.h"

#include <array>
#include <forward_list>
#include <list>

namespace dingo
{
    class Container;

    template < typename T > class ContextTracking
    {
    public:
        ContextTracking() { CurrentContext = static_cast < T* >(this); }
        ~ContextTracking() { CurrentContext = nullptr; }

        static thread_local T* CurrentContext;
    };

    template < typename T > thread_local T* ContextTracking< T >::CurrentContext;

    class Context
        : public ContextTracking< Context >
    {
        friend class Container;

    public:
        Context(Container& container)
            : container_(container)
            , arena_(buffer_)
            , allocator_(arena_)
            , typeInstances_(allocator_)
            , resettables_(allocator_)
            , constructibles_(allocator_)
        {}

        ~Context()
        {
            if (std::uncaught_exceptions())
            {
                // Rollback changes in container
                for (auto& resettable : resettables_)
                {
                    resettable->Reset();
                }
            }

            // Destroy temporary instances
            for (auto& typeInstance : typeInstances_)
            {
                typeInstance->~ITypeInstance();
            }
        }

        template < typename T > T Resolve();

        template < typename T > ArenaAllocator< T > GetAllocator() { return allocator_; }

        void AddTypeInstance(ITypeInstance* instance) { typeInstances_.push_front(instance); }
        void AddResettable(IResettable* ptr) { resettables_.push_front(ptr); }
        void AddConstructible(IConstructible* ptr) { constructibles_.push_back(ptr); }

        bool IsConstructibleAddress(uintptr_t address)
        {
            for (auto& constructible : constructibles_)
            {
                if (constructible->HasAddress(address))
                {
                    return true;
                }
            }

            return false;
        }

        void Construct()
        {
            // Note that invocation of Construct(_, 0) can grow constructibles_.
            for(auto it = constructibles_.begin(); it != constructibles_.end(); ++it)
            {
                (*it)->Construct(*this, 0);
            }

            for (auto& constructible : constructibles_)
            {
                constructible->Construct(*this, 1);
            }
        }

    private:
        Container& container_;

        std::array< unsigned char, 1024 > buffer_;
        Arena arena_;
        ArenaAllocator< void > allocator_;

        std::forward_list< ITypeInstance*, ArenaAllocator< ITypeInstance* > > typeInstances_;
        std::forward_list< IResettable*, ArenaAllocator< IResettable* > > resettables_;
        std::list< IConstructible*, ArenaAllocator< IConstructible* > > constructibles_;
    };

}