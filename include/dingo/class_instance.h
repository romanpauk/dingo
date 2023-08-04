#pragma once

#include <dingo/type_list.h>
#include <dingo/exceptions.h>
#include <dingo/rebind_type.h>
#include <dingo/class_instance_i.h>
#include <dingo/type_traits.h>
#include <dingo/class_instance_traits.h>
#include <dingo/class_instance_destructor.h>

namespace dingo
{
    template < typename RTTI, typename T, typename TypeStorage > class class_instance
        : public class_instance_destructor< RTTI, T, TypeStorage >
    {
        using conversions = typename TypeStorage::conversions;

    public:
        template < typename Ty > class_instance(Ty&& instance)
            : instance_(std::forward< Ty >(instance))
        {}

        // TODO: this is a bit convoluted, destroy() is invoked from class_instance_destructor
        // (if required). The intention is for class_instance to be trivially destructible if 
        // the instance_ is.
        void destroy() {
            class_instance_destructor< RTTI, T, TypeStorage >::destroy(instance_);
        }

        void* get_value(const typename RTTI::type_index& type) override { 
            return get_type_ptr< typename conversions::value_types >(type, instance_); 
        }

        void* get_lvalue_reference(const typename RTTI::type_index& type) override { 
            return get_type_ptr< typename conversions::lvalue_reference_types >(type, instance_); 
        }
        
        void* get_rvalue_reference(const typename RTTI::type_index& type) override { 
            return get_type_ptr< typename conversions::rvalue_reference_types >(type, instance_); 
        }

        void* get_pointer(const typename RTTI::type_index& type) override {
            void* ptr = get_type_ptr< typename conversions::pointer_types >(type, instance_);
            this->set_transferred(true);
            return ptr;
        }

    private:
        template< typename Types, typename Type > static void* get_type_ptr(const typename RTTI::type_index& type, Type& instance) {
            void* ptr = nullptr;
            if (!for_type<RTTI>(Types{}, type, [&](auto element) {
                if (type_traits< Type >::is_smart_ptr) {
                    if (type_traits< std::remove_pointer_t< std::decay_t< typename decltype(element)::type > > >::is_smart_ptr) {
                        ptr = &instance;
                    } else {
                        ptr = type_traits< Type >::get_address(instance);
                    }
                }
                else {
                    ptr = type_traits< Type >::get_address(instance);
                }
            }))
            {
                throw type_not_convertible_exception();
            }
            return ptr;
        }

        T instance_;
    };
}