//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/resolution/conversion_cache.h>
#include <dingo/resolution/instance_factory_interface.h>
#include <dingo/resolution/type_conversion.h>
#include <dingo/storage/storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/resolution/resolving_context.h>

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

template <typename Target, typename Context, typename T>
void* get_address_as(Context& context, T&& instance) {
    if constexpr (std::is_reference_v<T>) {
        return &instance;
    } else if constexpr (std::is_pointer_v<T>) {
        return instance;
    } else {
        return &context.template construct<Target>(std::forward<T>(instance));
    }
}

template <typename Type, typename Storage>
using instance_factory_conversion_types_t =
    rebind_leaf_t<typename Storage::conversions::conversion_types, Type>;

template <typename Types>
static constexpr bool instance_factory_has_conversion_cache_v =
    type_list_size_v<Types> != 0;

template <typename Storage, typename T, typename Context, typename Source,
          typename = void>
struct has_storage_resolve_conversion : std::false_type {};

template <typename Storage, typename T, typename Context, typename Source>
struct has_storage_resolve_conversion<
    Storage, T, Context, Source,
    std::void_t<decltype(std::declval<Storage&>().template resolve_conversion<T>(
        std::declval<Context&>(), std::declval<Source>()))>> : std::true_type {};

template <bool Enabled, typename ConversionTypes>
struct instance_factory_conversion_cache_base;

template <typename ConversionTypes>
struct instance_factory_conversion_cache_base<true, ConversionTypes> {
    template <typename T, typename Context, typename... Args>
    T& construct_conversion(Context&, Args&&... args) {
        return conversions_.template construct<T>(std::forward<Args>(args)...);
    }

  protected:
    conversion_cache<ConversionTypes> conversions_;
};

template <typename ConversionTypes>
struct instance_factory_conversion_cache_base<false, ConversionTypes> {
    template <typename T, typename Context, typename... Args>
    T& construct_conversion(Context& context, Args&&... args) {
        return context.template construct<T>(std::forward<Args>(args)...);
    }
};

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
                                                          registered_type,
                                                          context);
    }

    return address;
}

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename Type, typename Storage,
          typename Data>
class instance_factory
    : public instance_factory_interface<Container>,
      private detail::instance_factory_conversion_cache_base<
          Storage::conversions::is_stable &&
              detail::instance_factory_has_conversion_cache_v<
                  detail::instance_factory_conversion_types_t<Type, Storage>>,
          detail::instance_factory_conversion_types_t<Type, Storage>> {
  public:
    using storage_type = Storage;
    using data_type = std::remove_reference_t<decltype(get_instance_factory_data(
        std::declval<Data&>()))>;
    using container_type = typename data_type::container_type;
    using rtti_type = typename Container::rtti_type;
    using type_index = typename rtti_type::type_index;
    using request_type = instance_request<rtti_type>;
    using conversion_types =
        detail::instance_factory_conversion_types_t<Type, Storage>;
    static constexpr bool has_conversion_cache =
        detail::instance_factory_has_conversion_cache_v<conversion_types>;
    static constexpr bool uses_cached_conversions =
        Storage::conversions::is_stable && has_conversion_cache;
    using materialization_traits =
        detail::storage_materialization<typename Storage::tag_type,
                                        typename Storage::conversions>;
    using conversion_cache_base =
        detail::instance_factory_conversion_cache_base<
            uses_cached_conversions, conversion_types>;

  private:
    using conversion_cache_base::construct_conversion;

    resolving_context::closure closure_;
    // `data_` must be destroyed before factory state so shared storage can
    // tear down cached instances before preserved construction temporaries.
    Data data_;

    auto& get_storage() { return get_instance_factory_data(data_).storage; }

    static constexpr type_descriptor registered_type() {
        return describe_type<detail::registered_type_t<Storage>>();
    }

    template <typename Context>
    decltype(auto) materialize_source(Context& context) {
        return materialization_traits::materialize_source(
            context, get_storage(), get_container());
    }

    template <typename T, typename Context, typename Source>
    decltype(auto) resolve_conversion(Context& context, Source&& source) {
        if constexpr (detail::has_storage_resolve_conversion<
                          storage_type, T, Context, Source&&>::value) {
            return get_storage().template resolve_conversion<T>(
                context, std::forward<Source>(source));
        } else {
            return this->template construct_conversion<T>(
                context, std::forward<Source>(source));
        }
    }

    template <typename Target, typename ConversionSource, typename Context>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    void* resolve_address_impl(Context& context,
                               ConversionSource&& source,
                               type_descriptor requested_type,
                               type_descriptor registered_type) {
        using conversion_source = std::remove_reference_t<ConversionSource>;
        auto&& instance = type_conversion<Target, conversion_source>::apply(
            *this, context, std::forward<ConversionSource>(source), requested_type,
            registered_type);
        return detail::get_address_as<Target>(
            context, std::forward<decltype(instance)>(instance));
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <typename ConversionTypes>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    void* convert(resolving_context& context, const request_type& request,
                  instance_cache_sink cache) {
        void* ptr = ::dingo::resolve_address<rtti_type>(
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
    instance_factory(Args&&... args)
        : data_(std::forward<Args>(args)...) {}

    auto& get_container() { return get_instance_factory_data(data_).container; }
    
    void* get_value(resolving_context& context, const request_type& request,
                    instance_cache_sink cache) override {
        return convert<typename Storage::conversions::value_types>(
            context, request, cache);
    }

    void* get_lvalue_reference(resolving_context& context,
                               const request_type& request,
                               instance_cache_sink cache) override {
        return convert<typename Storage::conversions::lvalue_reference_types>(
            context, request, cache);
    }

    void* get_rvalue_reference(resolving_context& context,
                               const request_type& request,
                               instance_cache_sink cache) override {
        return convert<typename Storage::conversions::rvalue_reference_types>(
            context, request, cache);
    }

    void* get_pointer(resolving_context& context,
                      const request_type& request,
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
            if (!detail::matches_exact_lookup<
                    resolved_type_t<T, Type>>(requested_type)) {
                throw detail::make_type_not_convertible_exception(
                    requested_type, registered_type, context);
            }
        }

        using Target = std::remove_reference_t<resolved_type_t<T, Type>>;
        using leaf_type = leaf_type_t<typename Storage::type>;
        if (materialization_traits::preserves_closure(get_storage())) {
            [[maybe_unused]] auto guard =
                materialization_traits::template make_guard<leaf_type>(
                    context, get_storage());
            // Match the old resolver semantics: keep the preserved closure on
            // the resolving_context stack until container::resolve() exits if
            // this path throws. On success, detach it immediately after the
            // shared instance and its conversions have been materialized. The
            // recursion guard must therefore outlive the temporary source.
            context.push(&closure_);
            auto source = materialize_source(context);
            void* result = resolve_address_impl<Target>(
                context, std::move(source), requested_type, registered_type);
            context.pop();
            return result;
        }

        [[maybe_unused]] auto guard =
            materialization_traits::template make_guard<leaf_type>(context,
                                                                   get_storage());
        auto source = materialize_source(context);
        return resolve_address_impl<Target>(
            context, std::move(source), requested_type, registered_type);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <typename Context> decltype(auto) resolve(Context& context) {
        using leaf_type = leaf_type_t<typename Storage::type>;
        [[maybe_unused]] auto guard =
            materialization_traits::template make_guard<leaf_type>(context,
                                                                   get_storage());
        auto source = materialize_source(context);
        return std::move(source).get();
    }

    template <typename T, typename Context> decltype(auto) resolve(Context& context) {
        using materialized_source_type = decltype(materialize_source(context));
        using Source = decltype(std::declval<materialized_source_type>().get());
        using leaf_type = leaf_type_t<typename Storage::type>;
        [[maybe_unused]] auto guard =
            materialization_traits::template make_guard<leaf_type>(context,
                                                                   get_storage());
        auto source = materialize_source(context);
        if constexpr (std::is_same_v<T, Source>) {
            return std::move(source).get();
        } else {
            return resolve_conversion<T>(context, std::move(source).get());
        }
    }

    void destroy() override {
        auto allocator = allocator_traits::rebind<instance_factory>(
            get_container().get_allocator());
        allocator_traits::destroy(allocator, this);
        allocator_traits::deallocate(allocator, this, 1);
    }
};

// This is much faster than unique_ptr + deleter
template <typename T> struct instance_factory_ptr {
    instance_factory_ptr(T* ptr) : ptr_(ptr) {}

    instance_factory_ptr(instance_factory_ptr<T>&& other) {
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
