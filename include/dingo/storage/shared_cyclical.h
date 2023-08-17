#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/storage.h>
#include <dingo/memory/virtual_pointer.h>
#include <dingo/constructible_i.h>

#include <memory>
#include <atomic>

namespace dingo
{
    struct shared_cyclical {};

    template < typename Type, typename U > struct conversions< shared_cyclical, Type, U >
    {
        using value_types = type_list<>;
        using lvalue_reference_types = type_list< U& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U* >;
    };

    template < typename Type, typename U > struct conversions< shared_cyclical, Type*, U >
    {
        using value_types = type_list<>;
        using lvalue_reference_types = type_list< U& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U* >;
    };

    template < typename Type, typename U > struct conversions< shared_cyclical, std::shared_ptr< Type >, U >
    {
        using value_types = type_list<>;
        using lvalue_reference_types = type_list< U&, std::shared_ptr< U >& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U*, std::shared_ptr< U >* >;
    };

    template < typename Type > class storage_instance< Type, shared_cyclical > {
    public:
        ~storage_instance() {
            // TODO: should not have dtor for trivially-destructible Type
            reset();
        }

        template< typename Context > Type* resolve(Context& context) {
            assert(!resolved_);
            resolved_ = true;
            return get();
        }

        Type* get() { return reinterpret_cast< Type* >(&instance_); }

        template< typename Context > void construct(Context& context) {
            class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type, Context > >(&instance_, context);
            constructed_ = true;
        }
        
        bool empty() const { return !resolved_; }
        
        void reset() {
            resolved_ = false;
            if (constructed_) {
                constructed_ = false;
                reinterpret_cast<Type*>(&instance_)->~Type();
            }
        }
    private:
        std::aligned_storage_t< sizeof(Type), alignof(Type) > instance_;
        bool resolved_ = false;
        bool constructed_ = false;
    };

    template < typename T > class storage_instance_deleter {
    public:
        storage_instance_deleter()
            : constructed_(new std::atomic< bool >(false))
        {}

        void set_constructed() { constructed_->store(true); }

        void operator()(T* ptr) {
            if (constructed_->load()) {
                delete ptr;
            } else {
                delete[] reinterpret_cast<char*>(ptr);
            }
        }

    private:
        std::shared_ptr< std::atomic< bool > > constructed_;
    };

    template < typename Type > class storage_instance< std::shared_ptr<Type>, shared_cyclical > {
    public:
        template< typename Context > std::shared_ptr< Type > resolve(Context& context) {
            assert(!instance_);
            instance_.reset(reinterpret_cast<Type*>(new char[sizeof(Type)]), storage_instance_deleter< Type >());
            return instance_;
        }

        std::shared_ptr<Type> get() { return instance_; }

        template< typename Context > void construct(Context& context) {
            assert(!constructed_);
            class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type, Context > >(instance_.get(), context);
            std::get_deleter< storage_instance_deleter<Type> >(instance_)->set_constructed();
            constructed_ = true;
        }
        
        bool empty() const { return !instance_; }
        
        void reset() { instance_.reset(); }

    private:
        std::shared_ptr<Type> instance_;
        bool constructed_ = false;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared_cyclical, Type, Conversions >
        : public resettable_i
        , public constructible_i< Container >
    {
        storage_instance< Type, shared_cyclical > instance_;

    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        auto resolve(resolving_context< Container >& context) -> decltype(instance_.get()) {
            if (instance_.empty()) {
                context.register_constructible(this);
                return instance_.resolve(context);
            }
            return instance_.get();
        }

        bool is_resolved() const { return !instance_.empty(); }
        
        void reset() override { instance_.reset(); }

        void construct(resolving_context< Container >& context, int phase) override { 
            if (phase == 0)
                instance_.construct(context);
        }

        bool has_address(uintptr_t address) override { return false; }
    };
}