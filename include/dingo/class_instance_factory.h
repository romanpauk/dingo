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

template <typename T> struct class_instance_factory_data_traits {
    using container_type = typename T::container_type;
    static T& get_data(T& data) { return data; }
};

template <typename T> struct class_instance_factory_data_traits<std::shared_ptr<T>> {
    using container_type = typename T::container_type;
    static T& get_data(std::shared_ptr<T>& data) { return *data; }
};

template <typename RTTI, typename Factory, typename Context>
void* resolve_address(Factory&, Context&, type_list<>,
                              const typename RTTI::type_index&) {
    throw type_not_convertible_exception();
}

// TODO: instead of StorageTag, rvalue reference can be used to determine
// move-ability
template <typename RTTI, typename Factory, typename Context, typename Head,
          typename... Tail>
void* resolve_address(Factory& factory, Context& context,
                              type_list<Head, Tail...>,
                              const typename RTTI::type_index& type) {
    if (RTTI::template get_type_index<Head>() == type) {
        return factory.template resolve_address<Head>(context);
    } else {
        return resolve_address<RTTI>(factory, context,
                                             type_list<Tail...>{}, type);
    }
}

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename Type, typename Storage,
          typename Data>
class class_instance_factory : public class_instance_factory_i<Container> {
  public:
    using storage_type = Storage;
    using container_type = typename class_instance_factory_data_traits<Data>::container_type;

  private:
    Data data_;
    class_instance_resolver<typename Container::rtti_type, Type, Storage> resolver_;

    auto& get_storage() { return class_instance_factory_data_traits<Data>::get_data(data_).storage; }

  public:
    template <typename... Args>
    class_instance_factory(Args&&... args)
        : data_(std::forward<Args>(args)...) {}

    auto& get_container() { return class_instance_factory_data_traits<Data>::get_data(data_).container; }
    
    void*
    get_value(resolving_context& context,
              const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_address<typename Container::rtti_type>(
            *this, context, typename Storage::conversions::value_types{},
            type);
    }

    void* get_lvalue_reference(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_address<typename Container::rtti_type>(
            *this, context,
            typename Storage::conversions::lvalue_reference_types{},
            type);
    }

    void* get_rvalue_reference(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_address<typename Container::rtti_type>(
            *this, context,
            typename Storage::conversions::rvalue_reference_types{},
            type);
    }

    void* get_pointer(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_address<typename Container::rtti_type>(
            *this, context,
            typename Storage::conversions::pointer_types{}, type);
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename T, typename Context>
    void* resolve_address(Context& context) {
        using Target = std::remove_reference_t<rebind_type_t<T, Type>>;
        using Source = decltype(resolve(context));

        return resolver_.template resolve_address<Target, Source>(context,
            get_container(), get_storage(), *this);
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
