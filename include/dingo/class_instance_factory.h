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
    using container_type = Container;

    template <typename ParentContainer, typename... Args>
    class_instance_data(ParentContainer* parent, Args&&... args)
        : storage(std::forward<Args>(args)...),
          container(parent, parent->get_allocator()) {}

    Storage storage;
    Container container;
};

template <typename T> struct class_instance_data_traits {
    using container_type = typename T::container_type;
    static T& get(T& storage) { return storage; }
};

template <typename T> struct class_instance_data_traits<std::shared_ptr<T>> {
    using container_type = typename T::container_type;
    static T& get(std::shared_ptr<T>& storage) { return *storage; }
};

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename TypeInterface, typename Storage,
          typename Data>
class class_instance_factory : public class_instance_factory_i<Container> {
  public:
    using data_traits = class_instance_data_traits<Data>;

  private:
    Data data_;

    using storage_traits = class_instance_storage_traits<Storage>;
    // TODO: move this to Data.
    using ResolveType = decltype(data_traits::get(data_).storage.resolve(
        std::declval<resolving_context&>,
        std::declval<decltype(data_traits::get(data_).container)&>));
    using InterfaceType = rebind_type_t<ResolveType, TypeInterface>;
    class_instance_resolver<typename Container::rtti_type, InterfaceType,
                            Storage>
        resolver_;

  public:
    template <typename... Args>
    class_instance_factory(Args&&... args)
        : data_(std::forward<Args>(args)...) {}

    auto& get_container() { return data_traits::get(data_).container; }

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