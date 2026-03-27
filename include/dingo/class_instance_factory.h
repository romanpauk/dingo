//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/class_instance_factory_i.h>
#include <dingo/class_instance_resolver.h>
#include <dingo/rebind_type.h>
#include <dingo/resolving_context.h>

namespace dingo {
// TODO: this is bit convoluted, ideally merge resolver with factory

template <typename Container, typename Storage> struct class_instance_factory_data {
  public:
    using container_type = Container;

    template <typename ParentContainer, typename... Args>
    class_instance_factory_data(ParentContainer* parent, Args&&... args)
        : storage(std::forward<Args>(args)...),
          container(parent, parent->get_allocator()) {}

    Storage storage;
    Container container;
};

template <typename T> T& get_factory_data(T& data) { return data; }

template <typename T> T& get_factory_data(std::shared_ptr<T>& data) {
    return *data;
}

namespace detail {
template <typename Storage>
using registered_type_t = typename Storage::type;
} // namespace detail

template <typename TypeIdentity, typename Factory, typename Context>
void* resolve_address(Factory&, Context&, type_list<>,
                      const typename TypeIdentity::type_index&,
                      type_descriptor requested_type,
                      type_descriptor registered_type) {
    throw detail::make_type_not_convertible_exception(requested_type,
                                                      registered_type);
}

template <typename TypeIdentity, typename Factory, typename Context, typename Head>
void* resolve_address(Factory& factory, Context& context,
                      type_list<Head>,
                      const typename TypeIdentity::type_index& type,
                      type_descriptor requested_type,
                      type_descriptor registered_type) {
    if (!(TypeIdentity::template get<Head>() == type))
        throw detail::make_type_not_convertible_exception(
            requested_type, registered_type);
    return factory.template resolve_address<Head>(context, requested_type,
                                                  registered_type);
}

// TODO: instead of StorageTag, rvalue reference can be used to determine
// move-ability
template <typename TypeIdentity, typename Factory, typename Context,
          typename Head,
          typename Next, typename... Tail>
void* resolve_address(Factory& factory, Context& context,
                      type_list<Head, Next, Tail...>,
                      const typename TypeIdentity::type_index& type,
                      type_descriptor requested_type,
                      type_descriptor registered_type) {
    if (TypeIdentity::template get<Head>() == type) {
        return factory.template resolve_address<Head>(context, requested_type,
                                                      registered_type);
    } else {
        return resolve_address<TypeIdentity>(
            factory, context, type_list<Next, Tail...>{}, type, requested_type,
            registered_type);
    }
}

// TODO: the container here is just for type identity, but it is needed to get
// the inner container type and that is very hard. Perhaps pass type identity
// and inner container directly?
template <typename Container, typename Type, typename Storage,
          typename Data>
class class_instance_factory
    : public class_instance_factory_i<
          typename Container::container_traits_type::type_identity> {
  public:
    using storage_type = Storage;
    using data_type = std::remove_reference_t<decltype(get_factory_data(
        std::declval<Data&>()))>;
    using container_type = typename data_type::container_type;
    using type_identity =
        typename container_type::container_traits_type::type_identity;

  private:
    class_instance_resolver<type_identity, Type, Storage> resolver_;
    // `data_` must be destroyed before `resolver_` so shared storage can
    // tear down cached instances before preserved construction temporaries.
    Data data_;

    auto& get_storage() { return get_factory_data(data_).storage; }

    static constexpr type_descriptor registered_type() {
        return describe_type<detail::registered_type_t<Storage>>();
    }

  public:
    template <typename... Args>
    class_instance_factory(Args&&... args)
        : data_(std::forward<Args>(args)...) {}

    auto& get_container() { return get_factory_data(data_).container; }
    
    void*
    get_value(resolving_context& context,
              const typename type_identity::type_index& type,
              type_descriptor requested_type) override {
        using value_types = typename Storage::conversions::value_types;
        return ::dingo::resolve_address<type_identity>(
            *this, context, value_types{}, type, requested_type, registered_type());
    }

    void* get_lvalue_reference(
        resolving_context& context,
        const typename type_identity::type_index& type,
        type_descriptor requested_type) override {
        using lvalue_reference_types =
            typename Storage::conversions::lvalue_reference_types;
        return ::dingo::resolve_address<type_identity>(
            *this, context, lvalue_reference_types{}, type, requested_type,
            registered_type());
    }

    void* get_rvalue_reference(
        resolving_context& context,
        const typename type_identity::type_index& type,
        type_descriptor requested_type) override {
        using rvalue_reference_types =
            typename Storage::conversions::rvalue_reference_types;
        return ::dingo::resolve_address<type_identity>(
            *this, context, rvalue_reference_types{}, type, requested_type,
            registered_type());
    }

    void* get_pointer(
        resolving_context& context,
        const typename type_identity::type_index& type,
        type_descriptor requested_type) override {
        using pointer_types = typename Storage::conversions::pointer_types;
        return ::dingo::resolve_address<type_identity>(
            *this, context, pointer_types{}, type, requested_type,
            registered_type());
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename T, typename Context>
    void* resolve_address(Context& context, type_descriptor requested_type,
                          type_descriptor registered_type) {
        using Target = std::remove_reference_t<rebind_type_t<T, Type>>;
        using Source = decltype(resolve(context));

        return resolver_.template resolve_address<Target, Source>(context,
            get_container(), get_storage(), *this, requested_type,
            registered_type);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <typename Context> decltype(auto) resolve(Context& context) {
        return resolver_.resolve(context, get_container(), get_storage());
    }

    template <typename T, typename Context> decltype(auto) resolve(Context& context) {
        using Source = decltype(resolve(context));
        if constexpr (std::is_same_v<T, Source >)
            return resolve(context);
        else
            return resolver_.template construct_conversion<T>(context, resolve(context));
    }

    void destroy() override {
        auto allocator = allocator_traits::rebind<class_instance_factory>(
            get_container().get_allocator());
        allocator_traits::destroy(allocator, this);
        allocator_traits::deallocate(allocator, this, 1);
    }
};

// This is much faster than unique_ptr + deleter
template <typename T> struct class_instance_factory_ptr {
    class_instance_factory_ptr(T* ptr) : ptr_(ptr) {}

    class_instance_factory_ptr(class_instance_factory_ptr<T>&& other) {
        std::swap(ptr_, other.ptr_);
    }

    ~class_instance_factory_ptr() {
        if (ptr_)
            ptr_->destroy();
    }

    T& operator*() {
        assert(ptr_);
        return *ptr_;
    }

    T* get() { return ptr_; }
    T* operator->() { return ptr_; }
    operator bool() const { return ptr_ != nullptr; }

  private:
    T* ptr_ = nullptr;
};

} // namespace dingo
