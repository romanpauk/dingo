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

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared, Type, Conversions >
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

        template < typename Container > Type* resolve(resolving_context< Container >& context)
        {
            if (!initialized_)
            {
                class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type, resolving_context< Container > > >(context, &instance_);
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
        template < typename Container > Type* resolve(resolving_context< Container >& context)
        {
            return storage< Container, shared, std::unique_ptr< Type >, Conversions >::resolve(context).get();
        }
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared, std::shared_ptr< Type >, Conversions >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        template < typename Container > std::shared_ptr< Type > resolve(resolving_context< Container >& context)
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
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        template < typename Container > std::unique_ptr< Type >& resolve(resolving_context< Container >& context)
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