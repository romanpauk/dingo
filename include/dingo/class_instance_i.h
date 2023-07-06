#pragma once

namespace dingo
{
    template< typename RTTI > class class_instance_i {
    public:
        // Not all class instances need virtual destructor. This depends on stored type.
        // See: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1077r0.html

        virtual void* get_value(const typename RTTI::type_index&) = 0;
        virtual void* get_lvalue_reference(const typename RTTI::type_index&) = 0;
        virtual void* get_rvalue_reference(const typename RTTI::type_index&) = 0;
        virtual void* get_pointer(const typename RTTI::type_index&) = 0;
    };

    template< typename RTTI > class class_instance_destructor_i: public class_instance_i< RTTI > {
    public:
        virtual ~class_instance_destructor_i() {};
    };
}