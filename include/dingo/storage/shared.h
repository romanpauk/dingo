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

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared, Type, Conversions >
        : public resettable_i
    {
    public:
        static const bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage()
            : initialized_(false)
        {}

        ~storage()
        {
            reset();
        }

        Type* resolve(resolving_context< Container >& context)
        {
            if (!initialized_)
            {
                class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type, resolving_context< Container > > >(&instance_, context);
                initialized_ = true;
            }

            return reinterpret_cast<Type*>(&instance_);
        }

        bool is_resolved() const { return initialized_; }

        void reset() override
        {
            if (initialized_)
            {
                initialized_ = false;
                reinterpret_cast<Type*>(&instance_)->~Type();
            }
        }

    private:
        std::aligned_storage_t< sizeof(Type) > instance_;
        bool initialized_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared, Type*, Conversions >
        : public storage< Container, shared, std::unique_ptr< Type >, Conversions >
    {
    public:
        template < typename ContainerT > Type* resolve(resolving_context< ContainerT >& context)
        {
            return storage< ContainerT, shared, std::unique_ptr< Type >, Conversions >::resolve(context).get();
        }
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared, std::shared_ptr< Type >, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        std::shared_ptr< Type > resolve(resolving_context< Container >& context)
        {
            if (!instance_)
            {
                instance_ = class_factory< Type >::template construct< std::shared_ptr< Type >, constructor_argument< Type, resolving_context< Container > > >(context);
            }

            return instance_;
        }

        void reset() override { instance_.reset(); }
        bool is_resolved() { return instance_.get() != nullptr; }

    private:
        std::shared_ptr< Type > instance_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared, std::unique_ptr< Type >, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        std::unique_ptr< Type >& resolve(resolving_context< Container >& context)
        {
            // TODO: thread-safe
            if (!instance_)
            {
                instance_ = class_factory< Type >::template construct< std::unique_ptr< Type >, constructor_argument< Type, resolving_context< Container > > >(context);
            }

            return instance_;
        }

        void reset() override { instance_.reset(); }
        bool is_resolved() { return instance_.get() != nullptr; }

    private:
        std::unique_ptr< Type > instance_;
    };
}