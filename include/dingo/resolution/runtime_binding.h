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

#include <memory>
#include <type_traits>

namespace dingo {
// TODO: this is bit convoluted, ideally merge resolver with runtime binding
namespace detail {
template <typename Host, typename StaticRegistry> class binding_resolution;
} // namespace detail

template <typename InstanceContainer, typename Storage,
          typename ResolutionContainer = InstanceContainer>
class runtime_binding_state {
public:
  using container_type = InstanceContainer;
  using instance_container_type = InstanceContainer;
  using resolution_container_type = ResolutionContainer;
  using allocator_type = typename InstanceContainer::allocator_type;
  using parent_container_type =
      typename InstanceContainer::parent_container_type;

  template <typename ParentContainer, typename... Args>
  runtime_binding_state(ParentContainer *parent, Args &&...args)
      : storage_(std::forward<Args>(args)...), parent_(parent) {}

  detail::context_closure &closure() { return closure_; }
  Storage &storage_ref() { return storage_; }
  InstanceContainer &instance_container() { return container(); }
  ResolutionContainer &resolution_container() {
    if (!resolution_container_) {
      resolution_container_ = construct_container<ResolutionContainer>();
    }
    return *resolution_container_;
  }

  InstanceContainer &container() {
    if (!instance_container_) {
      instance_container_ = construct_container<InstanceContainer>();
    }
    return *instance_container_;
  }

private:
  template <typename T> T *construct_container() {
    if constexpr (std::is_constructible_v<T, parent_container_type *,
                                          decltype(parent_->get_allocator()),
                                          detail::context_closure_base &>) {
      return std::addressof(closure_.template construct<T>(
          parent_, parent_->get_allocator(), closure_));
    } else {
      return std::addressof(
          closure_.template construct<T>(parent_, parent_->get_allocator()));
    }
  }

  // Storage is declared after the closure so cached instances are destroyed
  // before preserved construction temporaries.
  detail::context_closure closure_;
  Storage storage_;
  parent_container_type *parent_;
  InstanceContainer *instance_container_ = nullptr;
  ResolutionContainer *resolution_container_ = nullptr;
};

template <typename InstanceContainer, typename Storage>
class runtime_binding_state<InstanceContainer, Storage, InstanceContainer> {
public:
  using container_type = InstanceContainer;
  using instance_container_type = InstanceContainer;
  using resolution_container_type = InstanceContainer;
  using allocator_type = typename InstanceContainer::allocator_type;
  using parent_container_type =
      typename InstanceContainer::parent_container_type;

  template <typename ParentContainer, typename... Args>
  runtime_binding_state(ParentContainer *parent, Args &&...args)
      : storage_(std::forward<Args>(args)...), parent_(parent) {}

  detail::context_closure &closure() { return closure_; }
  Storage &storage_ref() { return storage_; }
  InstanceContainer &instance_container() { return container(); }
  InstanceContainer &resolution_container() { return container(); }

  InstanceContainer &container() {
    if (!instance_container_) {
      instance_container_ = construct_container<InstanceContainer>();
    }
    return *instance_container_;
  }

private:
  template <typename T> T *construct_container() {
    if constexpr (std::is_constructible_v<T, parent_container_type *,
                                          decltype(parent_->get_allocator()),
                                          detail::context_closure_base &>) {
      return std::addressof(closure_.template construct<T>(
          parent_, parent_->get_allocator(), closure_));
    } else {
      return std::addressof(
          closure_.template construct<T>(parent_, parent_->get_allocator()));
    }
  }

  // Storage is declared after the closure so cached instances are destroyed
  // before preserved construction temporaries.
  detail::context_closure closure_;
  Storage storage_;
  parent_container_type *parent_;
  InstanceContainer *instance_container_ = nullptr;
};

namespace detail {

template <typename ResolveRoot, typename InstanceContainer, typename Bindings>
using runtime_binding_resolution_container_t =
    std::conditional_t<std::is_void_v<Bindings>, InstanceContainer,
                       binding_resolution<ResolveRoot, Bindings>>;

template <typename ResolveRoot, typename InstanceContainer, typename Storage,
          typename Bindings>
using runtime_binding_state_t =
    runtime_binding_state<InstanceContainer, Storage,
                          runtime_binding_resolution_container_t<
                              ResolveRoot, InstanceContainer, Bindings>>;

} // namespace detail

template <typename T> struct runtime_binding_state_traits {
  using state_type = T;

  static T &ref(T &state) { return state; }
};

template <typename T> struct runtime_binding_state_traits<std::shared_ptr<T>> {
  using state_type = T;

  static T &ref(std::shared_ptr<T> &state) { return *state; }
};

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
template <typename Container, typename Type, typename Storage, typename State>
class runtime_binding
    : public runtime_binding_interface<Container>,
      private detail::binding_conversion_cache_base<
          Storage::conversions::is_stable &&
              detail::runtime_binding_has_conversion_cache_v<
                  detail::runtime_binding_conversion_types_t<Type, Storage>>,
          detail::runtime_binding_conversion_types_t<Type, Storage>> {
public:
  using storage_type = Storage;
  using state_traits = runtime_binding_state_traits<State>;
  using state_type = typename state_traits::state_type;
  using container_type = typename state_type::container_type;
  using interface_type = Type;
  using rtti_type = typename Container::rtti_type;
  using type_index = typename rtti_type::type_index;
  using request_type = instance_request<rtti_type>;
  using conversion_types =
      detail::runtime_binding_conversion_types_t<Type, Storage>;
  using exact_value_types =
      std::conditional_t<type_traits<Type>::enabled && !std::is_pointer_v<Type>,
                         type_list<Type>, type_list<>>;
  using exact_lvalue_reference_types =
      std::conditional_t<Storage::conversions::is_stable &&
                             type_traits<Type>::enabled &&
                             !std::is_pointer_v<Type>,
                         type_list<Type &>, type_list<>>;
  using exact_pointer_types =
      std::conditional_t<Storage::conversions::is_stable &&
                             type_traits<Type>::enabled &&
                             !std::is_pointer_v<Type>,
                         type_list<Type *>, type_list<>>;
  using value_capability_types =
      type_list_cat_t<exact_value_types,
                      typename Storage::conversions::value_types>;
  using lvalue_reference_capability_types =
      type_list_cat_t<exact_lvalue_reference_types,
                      typename Storage::conversions::lvalue_reference_types>;
  using pointer_capability_types =
      type_list_cat_t<exact_pointer_types,
                      typename Storage::conversions::pointer_types>;
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
    runtime_binding &binding;

    template <typename T, typename Context, typename Source>
    decltype(auto) construct_conversion(Context &context, Source &&source) {
      return binding.template construct_conversion<T>(
          context, std::forward<Source>(source));
    }

    template <typename T, typename Context, typename Source>
    decltype(auto) resolve_conversion(Context &context, Source &&source) {
      return binding.template resolve_conversion<T>(
          context, std::forward<Source>(source));
    }
  };

  State state_;

  state_type &state_ref() { return state_traits::ref(state_); }
  auto &closure() { return state_ref().closure(); }
  auto &get_storage() { return state_ref().storage_ref(); }
  auto &get_resolution_container() {
    return state_ref().resolution_container();
  }

  template <typename Context, typename Fn>
  decltype(auto) materialize_resolution_source(Context &context, Fn &&fn) {
    return detail::materialize_binding_resolution_source(
        context, get_storage(), get_resolution_container(), closure(),
        std::forward<Fn>(fn));
  }

  static constexpr type_descriptor registered_type() {
    return describe_type<detail::registered_type_t<Storage>>();
  }

public:
  template <typename T, typename Context, typename Source>
  decltype(auto) resolve_conversion(Context &context, Source &&source) {
    binding_activation activation{*this};
    return detail::resolve_binding_conversion<T>(
        get_storage(), activation, context, std::forward<Source>(source));
  }

  template <typename ConversionTypes>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  void *convert(runtime_context &context, const request_type &request,
                instance_cache_sink cache) {
    void *ptr = ::dingo::resolve_binding_capability_address<rtti_type>(
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
  runtime_binding(Args &&...args) : state_(std::forward<Args>(args)...) {}

  auto &get_container() { return state_ref().container(); }

  void *get_value(runtime_context &context, const request_type &request,
                  instance_cache_sink cache) override {
    return convert<value_capability_types>(context, request, cache);
  }

  void *get_lvalue_reference(runtime_context &context,
                             const request_type &request,
                             instance_cache_sink cache) override {
    return convert<lvalue_reference_capability_types>(context, request, cache);
  }

  void *get_rvalue_reference(runtime_context &context,
                             const request_type &request,
                             instance_cache_sink cache) override {
    return convert<typename Storage::conversions::rvalue_reference_types>(
        context, request, cache);
  }

  void *get_pointer(runtime_context &context, const request_type &request,
                    instance_cache_sink cache) override {
    return convert<pointer_capability_types>(context, request, cache);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename T, typename Context>
  void *resolve_address(Context &context, type_descriptor requested_type,
                        type_descriptor registered_type) {
    if constexpr (is_exact_lookup_v<T>) {
      if (!detail::matches_exact_lookup<resolved_type_t<T, Type>>(
              requested_type)) {
        throw detail::make_type_not_convertible_exception(
            requested_type, registered_type, context);
      }
    }

    using Target = std::remove_reference_t<resolved_type_t<T, Type>>;
    return materialize_resolution_source(context, [&](auto &&source) -> void * {
      return detail::resolve_binding_address_from_source<Target>(
          *this, context, std::forward<decltype(source)>(source),
          requested_type, registered_type);
    });
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  template <typename Context> decltype(auto) resolve(Context &context) {
    return materialize_resolution_source(
        context, [](auto &&source) -> decltype(auto) {
          return std::forward<decltype(source)>(source).get();
        });
  }

  template <typename T, typename Context>
  decltype(auto) resolve(Context &context) {
    binding_activation activation{*this};
    return materialize_resolution_source(
        context, [&](auto &&source) -> decltype(auto) {
          return detail::resolve_binding_value<T>(
              activation, context, std::forward<decltype(source)>(source));
        });
  }
};

} // namespace dingo
