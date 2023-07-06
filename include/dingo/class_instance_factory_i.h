#pragma once

namespace dingo
{
    template < typename > class resolving_context;
    template < typename > class class_instance_i;

    template < typename Container > class class_instance_factory_i {
    public:
        virtual ~class_instance_factory_i() {}
        virtual class_instance_i< typename Container::rtti_type >* resolve(resolving_context< Container >& context) = 0;
    };
}
