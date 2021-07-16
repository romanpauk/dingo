#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/type_list.h>
#include "dingo/Storage.h"

namespace dingo
{
    class container;

    struct shared {};

    template < typename Type, typename U > struct conversions< shared, Type, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct conversions< shared, Type*, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct conversions< shared, std::shared_ptr< Type >, U >
    {
        typedef type_list< std::shared_ptr< U > > ValueTypes;
        typedef type_list< U&, std::shared_ptr< U >& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U*, std::shared_ptr< U >* > PointerTypes;
    };

    template < typename Type, typename U > struct conversions< shared, std::unique_ptr< Type >, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U*, std::unique_ptr< U >* > PointerTypes;
        typedef type_list< U&, std::unique_ptr< U >& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
    };

    template < typename Type, typename Conversions > class storage< shared, Type, Conversions >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        storage()
            : initialized_(false)
        {}

        ~storage()
        {
            reset();
        }

        Type* resolve(resolving_context& context)
        {
            if (!initialized_)
            {
                class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type > >(context, &instance_);
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

    template < typename Type, typename Conversions > class storage< shared, Type*, Conversions >
        : public storage< shared, std::unique_ptr< Type >, Conversions >
    {
    public:
        Type* resolve(resolving_context& context)
        {
            return storage< shared, std::unique_ptr< Type >, Conversions >::resolve(context).get();
        }
    };

    template < typename Type, typename Conversions > class storage< shared, std::shared_ptr< Type >, Conversions >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        std::shared_ptr< Type >& resolve(resolving_context& context)
        {
            if (!instance_)
            {
                instance_ = class_factory< Type >::template construct< std::shared_ptr< Type >, constructor_argument< Type > >(context);
            }

            return instance_;
        }

        void reset() override { instance_.reset(); }
        bool is_resolved() { return instance_.get() != nullptr; }

    private:
        std::shared_ptr< Type > instance_;
    };

    template < typename Type, typename Conversions > class storage< shared, std::unique_ptr< Type >, Conversions >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        std::unique_ptr< Type >& resolve(resolving_context& context)
        {
            // TODO: thread-safe
            if (!instance_)
            {
                instance_ = class_factory< Type >::template construct< std::unique_ptr< Type >, constructor_argument< Type > >(context);
            }

            return instance_;
        }

        void reset() override { instance_.reset(); }
        bool is_resolved() { return instance_.get() != nullptr; }

    private:
        std::unique_ptr< Type > instance_;
    };
}