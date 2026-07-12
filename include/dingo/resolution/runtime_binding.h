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
#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/context.h>
#include <dingo/type/rebind_type.h>

#include <cassert>
#include <memory>
#include <type_traits>

namespace dingo {
// TODO: this is bit convoluted, ideally merge resolver with runtime binding
namespace detail {
template <typename Host, typename StaticRegistry> class binding_resolution;

template <typename Storage, typename = void>
struct runtime_storage_can_retain_source : std::false_type {};

template <typename Storage>
struct runtime_storage_can_retain_source<
    Storage, std::void_t<typename Storage::tag_type, typename Storage::type>>
    : std::bool_constant<storage_materialization_traits<
          typename Storage::tag_type,
          typename Storage::type>::can_retain_source> {};

template <typename Storage>
inline constexpr bool runtime_storage_can_retain_source_v =
    runtime_storage_can_retain_source<Storage>::value;

} // namespace detail

template <typename Owner, typename InstanceContainer, typename Storage,
          typename ResolutionContainer = InstanceContainer>
class runtime_binding_state : private detail::cache::state<
                                  detail::cache::is_stable_storage_v<Storage>> {
public:
  using owner_type = Owner;
  using container_type = InstanceContainer;
  using instance_container_type = InstanceContainer;
  using resolution_container_type = ResolutionContainer;
  using allocator_type = typename Owner::allocator_type;
  using runtime_type = container_runtime<allocator_type>;
  using runtime_context_type = runtime_context<allocator_type>;
  using parent_container_type = typename Owner::parent_container_type;

  template <typename... Args>
  explicit runtime_binding_state(owner_type *owner, Args &&...args)
      : owner_(owner), storage_(std::forward<Args>(args)...) {}

  runtime_type &runtime() { return owner().runtime(); }
  parent_container_type *parent() { return owner().parent(); }
  allocator_type &get_allocator() { return owner().get_allocator(); }
  Storage &storage() { return storage_; }
  detail::cache::entry *cache_slot() noexcept { return cache_state::slot(); }
  void reset_containers() {
    instance_container_ = nullptr;
    resolution_container_ = nullptr;
  }
  InstanceContainer &container() {
    if (!instance_container_) {
      instance_container_ = construct_container<InstanceContainer>();
    }
    return *instance_container_;
  }

  template <bool TrackRollback = true>
  ResolutionContainer &resolution_container(runtime_context_type &context) {
    if (!resolution_container_) {
      assert(resolution_container_ == nullptr);
      auto *created = construct_container<ResolutionContainer>(context);
      if constexpr (TrackRollback) {
        context.on_rollback(
            [this]() noexcept { resolution_container_ = nullptr; });
      }
      resolution_container_ = created;
    }
    return *resolution_container_;
  }

  template <bool TrackRollback = true>
  InstanceContainer &container(runtime_context_type &context) {
    if (!instance_container_) {
      assert(instance_container_ == nullptr);
      auto *created = construct_container<InstanceContainer>(context);
      if constexpr (TrackRollback) {
        context.on_rollback(
            [this]() noexcept { instance_container_ = nullptr; });
      }
      instance_container_ = created;
    }
    return *instance_container_;
  }

  template <bool TrackRollback = true, typename Fn>
  decltype(auto) with_resolution_container(runtime_context_type &context,
                                           Fn &&fn) {
    return std::forward<Fn>(fn)(resolution_container<TrackRollback>(context));
  }

private:
  using cache_state =
      detail::cache::state<detail::cache::is_stable_storage_v<Storage>>;

  owner_type &owner() {
    assert(owner_ != nullptr);
    return *owner_;
  }

  template <typename T> T *construct_container(runtime_context_type &context) {
    return std::addressof(context.template construct<T>(
        persistent_scope, parent(), get_allocator()));
  }

  template <typename T> T *construct_container() {
    return std::addressof(
        runtime().template construct<T>(parent(), get_allocator()));
  }

  owner_type *owner_ = nullptr;
  Storage storage_;
  InstanceContainer *instance_container_ = nullptr;
  ResolutionContainer *resolution_container_ = nullptr;
};

template <typename Owner, typename InstanceContainer, typename Storage>
class runtime_binding_state<Owner, InstanceContainer, Storage,
                            InstanceContainer>
    : private detail::cache::state<
          detail::cache::is_stable_storage_v<Storage>> {
public:
  using owner_type = Owner;
  using container_type = InstanceContainer;
  using instance_container_type = InstanceContainer;
  using resolution_container_type = InstanceContainer;
  using allocator_type = typename Owner::allocator_type;
  using runtime_type = container_runtime<allocator_type>;
  using runtime_context_type = runtime_context<allocator_type>;
  using parent_container_type = typename Owner::parent_container_type;

  template <typename... Args>
  explicit runtime_binding_state(owner_type *owner, Args &&...args)
      : owner_(owner), storage_(std::forward<Args>(args)...) {}

  runtime_type &runtime() { return owner().runtime(); }
  parent_container_type *parent() { return owner().parent(); }
  allocator_type &get_allocator() { return owner().get_allocator(); }
  Storage &storage() { return storage_; }
  detail::cache::entry *cache_slot() noexcept { return cache_state::slot(); }
  void reset_containers() { instance_container_ = nullptr; }
  InstanceContainer &container() {
    if (!instance_container_) {
      instance_container_ = construct_container<InstanceContainer>();
    }
    return *instance_container_;
  }

  template <bool TrackRollback = true>
  InstanceContainer &container(runtime_context_type &context) {
    if (!instance_container_) {
      assert(instance_container_ == nullptr);
      auto *created = construct_container<InstanceContainer>(context);
      if constexpr (TrackRollback) {
        context.on_rollback(
            [this]() noexcept { instance_container_ = nullptr; });
      }
      instance_container_ = created;
    }
    return *instance_container_;
  }

  template <bool TrackRollback = true>
  InstanceContainer &resolution_container(runtime_context_type &context) {
    return container<TrackRollback>(context);
  }

  template <bool TrackRollback = true, typename Fn>
  decltype(auto) with_resolution_container(runtime_context_type &context,
                                           Fn &&fn) {
    (void)context;
    if (instance_container_ != nullptr) {
      return fn(*instance_container_);
    }
    return fn(*parent());
  }

private:
  using cache_state =
      detail::cache::state<detail::cache::is_stable_storage_v<Storage>>;

  owner_type &owner() {
    assert(owner_ != nullptr);
    return *owner_;
  }

  template <typename T> T *construct_container(runtime_context_type &context) {
    return std::addressof(context.template construct<T>(
        persistent_scope, parent(), get_allocator()));
  }

  template <typename T> T *construct_container() {
    return std::addressof(
        runtime().template construct<T>(parent(), get_allocator()));
  }

  owner_type *owner_ = nullptr;
  Storage storage_;
  InstanceContainer *instance_container_ = nullptr;
};

namespace detail {

template <typename ResolveRoot, typename InstanceContainer, typename Bindings>
using runtime_binding_resolution_container_t =
    std::conditional_t<std::is_void_v<Bindings>, InstanceContainer,
                       binding_resolution<ResolveRoot, Bindings>>;

template <typename Owner, typename InstanceContainer, typename Storage,
          typename Bindings>
using runtime_binding_state_t = runtime_binding_state<
    Owner, InstanceContainer, Storage,
    runtime_binding_resolution_container_t<
        typename Owner::parent_container_type, InstanceContainer, Bindings>>;

} // namespace detail

template <typename T> struct runtime_binding_state_traits {
  using state_type = T;

  static T &ref(T &state) { return state; }
};

template <typename T> struct runtime_binding_state_traits<std::shared_ptr<T>> {
  using state_type = T;

  static T &ref(std::shared_ptr<T> &state) { return *state; }
};

template <typename T> struct runtime_binding_state_traits<T *> {
  using state_type = T;

  static T &ref(T *state) { return *state; }
};

namespace detail {
template <typename Storage> using registered_type_t = typename Storage::type;

template <typename Type, typename Storage>
using runtime_binding_conversion_types_t =
    rebind_leaf_t<typename Storage::conversions::conversion_types, Type>;

template <typename Types>
static constexpr bool runtime_binding_has_conversion_cache_v =
    type_list_size_v<Types> != 0;

template <typename Storage, typename = void>
struct runtime_storage_can_reset : std::false_type {};

template <typename Storage>
struct runtime_storage_can_reset<
    Storage,
    std::void_t<decltype(std::declval<Storage &>().reset()),
                decltype(std::declval<const Storage &>().is_resolved())>>
    : std::true_type {};

} // namespace detail

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename Type, typename Storage, typename State>
class runtime_binding
    : public runtime_binding_interface<
          Container, runtime_context<typename Container::allocator_type>>,
      private detail::binding_conversion_cache_base<
          Storage::conversions::is_stable &&
              detail::runtime_binding_has_conversion_cache_v<
                  detail::runtime_binding_conversion_types_t<Type, Storage>>,
          detail::runtime_binding_conversion_types_t<Type, Storage>> {
public:
  using storage_type = Storage;
  using runtime_context_type =
      runtime_context<typename Container::allocator_type>;
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
  template <typename T, typename Context, typename... Args>
  T &construct_conversion(construction_scope scope, Context &context,
                          Args &&...args) {
    if constexpr (uses_cached_conversions) {
      return conversion_cache_base::template construct_conversion_with<T>(
          state().runtime(), scope, context, std::forward<Args>(args)...);
    } else {
      return conversion_cache_base::template construct_conversion<T>(
          scope, context, std::forward<Args>(args)...);
    }
  }

  struct binding_activation {
    runtime_binding &binding;
    construction_scope scope;

    template <typename T, typename Context, typename Source>
    decltype(auto) construct_conversion(construction_scope construction,
                                        Context &context, Source &&source) {
      return binding.template construct_conversion<T>(
          construction, context, std::forward<Source>(source));
    }

    template <typename T, typename Context, typename Source>
    decltype(auto) resolve_conversion(Context &context, Source &&source) {
      return binding.template resolve_conversion<T>(
          scope, context, std::forward<Source>(source));
    }
  };

  State state_;

  state_type &state() { return state_traits::ref(state_); }
  auto &get_storage() { return state().storage(); }
  template <bool TrackRollback = true, typename Fn>
  decltype(auto) with_resolution_container(runtime_context_type &context,
                                           Fn &&fn) {
    return state().template with_resolution_container<TrackRollback>(
        context, std::forward<Fn>(fn));
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename Context, typename Fn>
  decltype(auto) materialize_resolution_source(construction_scope scope,
                                               Context &context, Fn &&fn) {
    if constexpr (materialization_traits::can_retain_source) {
      if (materialization_traits::retains_source(get_storage())) {
        const bool reset_storage = should_reset_storage_on_failure();
        context.on_rollback([this, reset_storage]() noexcept {
          reset_runtime_artifacts();
          reset_storage_after_failure(reset_storage);
        });
        auto materialize = [&]() -> decltype(auto) {
          return with_resolution_container<false>(
              context, [&](auto &container) -> decltype(auto) {
                return detail::materialize_runtime_binding_resolution_source(
                    persistent_scope, context, get_storage(), container,
                    std::forward<Fn>(fn));
              });
        };
        using result_type = std::invoke_result_t<decltype(materialize) &>;
        if constexpr (std::is_void_v<result_type>) {
          materialize();
          add_retained_source_runtime_reset_destructor(context);
        } else if constexpr (std::is_reference_v<result_type>) {
          result_type result = materialize();
          add_retained_source_runtime_reset_destructor(context);
          return std::forward<result_type>(result);
        } else {
          auto result = materialize();
          add_retained_source_runtime_reset_destructor(context);
          return result;
        }
      }
    }
    return with_resolution_container(
        context, [&](auto &container) -> decltype(auto) {
          return detail::materialize_runtime_binding_resolution_source(
              scope, context, get_storage(), container, std::forward<Fn>(fn));
        });
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  void reset_runtime_artifacts() {
    conversion_cache_base::reset_conversions();
    state().reset_containers();
  }

  void reset_retained_source_runtime_artifacts() noexcept {
    reset_runtime_artifacts();
    reset_storage_after_failure(true);
  }

  void
  add_retained_source_runtime_reset_destructor(runtime_context_type &context) {
    context.add_runtime_destructor(
        this,
        &runtime_binding::reset_retained_source_runtime_artifacts_destructor);
  }

  static void
  reset_retained_source_runtime_artifacts_destructor(void *ptr) noexcept {
    reinterpret_cast<runtime_binding *>(ptr)
        ->reset_retained_source_runtime_artifacts();
  }

  bool should_reset_storage_on_failure() {
    if constexpr (detail::runtime_storage_can_reset<Storage>::value) {
      return !get_storage().is_resolved();
    } else {
      return false;
    }
  }

  void reset_storage_after_failure(bool reset_storage) {
    if constexpr (detail::runtime_storage_can_reset<Storage>::value) {
      if (reset_storage) {
        get_storage().reset();
      }
    } else {
      (void)reset_storage;
    }
  }

  static constexpr type_descriptor registered_type() {
    return describe_type<detail::registered_type_t<Storage>>();
  }

public:
  template <typename T, typename Context, typename Source>
  decltype(auto) resolve_conversion(construction_scope scope, Context &context,
                                    Source &&source) {
    binding_activation activation{*this, scope};
    return detail::resolve_binding_conversion<T>(scope, get_storage(),
                                                 activation, context,
                                                 std::forward<Source>(source));
  }

  template <typename ConversionTypes>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  void *convert(construction_scope scope, runtime_context_type &context,
                const request_type &request, detail::cache::sink cache) {
    void *ptr = ::dingo::resolve_binding_capability_address<rtti_type>(
        scope, *this, context, ConversionTypes{}, request.lookup_type,
        request.requested_type, registered_type());
    // Request caching is intentionally stricter than conversion caching and
    // is published with transaction rollback tracking by the registry.
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
  runtime_binding(Args &&...args) : state_(std::forward<Args>(args)...) {
    this->cache_slot(state().cache_slot());
  }

  auto &get_container() { return state().container(); }

  void *get_value(construction_scope scope, runtime_context_type &context,
                  const request_type &request,
                  detail::cache::sink cache) override {
    return convert<value_capability_types>(scope, context, request, cache);
  }

  void *get_lvalue_reference(construction_scope scope,
                             runtime_context_type &context,
                             const request_type &request,
                             detail::cache::sink cache) override {
    return convert<lvalue_reference_capability_types>(scope, context, request,
                                                      cache);
  }

  void *get_rvalue_reference(construction_scope scope,
                             runtime_context_type &context,
                             const request_type &request,
                             detail::cache::sink cache) override {
    return convert<typename Storage::conversions::rvalue_reference_types>(
        scope, context, request, cache);
  }

  void *get_pointer(construction_scope scope, runtime_context_type &context,
                    const request_type &request,
                    detail::cache::sink cache) override {
    return convert<pointer_capability_types>(scope, context, request, cache);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename T, typename Context>
  void *resolve_address(construction_scope scope, Context &context,
                        type_descriptor requested_type,
                        type_descriptor registered_type) {
    if constexpr (is_exact_lookup_v<T>) {
      if (!detail::matches_exact_lookup<resolved_type_t<T, Type>>(
              requested_type)) {
        throw detail::make_type_not_convertible_exception(
            requested_type, registered_type, context);
      }
    }

    using Target = std::remove_reference_t<resolved_type_t<T, Type>>;
    binding_activation activation{*this, scope};
    return materialize_resolution_source(
        scope, context, [&](auto &&source) -> void * {
          return detail::resolve_binding_address_from_source<Target>(
              scope, activation, context,
              std::forward<decltype(source)>(source), requested_type,
              registered_type);
        });
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  template <typename Context>
  decltype(auto) resolve(construction_scope scope, Context &context) {
    return materialize_resolution_source(
        scope, context, [](auto &&source) -> decltype(auto) {
          return std::forward<decltype(source)>(source).get();
        });
  }

  template <typename T, typename Context>
  decltype(auto) resolve(construction_scope scope, Context &context) {
    binding_activation activation{*this, scope};
    return materialize_resolution_source(
        scope, context, [&](auto &&source) -> decltype(auto) {
          return detail::resolve_binding_value<T>(
              activation, context, std::forward<decltype(source)>(source));
        });
  }
};

} // namespace dingo
