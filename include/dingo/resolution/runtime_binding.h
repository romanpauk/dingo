//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/binding_resolution.h>
#include <dingo/resolution/runtime_binding_interface.h>
#include <dingo/runtime/context.h>
#include <dingo/type/rebind_type.h>

namespace dingo {
// TODO: this is bit convoluted, ideally merge resolver with runtime binding

template <typename Container, typename Storage,
          typename ResolutionContainer = Container>
struct runtime_binding_data {
  public:
    using container_type = Container;
    using resolution_container_type = ResolutionContainer;

    template <typename ParentContainer, typename... Args>
    runtime_binding_data(ParentContainer* parent, Args&&... args)
        : storage(std::forward<Args>(args)...),
          container(parent, parent->get_allocator()),
          resolution_container(parent, parent->get_allocator()) {}

    Storage storage;
    Container container;
    ResolutionContainer resolution_container;
};

template <typename Container, typename Storage>
struct runtime_binding_data<Container, Storage, Container> {
  public:
    using container_type = Container;
    using resolution_container_type = Container;

    template <typename ParentContainer, typename... Args>
    runtime_binding_data(ParentContainer* parent, Args&&... args)
        : storage(std::forward<Args>(args)...),
          container(parent, parent->get_allocator()) {}

    Storage storage;
    Container container;
};

template <typename T> T& get_runtime_binding_data(T& data) { return data; }

template <typename T> T& get_runtime_binding_data(std::shared_ptr<T>& data) {
    return *data;
}

template <typename T, typename = void>
struct has_runtime_binding_resolution_container_member : std::false_type {};

template <typename T>
struct has_runtime_binding_resolution_container_member<
    T, std::void_t<decltype(std::declval<T&>().resolution_container)>>
    : std::true_type {};

template <typename T> auto& get_runtime_binding_resolution_container(T& data) {
    auto& binding_data = get_runtime_binding_data(data);
    if constexpr (has_runtime_binding_resolution_container_member<
                      std::remove_reference_t<decltype(binding_data)>>::value) {
        return binding_data.resolution_container;
    } else {
        return binding_data.container;
    }
}

namespace detail {
template <typename Storage> using registered_type_t = typename Storage::type;

template <typename Type, typename Storage>
using runtime_binding_conversion_types_t =
    rebind_leaf_t<typename Storage::conversions::conversion_types, Type>;

template <typename Types>
static constexpr bool runtime_binding_has_conversion_cache_v =
    type_list_size_v<Types> != 0;

} // namespace detail

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename Type, typename Storage, typename Data>
class runtime_binding
    : public runtime_binding_interface<Container>,
      private detail::binding_conversion_cache_base<
          Storage::conversions::is_stable &&
              detail::runtime_binding_has_conversion_cache_v<
                  detail::runtime_binding_conversion_types_t<Type, Storage>>,
          detail::runtime_binding_conversion_types_t<Type, Storage>> {
  public:
    using storage_type = Storage;
    using data_type = std::remove_reference_t<decltype(get_runtime_binding_data(
        std::declval<Data&>()))>;
    using container_type = typename data_type::container_type;
    using rtti_type = typename Container::rtti_type;
    using type_index = typename rtti_type::type_index;
    using request_type = instance_request<rtti_type>;
    using conversion_types =
        detail::runtime_binding_conversion_types_t<Type, Storage>;
    static constexpr bool has_conversion_cache =
        detail::runtime_binding_has_conversion_cache_v<conversion_types>;
    static constexpr bool uses_cached_conversions =
        Storage::conversions::is_stable && has_conversion_cache;
    using materialization_traits =
        storage_materialization_traits<typename Storage::tag_type,
                                       typename Storage::type>;
    using conversion_cache_base =
        detail::binding_conversion_cache_base<uses_cached_conversions,
                                              conversion_types>;

  private:
    using conversion_cache_base::construct_conversion;

    struct binding_activation {
        runtime_binding& binding;

        template <typename T, typename Context, typename Source>
        decltype(auto) construct_conversion(Context& context, Source&& source) {
            return binding.template construct_conversion<T>(
                context, std::forward<Source>(source));
        }

        template <typename T, typename Context, typename Source>
        decltype(auto) resolve_conversion(Context& context, Source&& source) {
            return binding.template resolve_conversion<T>(
                context, std::forward<Source>(source));
        }
    };

    detail::context_closure closure_;
    // `data_` must be destroyed before binding state so shared storage can
    // tear down cached instances before preserved construction temporaries.
    Data data_;

    auto& get_storage() { return get_runtime_binding_data(data_).storage; }
    auto& get_resolution_container() {
        return get_runtime_binding_resolution_container(data_);
    }

    static constexpr type_descriptor registered_type() {
        return describe_type<detail::registered_type_t<Storage>>();
    }

  public:
    template <typename T, typename Context, typename Source>
    decltype(auto) resolve_conversion(Context& context, Source&& source) {
        binding_activation activation{*this};
        return detail::resolve_binding_conversion<T>(
            get_storage(), activation, context, std::forward<Source>(source));
    }

    template <typename ConversionTypes>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    void* convert(runtime_context& context, const request_type& request,
                  instance_cache_sink cache) {
        void* ptr = ::dingo::resolve_binding_capability_address<rtti_type>(
            *this, context, ConversionTypes{}, request.lookup_type,
            request.requested_type, registered_type());
        // Request caching is intentionally stricter than conversion caching.
        // shared_cyclical shared_ptr storage, for example, can keep converted
        // handles alive in the storage while still deferring publication of a
        // request result until the outer resolve has committed successfully.
        if constexpr (Storage::conversions::is_stable) {
            cache(ptr);
        }
        return ptr;
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  public:
    template <typename... Args>
    runtime_binding(Args&&... args) : data_(std::forward<Args>(args)...) {}

    auto& get_container() { return get_runtime_binding_data(data_).container; }

    void* get_value(runtime_context& context, const request_type& request,
                    instance_cache_sink cache) override {
        return convert<typename Storage::conversions::value_types>(
            context, request, cache);
    }

    void* get_lvalue_reference(runtime_context& context,
                               const request_type& request,
                               instance_cache_sink cache) override {
        return convert<typename Storage::conversions::lvalue_reference_types>(
            context, request, cache);
    }

    void* get_rvalue_reference(runtime_context& context,
                               const request_type& request,
                               instance_cache_sink cache) override {
        return convert<typename Storage::conversions::rvalue_reference_types>(
            context, request, cache);
    }

    void* get_pointer(runtime_context& context, const request_type& request,
                      instance_cache_sink cache) override {
        return convert<typename Storage::conversions::pointer_types>(
            context, request, cache);
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename T, typename Context>
    void* resolve_address(Context& context, type_descriptor requested_type,
                          type_descriptor registered_type) {
        if constexpr (is_exact_lookup_v<T>) {
            if (!detail::matches_exact_lookup<resolved_type_t<T, Type>>(
                    requested_type)) {
                throw detail::make_type_not_convertible_exception(
                    requested_type, registered_type, context);
            }
        }

        using Target = std::remove_reference_t<resolved_type_t<T, Type>>;
        return detail::materialize_binding_resolution_source(
            context, get_storage(), get_resolution_container(), closure_,
            [&](auto&& source) -> void* {
                return detail::resolve_binding_address_from_source<Target>(
                    *this, context, std::forward<decltype(source)>(source),
                    requested_type, registered_type);
            });
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <typename Context> decltype(auto) resolve(Context& context) {
        return detail::materialize_tracked_binding_source(
            context, get_storage(), get_resolution_container(),
            [](auto&& source) -> decltype(auto) {
                return std::forward<decltype(source)>(source).get();
            });
    }

    template <typename T, typename Context>
    decltype(auto) resolve(Context& context) {
        binding_activation activation{*this};
        return detail::materialize_tracked_binding_source(
            context, get_storage(), get_resolution_container(),
            [&](auto&& source) -> decltype(auto) {
                return detail::resolve_binding_value<T>(
                    activation, context,
                    std::forward<decltype(source)>(source));
            });
    }

    void destroy() override {
        auto allocator = allocator_traits::rebind<runtime_binding>(
            get_container().get_allocator());
        allocator_traits::destroy(allocator, this);
        allocator_traits::deallocate(allocator, this, 1);
    }
};

// This is much faster than unique_ptr + deleter
template <typename T> struct runtime_binding_ptr {
    using destroy_fn = void (*)(T*);

    runtime_binding_ptr(T* ptr = nullptr, destroy_fn destroy = &default_destroy)
        : ptr_(ptr), destroy_(destroy) {}

    runtime_binding_ptr(const runtime_binding_ptr&) = delete;
    runtime_binding_ptr& operator=(const runtime_binding_ptr&) = delete;

    runtime_binding_ptr(runtime_binding_ptr<T>&& other) noexcept
        : ptr_(other.release()), destroy_(other.destroy_) {
        other.destroy_ = &default_destroy;
    }

    runtime_binding_ptr& operator=(runtime_binding_ptr<T>&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.release();
            destroy_ = other.destroy_;
            other.destroy_ = &default_destroy;
        }
        return *this;
    }

    ~runtime_binding_ptr() { reset(); }

    T& operator*() {
        assert(ptr_);
        return *ptr_;
    }

    T* release() {
        T* ptr = ptr_;
        ptr_ = nullptr;
        return ptr;
    }

    void reset(T* ptr = nullptr, destroy_fn destroy = &default_destroy) {
        if (ptr_) {
            destroy_(ptr_);
        }
        ptr_ = ptr;
        destroy_ = destroy;
    }

    T* get() { return ptr_; }
    T* operator->() { return ptr_; }
    operator bool() const { return ptr_ != nullptr; }

  private:
    static void default_destroy(T* ptr) { ptr->destroy(); }

    T* ptr_ = nullptr;
    destroy_fn destroy_ = &default_destroy;
};

} // namespace dingo
