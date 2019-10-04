#pragma once

#include <windows.h>

namespace dingo
{
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
                address_ = VirtualAlloc(0, size_, MEM_COMMIT, PAGE_NOACCESS);
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
}
