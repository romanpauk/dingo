#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/Type.h"
#include "dingo/Storage.h"
#include "dingo/VirtualPointer.h"
#include "dingo/IConstructible.h"

namespace dingo
{
    class Container;

    template < typename Type, typename U > struct Conversions< SharedCyclical, Type, U >: Conversions< Shared, Type, U > {};
    template < typename Type, typename U > struct Conversions< SharedCyclical, std::shared_ptr< Type >, U >: Conversions< Shared, std::shared_ptr< Type >, U > {};
    
    template < typename Type > struct CyclicalSupport
    {
    protected:
        int FilterException(Context& context, unsigned int code, struct _EXCEPTION_POINTERS* ep)
        {
            if (code == EXCEPTION_ACCESS_VIOLATION)
            {
                if (context.IsConstructibleAddress((uintptr_t)ep->ExceptionRecord->ExceptionInformation[1]))
                    return EXCEPTION_EXECUTE_HANDLER;
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }

        void Construct(Context& context, int phase, VirtualPointer< Type >& instance)
        {
            instance.SetAccessible(true);

            if (phase == 0)
            {
                __try
                {
                    TypeFactory< typename TypeDecay< Type >::type >::template Construct< Type*, Container::ConstructorArgument< Type > >(context, instance.Get());
                }
                __except (FilterException(context, GetExceptionCode(), GetExceptionInformation()))
                {
                    throw TypeNotConstructedException();
                }

                instance.SetAccessible(false);
            }
        }
    };

    template < typename Type, typename Conversions > class Storage< SharedCyclical, Type, Conversions >
        : public CyclicalSupport< Type >
        , public IResettable
        , public IConstructible
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        ~Storage()
        {
            Reset();
        }

        Type* Resolve(Context& context)
        {
            if (!instance_.Get())
            {
                instance_.Reserve();
                context.AddConstructible(this);
            }

            return instance_.Get();
        }

        bool IsResolved() const { return instance_.Get(); }

        void Reset() override { instance_.Reset(); }

        void Construct(Context& context, int phase) override { CyclicalSupport< Type >::Construct(context, phase, instance_); }

        bool HasAddress(uintptr_t address) override { return instance_.HasAddress(address); }

    private:
        VirtualPointer< Type > instance_;
    };

    template < typename Type, typename Conversions > class Storage< SharedCyclical, std::shared_ptr< Type >, Conversions >
        : public CyclicalSupport< Type >
        , public IResettable
        , public IConstructible        
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        ~Storage()
        {
            Reset();
        }

        std::shared_ptr< Type >& Resolve(Context& context)
        {
            if (!instance_)
            {
                virtualInstance_ = std::make_shared< VirtualPointer< Type > >();
                virtualInstance_->Reserve();

                auto virtualInstance = virtualInstance_;
                instance_.reset(virtualInstance_->Get(), [virtualInstance](Type*) {});

                context.AddConstructible(this);
            }

            return instance_;
        }

        bool IsResolved() const { return instance_.get() != nullptr; }

        void Reset() override { instance_.reset(); virtualInstance_.reset(); }

        void Construct(Context& context, int phase) override { CyclicalSupport< Type >::Construct(context, phase, *virtualInstance_); }

        bool HasAddress(uintptr_t address) override { return virtualInstance_->HasAddress(address); }

    private:
        std::shared_ptr< VirtualPointer< Type > > virtualInstance_;
        std::shared_ptr< Type > instance_;
    };
}