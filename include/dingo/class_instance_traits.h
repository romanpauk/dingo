#pragma once

#include <memory>

namespace dingo
{
    template < typename RTTI, typename T > struct class_instance_traits {
        static T get(class_instance_i<RTTI>& instance) { 
            return *static_cast<T*>(instance.get_value(RTTI::template get_type_index<rebind_type_t< T, runtime_type >>())); 
        }
    };

    template < typename RTTI, typename T > struct class_instance_traits<RTTI, T&> {
        static T& get(class_instance_i<RTTI>& instance) { 
            return *static_cast<T*>(instance.get_lvalue_reference(RTTI::template get_type_index<rebind_type_t< T&, runtime_type >>())); }
    };

    template < typename RTTI, typename T > struct class_instance_traits<RTTI, T&&> {
        static T&& get(class_instance_i<RTTI>& instance) { 
            return std::move(*static_cast<T*>(instance.get_rvalue_reference(RTTI::template get_type_index<rebind_type_t< T&&, runtime_type >>()))); 
        }
    };

    template < typename RTTI, typename T > struct class_instance_traits<RTTI, T*> {
        static T* get(class_instance_i<RTTI>& instance) { 
            return static_cast<T*>(instance.get_pointer(RTTI::template get_type_index<rebind_type_t< T*, runtime_type >>())); 
        }
    };

    template < typename RTTI, typename T > struct class_instance_traits< RTTI, std::unique_ptr< T > > {
        static std::unique_ptr< T > get(class_instance_i<RTTI>& instance) { 
            return std::move(*static_cast<std::unique_ptr< T >*>(instance.get_value(RTTI::template get_type_index<rebind_type_t< std::unique_ptr< T >, runtime_type >>()))); 
        }
    };
}