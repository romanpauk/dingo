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
        class SEHScopedTranslator
        {
        public:
            SEHScopedTranslator(_se_translator_function translator): translator_(_set_se_translator(translator)) {}
            ~SEHScopedTranslator() { _set_se_translator(translator_); }

        private:
            const _se_translator_function translator_;
        };

        void Construct(Context& context, int phase, VirtualPointer< Type >& instance)
        {
            instance.SetAccessible(true);

            if (phase == 0)
            {
                SEHScopedTranslator translator(SEHExceptionHandler);
                TypeFactory< typename TypeDecay< Type >::type >::template Construct< Type*, Container::ConstructorArgument< Type > >(context, instance.Get());
                
                instance.SetConstructed();
                instance.SetAccessible(false);
            }
        }

        static void SEHExceptionHandler(unsigned int u, EXCEPTION_POINTERS* ex)
        {
            if (ex->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
            {
                if (ContextTracking< Context >::CurrentContext->IsConstructibleAddress((uintptr_t)ex->ExceptionRecord->ExceptionInformation[1]))
                {
                    throw TypeNotConstructedException();
                }
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
        std::shared_ptr< Type > instance_;
        std::shared_ptr< VirtualPointer< Type > > virtualInstance_;
    };
}