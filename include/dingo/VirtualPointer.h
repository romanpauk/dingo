#pragma once

#include "dingo/Exceptions.h"
#include "dingo/ScopeGuard.h"

#include <windows.h>

namespace dingo
{
    template < typename T > class VirtualPointer
    {
    public:
        VirtualPointer()
            : instance_()
        {}

        ~VirtualPointer()
        {
            Reset();
         }

        void SetAccessible(bool accessible)
        {
            DWORD old;
            if(!VirtualProtect(instance_, sizeof(T), accessible ? PAGE_READWRITE : PAGE_NOACCESS, &old))
            {
                throw VirtualPointerException();
            }
        }

        T* Get() { return instance_; }
        const T* Get() const { return instance_; }

        void Reserve()
        {
            if (!instance_)
            {
                instance_ = reinterpret_cast<T*>(VirtualAlloc(0, sizeof(T), MEM_COMMIT, PAGE_NOACCESS));
                if(!instance_)
                {
                    throw VirtualPointerException();
                }
            }
        }

        void Reset()
        {
            if (instance_)
            {
                auto instance = instance_;
                instance_ = 0;

                auto guard = MakeScopeGuard([instance]
                {
                    if (!VirtualFree(instance, 0, MEM_RELEASE))
                    {
                        throw VirtualPointerException();
                    }
                });

                instance->~T();
            }
        }

        bool HasAddress(uintptr_t address) const
        {
            auto instanceAddress = reinterpret_cast<uintptr_t>(instance_);
            return address >= instanceAddress && address < instanceAddress + sizeof(T);
        }

    private:
        T* instance_;
    };

}
