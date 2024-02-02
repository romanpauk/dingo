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

template <typename RTTI, typename Factory, typename Context>
void* resolve_type_conversion(Factory&, Context&, type_list<>,
                              const typename RTTI::type_index&) {
    throw type_not_convertible_exception();
}

// TODO: instead of StorageTag, rvalue reference can be used to determine
// move-ability
template <typename RTTI, typename Factory, typename Context, typename Head,
          typename... Tail>
void* resolve_type_conversion(Factory& factory, Context& context,
                              type_list<Head, Tail...>,
                              const typename RTTI::type_index& type) {
    if (RTTI::template get_type_index<Head>() == type) {
        return factory.template resolve_type_conversion<Head>(context);
    } else {
        return resolve_type_conversion<RTTI>(factory, context,
                                             type_list<Tail...>{}, type);
    }
}

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename TypeInterface, typename Storage,
          typename Data>
class class_instance_factory : public class_instance_factory_i<Container> {
  public:
    using data_traits = class_instance_data_traits<Data>;
    using storage_type = Storage;

  private:
    Data data_;

    using storage_traits = class_instance_storage_traits<Storage>;
    // TODO: move this to Data.
    using resolve_type = decltype(data_traits::get(data_).storage.resolve(
        std::declval<resolving_context&>,
        std::declval<decltype(data_traits::get(data_).container)&>));
    class_instance_resolver<typename Container::rtti_type,
                            rebind_type_t<resolve_type, TypeInterface>, Storage>
        resolver_;

  public:
    template <typename... Args>
    class_instance_factory(Args&&... args)
        : data_(std::forward<Args>(args)...) {}

    auto& get_container() { return data_traits::get(data_).container; }

    void*
    get_value(resolving_context& context,
              const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_type_conversion<typename Container::rtti_type>(
            *this, context, typename storage_traits::conversions::value_types{},
            type);
    }

    void* get_lvalue_reference(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_type_conversion<typename Container::rtti_type>(
            *this, context,
            typename storage_traits::conversions::lvalue_reference_types{},
            type);
    }

    void* get_rvalue_reference(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_type_conversion<typename Container::rtti_type>(
            *this, context,
            typename storage_traits::conversions::rvalue_reference_types{},
            type);
    }

    void* get_pointer(
        resolving_context& context,
        const typename Container::rtti_type::type_index& type) override {
        return ::dingo::resolve_type_conversion<typename Container::rtti_type>(
            *this, context,
            typename storage_traits::conversions::pointer_types{}, type);
    }

    template <typename T, typename Context>
    void* resolve_type_conversion(Context& context) {
        using Target =
            std::remove_reference_t<rebind_type_t<T, decay_t<TypeInterface>>>;
        using Source = decltype(resolve(context));

        auto&& instance =
            type_conversion<typename Storage::tag_type, Target, Source>::apply(
                *this, context, resolver_.get_temporary_context(context));
        return get_address(context, std::forward<decltype(instance)>(instance));
    }

    template <typename Context> decltype(auto) resolve(Context& context) {
        return resolver_.resolve(context, data_traits::get(data_).container,
                                 data_traits::get(data_).storage);
    }

    void destroy() override {
        auto allocator = allocator_traits::rebind<class_instance_factory>(
            get_container().get_allocator());
        allocator_traits::destroy(allocator, this);
        allocator_traits::deallocate(allocator, this, 1);
    }

    template <typename Context, typename T>
    void* get_address(Context& context, T&& instance) {
        if constexpr (std::is_reference_v<T>) {
            return &instance;
        } else if constexpr (std::is_pointer_v<T>) {
            return instance;
        } else {
            // TODO: there should be a way how to construct into memory for
            // unique storage
            return &context.template construct<T>(std::forward<T>(instance));
        }
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