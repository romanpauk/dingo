#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/type_list.h>
#include "dingo/Storage.h"

namespace dingo
{
    class Container;

    template < typename Type, typename U > struct Conversions< Shared, Type, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Shared, Type*, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Shared, std::shared_ptr< Type >, U >
    {
        typedef type_list< std::shared_ptr< U > > ValueTypes;
        typedef type_list< U&, std::shared_ptr< U >& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U*, std::shared_ptr< U >* > PointerTypes;
    };

    template < typename Type, typename U > struct Conversions< Shared, std::unique_ptr< Type >, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U*, std::unique_ptr< U >* > PointerTypes;
        typedef type_list< U&, std::unique_ptr< U >& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
    };

    template < typename Type, typename Conversions > class Storage< Shared, Type, Conversions >
        : public IResettable
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        Storage()
            : initialized_(false)
        {}

        ~Storage()
        {
            Reset();
        }

        Type* Resolve(Context& context)
        {
            if (!initialized_)
            {
                class_factory< decay_t< Type > >::template construct< Type*, container::ConstructorArgument< Type > >(context, &instance_);
                initialized_ = true;
            }

            return reinterpret_cast<Type*>(&instance_);
        }

        bool IsResolved() const { return initialized_; }

        void Reset() override
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

    template < typename Type, typename Conversions > class Storage< Shared, Type*, Conversions >
        : public Storage< Shared, std::unique_ptr< Type >, Conversions >
    {
    public:
        Type* Resolve(Context& context)
        {
            return Storage< Shared, std::unique_ptr< Type >, Conversions >::Resolve(context).get();
        }
    };

    template < typename Type, typename Conversions > class Storage< Shared, std::shared_ptr< Type >, Conversions >
        : public IResettable
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        std::shared_ptr< Type >& Resolve(Context& context)
        {
            if (!instance_)
            {
                instance_ = class_factory< Type >::template construct< std::shared_ptr< Type >, container::ConstructorArgument< Type > >(context);
            }

            return instance_;
        }

        void Reset() override { instance_.reset(); }
        bool IsResolved() { return instance_.get() != nullptr; }

    private:
        std::shared_ptr< Type > instance_;
    };

    template < typename Type, typename Conversions > class Storage< Shared, std::unique_ptr< Type >, Conversions >
        : public IResettable
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        std::unique_ptr< Type >& Resolve(Context& context)
        {
            // TODO: thread-safe
            if (!instance_)
            {
                instance_ = class_factory< Type >::template construct< std::unique_ptr< Type >, container::ConstructorArgument< Type > >(context);
            }

            return instance_;
        }

        void Reset() override { instance_.reset(); }
        bool IsResolved() { return instance_.get() != nullptr; }

    private:
        std::unique_ptr< Type > instance_;
    };
}