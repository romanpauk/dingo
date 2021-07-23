#pragma once

namespace dingo
{
    template < typename > class resolving_context;
    class class_instance_i;

    template < typename Container > class class_instance_factory_i
    {
    public:
        virtual ~class_instance_factory_i() {}

        virtual class_instance_i* resolve(resolving_context< Container >& context) = 0;
        virtual bool is_persistent() = 0;
        virtual bool is_resolved() = 0;
    };
}
