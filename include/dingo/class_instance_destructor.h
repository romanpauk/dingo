#pragma once

namespace dingo
{
    template < typename RTTI, typename T, typename TypeStorage > class class_instance;
    template< typename RTTI, typename T, bool IsTriviallyDestructible = std::is_trivially_destructible_v< T > > class class_instance_destructor_base;

    template< typename RTTI, typename T > class class_instance_destructor_base<RTTI, T, true>: public class_instance_i<RTTI> {};
    template< typename RTTI, typename T > class class_instance_destructor_base<RTTI, T, false>: public class_instance_destructor_i<RTTI> {};

    template< typename RTTI, typename T, typename TypeStorage, bool IsCaching = TypeStorage::is_caching > struct class_instance_destructor
        : class_instance_destructor_base< RTTI, T >
    {
        void set_transferred(bool) {}
        void destroy(T&) {}
    };

    // TODO: rething this case, if we create pointer, and we pass it somewhere, it is already transferred.
    // This tries to prevent a leak with something created as T* and resolved as T&, but that can't be done
    // as we can't delete it anyway...
    template< typename RTTI, typename T, typename TypeStorage > struct class_instance_destructor< RTTI, T*, TypeStorage, false >
        : class_instance_destructor_base< RTTI, T >
    {
        ~class_instance_destructor() {
            static_cast< class_instance< RTTI, T*, TypeStorage >* >(this)->destroy();
        }

        void set_transferred(bool value) { transferred_ = value; }
        void destroy(T* ptr) { if (!transferred_) delete ptr; }

    private:
        bool transferred_ = false;
    };
}