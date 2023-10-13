#pragma once

#include <dingo/config.h>

#include <dingo/class_instance_factory_i.h>
#include <dingo/class_instance_resolver.h>
#include <dingo/rebind_type.h>
#include <dingo/resolving_context.h>

namespace dingo {
// TODO: this is bit convoluted, ideally merge resolver with factory

// TODO: remove this, needed to get conversions now
template <typename T> struct class_instance_storage_traits {
    using type = T;
    using conversions = typename T::conversions;

    static T& get(type& storage) { return storage; }
};

template <typename T> struct class_instance_storage_traits<std::shared_ptr<T>> {
    using type = std::shared_ptr<T>;
    using conversions = typename T::conversions;

    static T& get(type& storage) { return *storage; }
};

template <typename Container, typename Storage> struct class_instance_data {
  public:
    template <typename ParentContainer, typename... Args>
    class_instance_data(ParentContainer* parent, Args&&... args)
        : storage(std::forward<Args>(args)...),
          container(parent, parent->get_allocator()) {}

    Storage storage;
    Container container;
};

template <typename T> struct class_instance_data_traits {
    static T& get(T& storage) { return storage; }
};

template <typename T> struct class_instance_data_traits<std::shared_ptr<T>> {
    static T& get(std::shared_ptr<T>& storage) { return *storage; }
};

template <typename Container, typename TypeInterface, typename Storage,
          typename Data>
class class_instance_factory : public class_instance_factory_i<Container> {
    using data_traits = class_instance_data_traits<Data>;
    Data data_;

    using storage_traits = class_instance_storage_traits<Storage>;
    using ResolveType = decltype(data_traits::get(data_).storage.resolve(
        std::declval<resolving_context&>, std::declval<Container&>()));
    using InterfaceType = rebind_type_t<ResolveType, TypeInterface>;
    class_instance_resolver<typename Container::rtti_type, InterfaceType,
                            Storage>
        resolver_;

  public:
    template <typename... Args>
    class_instance_factory(Args&&... args)
        : data_(std::forward<Args>(args)...) {}

    auto& get_container() { return data_traits::get(data_).container; }

    // TODO: we will need two pairs of calls - context-aware and context-less
    // First, resolution will be attempted without context.
    //      Sufficient for trivial types or already resolved types (or types
    //      composed from those...), simply anything, that does not throw during
    //      construction.
    // Second, resolution will continue with context
    //      Could that use pointer to context, so in resolver we check if
    //      context is needed and if so, continue with pointer to the stack?

    void*
    get_value(resolving_context& context,
              const typename Container::rtti_type::type_index& type) override {
        return resolver_.template resolve<
            typename storage_traits::conversions::value_types>(
            context, data_traits::get(data_).container,
            data_traits::get(data_).storage, type);
    }

    void* get_lvalue_reference(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return resolver_.template resolve<
            typename storage_traits::conversions::lvalue_reference_types>(
            context, data_traits::get(data_).container,
            data_traits::get(data_).storage, type);
    }

    void* get_rvalue_reference(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return resolver_.template resolve<
            typename storage_traits::conversions::rvalue_reference_types>(
            context, data_traits::get(data_).container,
            data_traits::get(data_).storage, type);
    }

    void* get_pointer(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return resolver_.template resolve<
            typename storage_traits::conversions::pointer_types>(
            context, data_traits::get(data_).container,
            data_traits::get(data_).storage, type);
    }
};
} // namespace dingo