#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/type_list.h>
#include <dingo/storage.h>

namespace dingo
{
    struct shared {};

    template < typename Type, typename U > struct conversions< shared, Type, U >
    {
        using value_types = type_list< U >;
        using lvalue_reference_types = type_list< U& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U* >;
    };

    template < typename Type, typename U > struct conversions< shared, Type*, U >
    {
        using value_types = type_list<>;
        using lvalue_reference_types = type_list< U& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U* >;
    };

    template < typename Type, typename U > struct conversions< shared, std::shared_ptr< Type >, U >
    {
        using value_types = type_list< U, std::shared_ptr< U > >;
        using lvalue_reference_types = type_list< U&, std::shared_ptr< U >& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U*, std::shared_ptr< U >* >;
    };

    template < typename Type, typename U > struct conversions< shared, std::unique_ptr< Type >, U >
    {
        using value_types = type_list<>;
        using lvalue_reference_types = type_list< U&, std::unique_ptr< U >& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U*, std::unique_ptr< U >* >;
    };

    template< typename Type > struct storage_instance_base {
        Type* get() const {
            return reinterpret_cast<Type*>(&instance_);
        }

        template< typename Context > void construct(Context& context) {
            assert(!initialized_);
            class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type, Context > >(&instance_, context);
            initialized_ = true;
        }
            
        bool empty() const { return !initialized_; }

    protected:
        mutable std::aligned_storage_t< sizeof(Type), alignof(Type) > instance_;
        bool initialized_ = false;
    };

    template< typename Type, 
        bool IsTriviallyDestructible = std::is_trivially_destructible_v< Type > > struct storage_instance_dtor;

    template< typename Type > struct storage_instance_dtor< Type, true >
        : storage_instance_base< Type > 
    {
        void reset() { this->initialized_ = false; }
    };

    template< typename Type > struct storage_instance_dtor< Type, false >
        : storage_instance_base< Type >
    {
        ~storage_instance_dtor() {
            reset();
        }

        void reset() {
            if (this->initialized_) {
                this->initialized_ = false;
                this->get()->~Type();
            }
        }
    };
    
    template < typename Type > struct storage_instance< Type, shared >
        : storage_instance_dtor< Type >
    {
        static_assert(std::is_trivially_destructible_v< Type > == std::is_trivially_destructible_v< storage_instance_dtor< Type > >);
    };

    template < typename Type > struct storage_instance< std::unique_ptr<Type>, shared > {
        template< typename Context > void construct(Context& context) {
            assert(!instance_);
            instance_ = class_factory< Type >::template construct< std::unique_ptr< Type >, constructor_argument< Type, Context > >(context);
        }

        std::unique_ptr<Type>& get() const { return instance_; }
        void reset() { instance_.reset(); }
        bool empty() const { return instance_.get() == nullptr; } 

    private:
        mutable std::unique_ptr<Type> instance_;
    };

    template < typename Type > struct storage_instance< std::shared_ptr<Type>, shared > {
        template< typename Context > void construct(Context& context) {
            assert(!instance_);
            instance_ = class_factory< Type >::template construct< std::shared_ptr< Type >, constructor_argument< Type, Context > >(context);
        }

        // TODO: this can't be reference so we can construct shared_ptr<Base> from shared_ptr<Type> later
        std::shared_ptr<Type> get() const { return instance_; }
        void reset() { instance_.reset(); }
        bool empty() const { return instance_.get() == nullptr; } 

    private:
        mutable std::shared_ptr<Type> instance_;
    };

    template < typename Type > struct storage_instance< Type*, shared >
        : public storage_instance< std::unique_ptr< Type >, shared > 
    {
        template< typename Context > void construct(Context& context) {
            storage_instance< std::unique_ptr< Type >, shared >::construct(context);
        }

        Type* get() const { return storage_instance< std::unique_ptr< Type >, shared >::get().get(); }
    };

    template < typename Type, typename Conversions > class storage< shared, Type, void, Conversions >
        : public resettable_i
    {
        // TODO
        //static_assert(std::is_trivially_destructible_v< Type > == std::is_trivially_destructible_v< storage_instance< Type, shared > >);
        storage_instance< Type, shared > instance_;

    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        template< typename Context > auto resolve(Context& context) -> decltype(instance_.get()) {
            if (instance_.empty())
                instance_.construct(context);
            return instance_.get();
        }

        bool is_resolved() const { return !instance_.empty(); }
        void reset() override { instance_.reset(); }
    };
}