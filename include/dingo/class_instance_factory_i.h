#pragma once

namespace dingo {
template <typename> class resolving_context;
template <typename> class class_instance_i;

template <typename Container> class class_instance_factory_i {
  public:
    virtual ~class_instance_factory_i() = default;

    virtual void* get_value(resolving_context<Container>&, const typename Container::rtti_type::type_index&) = 0;
    virtual void* get_lvalue_reference(resolving_context<Container>&,
                                       const typename Container::rtti_type::type_index&) = 0;
    virtual void* get_rvalue_reference(resolving_context<Container>&,
                                       const typename Container::rtti_type::type_index&) = 0;
    virtual void* get_pointer(resolving_context<Container>&, const typename Container::rtti_type::type_index&) = 0;
};
} // namespace dingo
