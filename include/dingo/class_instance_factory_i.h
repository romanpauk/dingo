#pragma once

namespace dingo
{
    class resolving_context;
    class class_instance_i;

    class class_instance_factory_i
    {
    public:
        virtual ~class_instance_factory_i() {}

        virtual class_instance_i* resolve(resolving_context& context) = 0;
        virtual bool is_persistent() = 0;
        virtual bool is_resolved() = 0;
    };
}
