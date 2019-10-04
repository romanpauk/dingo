#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/Type.h"
#include "dingo/Storage.h"
#include "dingo/VirtualBuffer.h"
#include "dingo/IConstructible.h"

namespace dingo
{
    class Container;

    template < typename Type, typename U > struct Conversions< Cyclical, Type, U >
    {
        typedef TypeList<> ValueTypes;
        typedef TypeList< U& > LvalueReferenceTypes;
        typedef TypeList<> RvalueReferenceTypes;
        typedef TypeList< U* > PointerTypes;
    };
    
    template < typename Type, typename Conversions > class Storage< Cyclical, Type, Conversions >
        : public IResettable
        , public IConstructible
    {
    public:
        static const bool IsCaching = true;

        typedef Conversions Conversions;
        typedef Type Type;

        Storage()
            : initialized_(false)
            , buffer_(sizeof(Type))
        {}

        ~Storage()
        {
            Reset();
        }

        Type* Resolve(Context& context)
        {
            if (!initialized_)
            {
                context.AddConstructible(this);
                initialized_ = true;
            }

            return reinterpret_cast<Type*>(buffer_.GetPointer());
        }

        bool IsResolved() const { return initialized_; }

        void Reset() override
        {
            if (initialized_)
            {
                initialized_ = false;
                
                // TODO: scope exit
                try
                {
                    reinterpret_cast<Type*>(buffer_.GetPointer())->~Type();
                    buffer_.Reset(); 
                }
                catch(...)
                {
                    buffer_.Reset(); 
                    throw;
                }
            }
        }

        void Construct(Context& context, int phase) override
        {
            buffer_.SetAccessible(true);

            if(phase == 0)
            {
                __try
                {
                    TypeFactory< typename TypeDecay< Type >::type >::template Construct< Type*, Container::ConstructorArgument< Type > >(context, buffer_.GetPointer());
                }
                __except (FilterException(context, GetExceptionCode(), GetExceptionInformation()))
                {
                    throw TypeNotConstructedException();
                }

                buffer_.SetAccessible(false);
            }
        }

        bool HasAddress(uintptr_t address) override
        {
            auto bufferAddress = reinterpret_cast<uintptr_t>(buffer_.GetPointer());
            return address >= bufferAddress && address < bufferAddress + buffer_.Size();
        }

    private:
        int FilterException(Context& context, unsigned int code, struct _EXCEPTION_POINTERS* ep)
        {
            if (code == EXCEPTION_ACCESS_VIOLATION)
            {
                if (context.IsConstructibleAddress((uintptr_t)ep->ExceptionRecord->ExceptionInformation[1]))
                    return EXCEPTION_EXECUTE_HANDLER;                
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }

        VirtualBuffer buffer_;
        bool initialized_;
    };
}