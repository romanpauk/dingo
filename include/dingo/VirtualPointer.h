#pragma once

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
            // TODO: retval
            DWORD old;
            VirtualProtect(instance_, sizeof(T), accessible ? PAGE_READWRITE : PAGE_NOACCESS, &old);
        }

        T* Get() { return instance_; }
        const T* Get() const { return instance_; }

        void Reserve()
        {
            if (!instance_)
            {
                // TODO: retval
                instance_ = reinterpret_cast<T*>(VirtualAlloc(0, sizeof(T), MEM_COMMIT, PAGE_NOACCESS));
            }
        }

        void Reset()
        {
            if (instance_)
            {
                instance_->~T();

                VirtualFree(instance_, 0, MEM_RELEASE);
                instance_ = 0;
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
