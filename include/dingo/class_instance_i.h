#pragma once

#include <typeinfo>

namespace dingo
{
    class class_instance_i
    {
    public:
        virtual ~class_instance_i() {};

        virtual void* get_value(const std::type_info&) = 0;
        virtual void* get_lvalue_reference(const std::type_info&) = 0;
        virtual void* get_rvalue_reference(const std::type_info&) = 0;
        virtual void* get_pointer(const std::type_info&) = 0;
    };
}