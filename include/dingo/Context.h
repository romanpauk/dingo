#pragma once

#include "dingo/ArenaAllocator.h"
#include "dingo/IResettable.h"

#include <array>
#include <forward_list>

namespace dingo
{
    class Container;

    class Context
    {
        friend class Container;

    public:
        Context(Container& container)
            : container_(container)
            , arena_(buffer_)
            , allocator_(arena_)
            , typeInstances_(allocator_)
            , resettables_(allocator_)
        {}

        template < typename T > T Resolve();

        template < typename T > ArenaAllocator< T > GetAllocator() { return allocator_; }

        void AddTypeInstance(ITypeInstance* instance)
        {
            typeInstances_.push_front(instance);
        }

        void AddResettable(IResettable* ptr)
        {
            resettables_.push_front(ptr);
        }

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

    private:
        Container& container_;

        std::array< unsigned char, 1024 > buffer_;
        Arena arena_;
        ArenaAllocator< void > allocator_;

        std::forward_list< ITypeInstance*, ArenaAllocator< ITypeInstance* > > typeInstances_;
        std::forward_list< IResettable*, ArenaAllocator< IResettable* > > resettables_;
    };

}