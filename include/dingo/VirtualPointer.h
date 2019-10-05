#pragma once

#include "dingo/Exceptions.h"
#include "dingo/ScopeGuard.h"

#include <windows.h>
#include <cassert>

namespace dingo
{
    class VirtualBuffer
    {
    public:
        VirtualBuffer()
            : instance_()
        {}

        ~VirtualBuffer()
        {
            Reset();
        }

        void Reserve(size_t size)
        {
            assert(!instance_);

            instance_ = VirtualAlloc(0, size, MEM_COMMIT, PAGE_NOACCESS);
            if (!instance_)
            {
                throw VirtualPointerException();
            }
        }

        void Reset()
        {
            if(instance_)
            {
                auto instance = instance_;
                instance_ = 0;

                if (!VirtualFree(instance, 0, MEM_RELEASE))
                {
                    throw VirtualPointerException();
                }
            }
        }

        void* Get() { return instance_; }
        const void* Get() const { return instance_; }

        void SetAccessible(bool accessible, size_t size)
        {
            DWORD old;
            if (!VirtualProtect(instance_, size, accessible ? PAGE_READWRITE : PAGE_NOACCESS, &old))
            {
                throw VirtualPointerException();
            }
        }
    private:
        void* instance_;
    };

    template < typename T > class VirtualPointer
    {
    public:
        VirtualPointer()
            : constructed_()
        {}

        ~VirtualPointer()
        {
            Reset();
        }

        void SetConstructed() { constructed_ = true; }
        void SetAccessible(bool accessible) { buffer_.SetAccessible(accessible, sizeof(T)); }

        T* Get() { return reinterpret_cast< T* >(buffer_.Get()); }
        const T* Get() const { return reinterpret_cast<const T*>(buffer_.Get()); }

        void Reserve() { buffer_.Reserve(sizeof(T)); }

        void Reset()
        {
            if(constructed_)
            {
                // Make sure we can call the dtor
                SetAccessible(true);
            }

            auto guard = MakeScopeGuard([&] { buffer_.Reset(); });

            if(constructed_)
            {
                constructed_ = false;
                Get()->~T();
            }
        }

        bool HasAddress(uintptr_t address) const
        {
            auto instanceAddress = reinterpret_cast<uintptr_t>(buffer_.Get());
            return address >= instanceAddress && address < instanceAddress + sizeof(T);
        }

    private:
        VirtualBuffer buffer_;
        bool constructed_;
    };

}
