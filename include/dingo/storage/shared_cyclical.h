#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/storage.h>
#include <dingo/memory/virtual_pointer.h>
#include <dingo/constructible_i.h>

#include <memory>

namespace dingo
{
    class container;

    struct shared_cyclical {};

    template < typename Type, typename U > struct conversions< shared_cyclical, Type, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct conversions< shared_cyclical, Type*, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename Conversions > class storage< shared_cyclical, Type, Conversions >
        : public resettable_i
        , public constructible_i
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        storage()
            : constructed_()
            , resolved_()
        {}

        ~storage()
        {
            reset();
        }

        Type* resolve(resolving_context& context)
        {
            if (!resolved_)
            {
                resolved_ = true;
                context.register_constructible(this);
            }
            
            return reinterpret_cast< Type* >(&instance_);
        }

        bool is_resolved() const { return resolved_; }
        
        void reset() override
        {
            if (constructed_)
            {
                constructed_ = resolved_ = false;
                reinterpret_cast<Type*>(&instance_)->~Type();
            }
        }

        void construct(resolving_context& context, int phase) override 
        { 
            if (phase == 0)
            {
                class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type > >(context, reinterpret_cast<Type*>(&instance_));
                constructed_ = true;
            } 
        }

        bool has_address(uintptr_t address) override { return false; }

    private:
        std::aligned_storage_t< sizeof(Type) > instance_;
        bool resolved_;
        bool constructed_;
    };

/*
    template < typename Type, typename Conversions > class storage< shared_cyclical, Type*, Conversions >
        : public storage< shared_cyclical, std::unique_ptr< Type >, Conversions >
    {
    public:
        Type* resolve(resolving_context& context)
        {
            return storage< shared, std::unique_ptr< Type >, Conversions >::resolve(context).get();
        }
    };

    template < typename Type, typename Conversions > class storage< shared_cyclical, std::unique_ptr< Type >, Conversions >
        : public resettable_i
        , public constructible_i
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        storage()
            : constructed_()
            , resolved_()
        {}

        ~storage()
        {
            reset();
        }

        std::unique_ptr< Type >& resolve(resolving_context& context)
        {
            if (!resolved_)
            {
                resolved_ = true;
                context.register_constructible(this);
            }

            return *reinterpret_cast<std::unique_ptr<Type>*>(&instance_);
        }

        bool is_resolved() const { return resolved_; }

        void reset() override
        {
            if (constructed_)
            {
                constructed_ = resolved_ = false;
                reinterpret_cast<std::unique_ptr<Type>*>(&instance_)->~unique_ptr< Type >();
            }
        }

        void construct(resolving_context& context, int phase) override
        {
            if (phase == 0)
            {
                class_factory< decay_t< Type > >::template construct< std::unique_ptr< Type >, constructor_argument< Type > >(context, reinterpret_cast<std::unique_ptr< Type >*>(&instance_));
                constructed_ = true;
            }
        }

        bool has_address(uintptr_t address) override { return false; }

    private:
        std::aligned_storage_t< sizeof(std::unique_ptr< Type >) > instance_;
        bool resolved_;
        bool constructed_;
    };

    template < typename Type, typename Conversions > class storage< shared_cyclical, std::shared_ptr< Type >, Conversions >
        : public resettable_i
        , public constructible_i     
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        std::shared_ptr< Type >& resolve(resolving_context& context)
        {
            if (!resolved_)
            {
                resolved_ = true;
                context.register_constructible(this);
            }

            return reinterpret_cast< std::shared_ptr< Type >* >(&instance_);
        }

        bool is_resolved() const { return resolved_; }
        void reset() override { instance_.reset(); virtualInstance_.reset(); }

        void construct(resolving_context& context, int phase) override { cyclical_support_protected< Type >::construct(context, phase, *virtualInstance_); }

        bool has_address(uintptr_t address) override { return virtualInstance_->HasAddress(address); }

    private:
        std::aligned_storage_t< sizeof(std::shared_ptr< Type >) > instance_;
    };
*/
}