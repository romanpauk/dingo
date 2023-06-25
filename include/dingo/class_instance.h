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
    template < typename T, typename TypeStorage > class class_instance
        : public class_instance_i
        , private class_instance_destructor< T, TypeStorage::IsCaching >
    {
        typedef typename TypeStorage::Conversions Conversions;

    public:
        template < typename Ty > class_instance(Ty&& instance)
            : instance_(std::forward< Ty >(instance))
        {}

        ~class_instance()
        {
            this->destroy(instance_);
        }

        void* get_value(const std::type_info& type) override { return get_type_ptr< typename Conversions::ValueTypes >(type, instance_); }
        void* get_lvalue_reference(const std::type_info& type) override { return get_type_ptr< typename Conversions::LvalueReferenceTypes >(type, instance_); }
        void* get_rvalue_reference(const std::type_info& type) override { return get_type_ptr< typename Conversions::RvalueReferenceTypes >(type, instance_); }

        void* get_pointer(const std::type_info& type) override
        {
            void* ptr = get_type_ptr< typename Conversions::PointerTypes >(type, instance_);
            this->set_transferred(true);
            return ptr;
        }

    private:
        template< typename Ty, typename Type > static void* get_type_ptr(const std::type_info& type, Type& instance)
        {
            void* ptr = nullptr;
            if (!for_type((Ty*)0, type, [&](auto element)
            {
                if (type_traits< Type >::is_smart_ptr)
                {
                    if (type_traits< std::remove_pointer_t< std::decay_t< typename decltype(element)::type > > >::is_smart_ptr)
                    {
                        ptr = &instance;
                    }
                    else
                    {
                        ptr = type_traits< Type >::get_address(instance);
                    }
                }
                else
                {
                    ptr = type_traits< Type >::get_address(instance);
                }
            }
            ))
            {
                throw type_not_convertible_exception();
            }
            return ptr;
        }

        T instance_;
    };
}