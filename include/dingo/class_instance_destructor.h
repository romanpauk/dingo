#pragma once

namespace dingo
{
    template< class T, bool > struct class_instance_destructor
    {
        void set_transferred(bool) {}
        void destroy(T&) {}
    };

    template< class T > struct class_instance_destructor< T*, false >
    {
        void set_transferred(bool value) { transferred_ = value; }
        void destroy(T* ptr) { if (!transferred_) delete ptr; }

    private:
        bool transferred_ = false;
    };
}