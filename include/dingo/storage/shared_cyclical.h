#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/storage.h>
#include <dingo/memory/VirtualPointer.h>
#include <dingo/IConstructible.h>

namespace dingo
{
    class container;

    struct shared_cyclical {};

    template < typename Type, typename U > struct conversions< shared_cyclical, Type, U >: conversions< shared, Type, U > {};
    template < typename Type, typename U > struct conversions< shared_cyclical, std::shared_ptr< Type >, U >: conversions< shared, std::shared_ptr< Type >, U > {};

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

        void Construct(resolving_context& context, int phase, VirtualPointer< Type >& instance)
        {
            instance.SetAccessible(true);

            if (phase == 0)
            {
                SEHScopedTranslator translator(SEHExceptionHandler);
                class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type > >(context, instance.Get());
                
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
                    throw type_not_constructed_exception();
                }
            }
        }
    };

    template < typename Type, typename Conversions > class storage< shared_cyclical, Type, Conversions >
        : public CyclicalSupport< Type >
        , public IResettable
        , public IConstructible
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        Type* resolve(resolving_context& context)
        {
            if (!instance_.Get())
            {
                instance_.Reserve();
                context.AddConstructible(this);
            }

            return instance_.Get();
        }

        bool is_resolved() const { return instance_.Get(); }
        void reset() override { instance_.Reset(); }

        void construct(resolving_context& context, int phase) override { CyclicalSupport< Type >::Construct(context, phase, instance_); }

        bool HasAddress(uintptr_t address) override { return instance_.HasAddress(address); }

    private:
        VirtualPointer< Type > instance_;
    };

    template < typename Type, typename Conversions > class storage< shared_cyclical, std::shared_ptr< Type >, Conversions >
        : public CyclicalSupport< Type >
        , public IResettable
        , public IConstructible        
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        std::shared_ptr< Type >& resolve(resolving_context& context)
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

        bool is_resolved() const { return instance_.get() != nullptr; }
        void reset() override { instance_.reset(); virtualInstance_.reset(); }

        void construct(resolving_context& context, int phase) override { CyclicalSupport< Type >::Construct(context, phase, *virtualInstance_); }

        bool HasAddress(uintptr_t address) override { return virtualInstance_->HasAddress(address); }

    private:
        std::shared_ptr< Type > instance_;
        std::shared_ptr< VirtualPointer< Type > > virtualInstance_;
    };
}