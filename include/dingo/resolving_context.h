#pragma once

#include <dingo/memory/arena_allocator.h>
#include <dingo/resettable_i.h>
#include <dingo/constructible_i.h>

#include <array>
#include <deque>
#include <list>
#include <forward_list>

namespace dingo {
    template< typename Container > class resolving_context {
        static constexpr size_t size = 32;
        static constexpr size_t arena_size = 1024 * 4;

        using arena_type = arena< arena_size >;

    public:
        resolving_context(Container& container)
            : container_(container)
            , class_instances_size_()
            , resettables_size_()
            , constructibles_size_()
            , resolve_counter_()
        {}

        ~resolving_context() 
        {
            if(resolve_counter_ == 0) 
            {
                if (constructibles_size_) 
                {
                    // Do the two phase construction required for cyclical dependencies
                    for(int state = 0; state < 2; state++) 
                    {
                        // Note that invocation of construct(_, 0) can grow constructibles_.
                        for(size_t i = 0; i < constructibles_size_; ++i)
                            constructibles_[i]->construct(*this, state);
                    }
                }
            }
            else 
            {
                // Rollback changes in container
                for (size_t i = 0; i < resettables_size_; ++i)
                {
                    resettables_[i]->reset();
                }
            }
            
            // Destroy temporary instances
            for (size_t i = 0; i < class_instances_size_; ++i)
            {
                class_instances_[i]->reset();
            }
        }
        
        template < typename T > T resolve() {
            return this->container_.template resolve< T, false >(*this);
        }

        void register_class_instance(resettable_i* ptr) {
            check_size(class_instances_size_);
            class_instances_[class_instances_size_++] = ptr; 
        }

        void register_resettable(resettable_i* ptr) { 
            check_size(resettables_size_);
            resettables_[resettables_size_++] = ptr; 
        }

        void register_constructible(constructible_i< Container >* ptr) { 
            check_size(constructibles_size_);
            constructibles_[constructibles_size_++] = ptr; 
        }

        bool has_constructible_address(uintptr_t address)
        {
            for (size_t i = 0; i < constructibles_size_; ++i)
            {
                if (constructibles_[i]->has_address(address))
                    return true;
            }

            return false;
        }

        // Use this counter to determine if exception was thrown or not (std::uncaught_exceptions is slow).
        void increment() { ++resolve_counter_; }
        void decrement() { --resolve_counter_; }

    private:
        void check_size(size_t count) {
            if(size == count)
                throw type_overflow_exception();
        }
        
        Container& container_;
        
        // TODO: this is fast, but non-generic, needs more work
        std::array< resettable_i*, size > class_instances_;
        std::array< resettable_i*, size > resettables_;
        std::array< constructible_i< Container >*, size > constructibles_;

        size_t class_instances_size_;
        size_t resettables_size_;
        size_t constructibles_size_;

        size_t resolve_counter_;
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