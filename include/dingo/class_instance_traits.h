#pragma once

#include <memory>

namespace dingo
{
    template < class T > struct class_instance_traits
    {
        static T get(class_instance_i& instance) { return *static_cast<T*>(instance.get_value(typeid(typename rebind_type< T, runtime_type >::type))); }
    };

    template < class T > struct class_instance_traits<T&>
    {
        static T& get(class_instance_i& instance) { return *static_cast<std::decay_t< T >*>(instance.get_lvalue_reference(typeid(typename rebind_type< T&, runtime_type >::type))); }
    };

    template < class T > struct class_instance_traits<T&&>
    {
        static T&& get(class_instance_i& instance) { return std::move(*static_cast<std::decay_t< T >*>(instance.get_rvalue_reference(typeid(typename rebind_type< T&&, runtime_type >::type)))); }
    };

    template < class T > struct class_instance_traits<T*>
    {
        static T* get(class_instance_i& instance) { return static_cast<T*>(instance.get_pointer(typeid(typename rebind_type< T*, runtime_type >::type))); }
    };

    template < class T > struct class_instance_traits< std::unique_ptr< T > >
    {
        static std::unique_ptr< T > get(class_instance_i& instance) { return std::move(*static_cast<std::unique_ptr< T >*>(instance.get_value(typeid(typename rebind_type< std::unique_ptr< T >, runtime_type >::type)))); }
    };
}