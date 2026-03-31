//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/resolution/instance_factory_interface.h>
#include <dingo/resolution/instance_resolver.h>
#include <dingo/type/rebind_type.h>
#include <dingo/resolution/resolving_context.h>

#include <utility>

namespace dingo {
// TODO: this is bit convoluted, ideally merge resolver with factory

template <typename Container, typename Storage> struct instance_factory_data {
  public:
    using container_type = Container;

    template <typename ParentContainer, typename... Args>
    instance_factory_data(ParentContainer* parent, Args&&... args)
        : storage(std::forward<Args>(args)...),
          container(parent, parent->get_allocator()) {}

    Storage storage;
    Container container;
};

template <typename T> T& get_instance_factory_data(T& data) { return data; }

template <typename T> T& get_instance_factory_data(std::shared_ptr<T>& data) {
    return *data;
}

namespace detail {
template <typename Storage>
using registered_type_t = typename Storage::type;

template <typename Storage, typename Context, typename Container>
using storage_resolve_result_t = decltype(
    std::declval<Storage&>().resolve(std::declval<Context&>(),
                                     std::declval<Container&>()));

template <typename ExactLookup>
constexpr bool matches_exact_lookup(type_descriptor requested_type) {
    if (requested_type == describe_type<ExactLookup>()) {
        return true;
    }

    if constexpr (std::is_lvalue_reference_v<ExactLookup>) {
        using target_type = std::remove_reference_t<ExactLookup>;
        return requested_type == describe_type<const target_type&>();
    } else if constexpr (std::is_pointer_v<ExactLookup>) {
        using target_type = std::remove_pointer_t<ExactLookup>;
        return requested_type == describe_type<const target_type*>();
    } else {
        return false;
    }
}
} // namespace detail

template <typename RTTI, typename Factory, typename Context, typename... Types>
void* resolve_address(Factory& factory, Context& context, type_list<Types...>,
                      const typename RTTI::type_index& type,
                      type_descriptor requested_type,
                      type_descriptor registered_type) {
    void* address = nullptr;
    const bool matched =
        ((RTTI::template get_type_index<lookup_type_t<Types>>() == type
              ? (address = factory.template resolve_address<Types>(
                     context, requested_type, registered_type),
                 true)
              : false) ||
         ...);

    if (!matched) {
        throw detail::make_type_not_convertible_exception(requested_type,
                                                          registered_type);
    }

    return address;
}

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename Type, typename Storage,
          typename Data>
class instance_factory : public instance_factory_interface<Container> {
  public:
    using storage_type = Storage;
    using data_type = std::remove_reference_t<decltype(get_instance_factory_data(
        std::declval<Data&>()))>;
    using container_type = typename data_type::container_type;
    using rtti_type = typename Container::rtti_type;
    using type_index = typename rtti_type::type_index;

    template <typename Request>
    using request_plan_t = detail::request_ir_t<Request>;
    using source_type =
        detail::storage_resolve_result_t<Storage, resolving_context,
                                         container_type>;

  private:
    instance_resolver<rtti_type, Type, Storage> resolver_;
    // `data_` must be destroyed before `resolver_` so shared storage can
    // tear down cached instances before preserved construction temporaries.
    Data data_;

    auto& get_storage() { return get_instance_factory_data(data_).storage; }

    static constexpr type_descriptor registered_type() {
        return describe_type<detail::registered_type_t<Storage>>();
    }

    template <typename ConversionTypes>
    void* get_converted(resolving_context& context, const type_index& type,
                        type_descriptor requested_type) {
        return ::dingo::resolve_address<rtti_type>(
            *this, context, ConversionTypes{}, type, requested_type,
            registered_type());
    }

  public:
    template <typename... Args>
    instance_factory(Args&&... args)
        : data_(std::forward<Args>(args)...) {}

    template <typename Request>
    using plan_type = ir::resolution<
        request_plan_t<Request>, detail::binding_ir_t<Type, Storage>,
        detail::acquisition_ir_t<Storage>,
        detail::factory_invocation_ir_t<typename Storage::factory_type>,
        detail::selected_request_conversion_ir_t<
            request_plan_t<Request>, Type, Storage, source_type,
            typename Storage::conversions>>;

    template <typename Request> static constexpr auto plan_for() -> plan_type<Request> {
        return {};
    }

    auto& get_container() { return get_instance_factory_data(data_).container; }
    
    void*
    get_value(resolving_context& context,
              const type_index& type,
              type_descriptor requested_type) override {
        return get_converted<typename Storage::conversions::value_types>(
            context, type, requested_type);
    }

    void* get_lvalue_reference(
        resolving_context& context,
        const type_index& type,
        type_descriptor requested_type) override {
        return get_converted<
            typename Storage::conversions::lvalue_reference_types>(
            context, type, requested_type);
    }

    void* get_rvalue_reference(
        resolving_context& context,
        const type_index& type,
        type_descriptor requested_type) override {
        return get_converted<
            typename Storage::conversions::rvalue_reference_types>(
            context, type, requested_type);
    }

    void* get_pointer(
        resolving_context& context,
        const type_index& type,
        type_descriptor requested_type) override {
        return get_converted<typename Storage::conversions::pointer_types>(
            context, type, requested_type);
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename T, typename Context>
    void* resolve_address(Context& context, type_descriptor requested_type,
                          type_descriptor registered_type) {
        if constexpr (is_exact_lookup_v<T>) {
            if (!detail::matches_exact_lookup<
                    resolved_type_t<T, Type>>(requested_type)) {
                throw detail::make_type_not_convertible_exception(
                    requested_type, registered_type);
            }
        }

        using Source = decltype(resolve(context));
        using request_kind = typename detail::request_kind_ir<
            resolved_type_t<T, Type>>::type;
        using conversion =
            detail::selected_conversion_for_kind_t<request_kind, Type, Storage,
                                                   Source, T>;

        return resolver_.template resolve_address<conversion>(
            context, get_container(), get_storage(), *this, requested_type,
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
        if constexpr (std::is_same_v<T, Source>)
            return resolve(context);
        else
            return resolver_.template construct_conversion<T>(context, resolve(context));
    }

    void destroy() override {
        auto allocator = allocator_traits::rebind<instance_factory>(
            get_container().get_allocator());
        allocator_traits::destroy(allocator, this);
        allocator_traits::deallocate(allocator, this, 1);
    }
};

namespace detail {
template <typename Request, typename Container, typename Type, typename Storage,
          typename Data>
constexpr auto plan_for(instance_factory<Container, Type, Storage, Data>&)
    -> typename instance_factory<Container, Type, Storage, Data>::template plan_type<
        Request> {
    return {};
}

template <typename Request, typename Factory>
using resolution_plan_for_t =
    decltype(plan_for<Request>(std::declval<Factory&>()));
} // namespace detail

// This is much faster than unique_ptr + deleter
template <typename T> struct instance_factory_ptr {
    instance_factory_ptr(T* ptr) : ptr_(ptr) {}

    instance_factory_ptr(instance_factory_ptr<T>&& other) noexcept : ptr_(nullptr) {
        std::swap(ptr_, other.ptr_);
    }

    ~instance_factory_ptr() {
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
