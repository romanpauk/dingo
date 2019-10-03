#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/Type.h"
#include "dingo/Storage.h"

#include <windows.h>

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
    
    class VirtualBuffer
    {
    public:
        VirtualBuffer(size_t size)
            : address_()
            , size_(size)
        {}

        ~VirtualBuffer()
        {
            Reset();
        }

        void SetAccessible(bool accessible)
        {
            // TODO: retval
            DWORD old;
            VirtualProtect(address_, size_, accessible ? PAGE_READWRITE : PAGE_NOACCESS, &old);
        }

        void* GetPointer() 
        {
            if (!address_)
            {
                // TODO: retval
                address_ = VirtualAlloc(0, size_, MEM_COMMIT, PAGE_READWRITE);
            }

            return address_;
        }

        void Reset()
        {
            if (address_)
            {
                VirtualFree(address_, 0, MEM_RELEASE);
                address_ = 0;
            }
        }

        size_t Size() const { return size_; }

    private:
        void* address_;
        size_t size_;
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
                initialized_ = true;

                context.AddConstructible(this);
                buffer_.SetAccessible(false);
            }

            return reinterpret_cast<Type*>(buffer_.GetPointer());
        }

        bool IsResolved() const { return initialized_; }

        void Reset() override
        {
            if (initialized_)
            {
                initialized_ = false;
                
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

        bool HasAddress(void* ptr) override
        {
            auto address = reinterpret_cast<uintptr_t>(ptr);
            auto bufferAddress = reinterpret_cast<uintptr_t>(buffer_.GetPointer());
            return address >= bufferAddress && address < bufferAddress + buffer_.Size();
        }

    private:
        int FilterException(Context& context, unsigned int code, struct _EXCEPTION_POINTERS* ep)
        {
            if (code == EXCEPTION_ACCESS_VIOLATION)
            {
                return EXCEPTION_EXECUTE_HANDLER;

                // TODO: bug here...
                if(context.IsConstructibleAddress(ep->ExceptionRecord->ExceptionAddress))
                    return EXCEPTION_EXECUTE_HANDLER;
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }

        VirtualBuffer buffer_;
        bool initialized_;
    };
}