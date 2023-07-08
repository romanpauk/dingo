#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/storage.h>
#include <dingo/memory/virtual_pointer.h>
#include <dingo/constructible_i.h>

namespace dingo
{
/*
    template < typename T > class ContextTracking
    {
    public:
        ContextTracking() { CurrentContext = static_cast < T* >(this); }
        ~ContextTracking() { CurrentContext = nullptr; }

        static thread_local T* CurrentContext;
    };

    template < typename T > thread_local T* ContextTracking< T >::CurrentContext;
*/
    struct shared_cyclical_protected {};

    template < typename Type, typename U > struct conversions< shared_cyclical_protected, Type, U >: conversions< shared, Type, U > {};
    template < typename Type, typename U > struct conversions< shared_cyclical_protected, std::shared_ptr< Type >, U >: conversions< shared, std::shared_ptr< Type >, U > {};

    template < typename Type > struct cyclical_support_protected
    {
    protected:
        class seh_translator
        {
        public:
            seh_translator(_se_translator_function translator): translator_(_set_se_translator(translator)) {}
            ~seh_translator() { _set_se_translator(translator_); }

        private:
            const _se_translator_function translator_;
        };

        template < typename Container > void construct(resolving_context< Container >& context, int phase, VirtualPointer< Type >& instance)
        {
            instance.SetAccessible(true);

            if (phase == 0)
            {
                seh_translator translator(seh_handler);
                class_factory< decay_t< Type > >::template construct< Type*, constructor_argument< Type, Container > >(context, instance.Get());
                
                instance.SetConstructed();
                instance.SetAccessible(false);
            }
        }

    private:
        static void seh_handler(unsigned int u, EXCEPTION_POINTERS* ex)
        {
            if (ex->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
            {
                if (ContextTracking< resolving_context >::CurrentContext->has_constructible_address((uintptr_t)ex->ExceptionRecord->ExceptionInformation[1]))
                {
                    throw type_not_constructed_exception();
                }
            }
        }
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared_cyclical_protected, Type, Conversions >
        : public cyclical_support_protected< Type >
        , public resettable_i
        , public constructible_i< Container >
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        Type* resolve(resolving_context< Container >& context)
        {
            if (!instance_.Get())
            {
                instance_.Reserve();
                context.register_constructible(this);
            }

            return instance_.Get();
        }

        bool is_resolved() const { return instance_.Get(); }
        void reset() override { instance_.Reset(); }

        void construct(resolving_context< Container >& context, int phase) override { cyclical_support_protected< Type >::construct(context, phase, instance_); }

        bool has_address(uintptr_t address) override { return instance_.HasAddress(address); }

    private:
        VirtualPointer< Type > instance_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, shared_cyclical_protected, std::shared_ptr< Type >, Conversions >
        : public cyclical_support_protected< Type >
        , public resettable_i
        , public constructible_i < Container >   
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        std::shared_ptr< Type >& resolve(resolving_context< Container >& context)
        {
            if (!instance_)
            {
                virtualInstance_ = std::make_shared< VirtualPointer< Type > >();
                virtualInstance_->Reserve();

                auto virtualInstance = virtualInstance_;
                instance_.reset(virtualInstance_->Get(), [virtualInstance](Type*) {});

                context.register_constructible(this);
            }

            return instance_;
        }

        bool is_resolved() const { return instance_.get() != nullptr; }
        void reset() override { instance_.reset(); virtualInstance_.reset(); }

        void construct(resolving_context< Container >& context, int phase) override { cyclical_support_protected< Type >::construct(context, phase, *virtualInstance_); }

        bool has_address(uintptr_t address) override { return virtualInstance_->HasAddress(address); }

    private:
        std::shared_ptr< Type > instance_;
        std::shared_ptr< VirtualPointer< Type > > virtualInstance_;
    };
}