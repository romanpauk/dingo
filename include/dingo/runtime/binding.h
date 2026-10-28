//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The runtime binding: the storage adapter and kernel policy of one binding,
// the state a request walks from, and the rollback wrapper.

#include <dingo/base/config.h>

#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/runtime_binding_interface.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/storage.h>
#include <dingo/type/rebind_type.h>

#include <cassert>
#include <memory>
#include <type_traits>

namespace dingo {
// TODO: this is bit convoluted, ideally merge resolver with runtime binding
namespace detail {} // namespace detail

template <typename Owner, typename InstanceContainer, typename Storage,
          typename RegistrationParent = typename Owner::parent_container_type>
class runtime_binding_state
    : private detail::cache::state<detail::cache::is_stable_storage_v<Storage>>,
      private detail::binding_slot<Storage> {
  using storage_holder = detail::binding_slot<Storage>;

public:
  using owner_type = Owner;
  using container_type = InstanceContainer;
  using registration_parent_type = RegistrationParent;
  using allocator_type = typename Owner::allocator_type;
  using runtime_type = runtime_storage<allocator_type>;
  using runtime_context_type = runtime_context<allocator_type>;
  using parent_container_type = registration_parent_type;

  template <typename... Args>
  explicit runtime_binding_state(owner_type *owner, Args &&...args)
      : storage_holder(std::forward<Args>(args)...), owner_(owner) {}

  runtime_type &runtime() { return owner().runtime(); }
  parent_container_type *parent() {
    if constexpr (std::is_same_v<registration_parent_type,
                                 typename owner_type::parent_container_type>) {
      return owner().parent();
    } else {
      return owner()
          .template runtime_registration_parent<registration_parent_type>();
    }
  }
  allocator_type &get_allocator() { return owner().get_allocator(); }
  Storage &storage() { return storage_holder::storage(); }
  detail::cache::entry *cache_slot() noexcept {
    return detail::cache::state<
        detail::cache::is_stable_storage_v<Storage>>::slot();
  }
  InstanceContainer &container() {
    if (!instance_container_) {
      instance_container_ = construct_container<InstanceContainer>(nullptr);
    }
    return *instance_container_;
  }

  template <bool TrackRollback = true>
  InstanceContainer &container(runtime_context_type &context) {
    if (!instance_container_) {
      assert(instance_container_ == nullptr);
      auto *created = construct_container<InstanceContainer>(&context);
      if constexpr (TrackRollback) {
        context.on_rollback(
            +[](void *self, void *) noexcept {
              static_cast<runtime_binding_state *>(self)->instance_container_ =
                  nullptr;
            },
            this);
      }
      instance_container_ = created;
    }
    return *instance_container_;
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
  owner_type &owner() {
    assert(owner_ != nullptr);
    return *owner_;
  }

  template <typename T> T *construct_container(runtime_context_type *context) {
    if (context != nullptr) {
      return std::addressof(context->template construct<T>(
          persistent_scope, parent(), get_allocator()));
    }
    return std::addressof(
        runtime().template construct<T>(parent(), get_allocator()));
  }

  owner_type *owner_ = nullptr;
  InstanceContainer *instance_container_ = nullptr;
};

namespace detail {

template <typename Owner, typename InstanceContainer, typename Storage,
          typename RegistrationParent = typename Owner::parent_container_type>
using runtime_binding_state_t =
    runtime_binding_state<Owner, InstanceContainer, Storage,
                          RegistrationParent>;

} // namespace detail

namespace detail {
template <typename T> T &state_ref(T &state) { return state; }

template <typename T> T &state_ref(std::shared_ptr<T> &state) { return *state; }

template <typename T> T &state_ref(T *state) { return *state; }
} // namespace detail

namespace detail {
template <typename Type, typename Storage>
using runtime_binding_cache_types_t =
    std::conditional_t<Storage::conversions::is_stable,
                       binding_conversion_cache_types_t<Type, Storage>,
                       type_list<>>;

template <typename Storage, typename = void>
struct runtime_storage_can_reset : std::false_type {};

template <typename Storage>
struct runtime_storage_can_reset<
    Storage,
    std::void_t<decltype(std::declval<Storage &>().reset()),
                decltype(std::declval<const Storage &>().is_resolved())>>
    : std::true_type {};

template <typename... Resolutions>
std::size_t resolution_request_index(type_list<Resolutions...>,
                                     type_descriptor requested_type) {
  std::size_t index = 0;
  std::size_t result = sizeof...(Resolutions);
  (void)(((matches_resolution_request<Resolutions>(requested_type)
               ? (result = index, true)
               : (++index, false)) ||
          ...));
  return result;
}

} // namespace detail

// TODO: the container here is just for RTTI, but it is needed to get the
// inner container type and that is very hard. Perhaps pass RTTI and inner
// container directly?
template <typename Container, typename Type, typename Storage, typename State>
class runtime_binding
    : public runtime_binding_interface<
          typename Container::rtti_type,
          runtime_context<typename Container::allocator_type>>,
      private detail::conversion_cache_slot<
          detail::runtime_binding_cache_types_t<Type, Storage>> {
public:
  using storage_type = Storage;
  using runtime_context_type =
      runtime_context<typename Container::allocator_type>;
  using state_type = std::remove_reference_t<decltype(detail::state_ref(
      std::declval<State &>()))>;
  using container_type = typename state_type::container_type;
  using interface_type = Type;
  using request_type = instance_request<typename Container::rtti_type>;
  using interface_base =
      runtime_binding_interface<typename Container::rtti_type,
                                runtime_context_type>;
  using request_resolutions =
      typename detail::binding_resolutions<Type, Storage>::type;
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using conversion_cache_base = detail::conversion_cache_slot<
      detail::runtime_binding_cache_types_t<Type, Storage>>;

private:
  template <typename T, typename Context, typename... Args>
  T &construct_conversion(construction_scope scope, Context &context,
                          Args &&...args) {
    return conversion_cache_base::template construct_conversion_with<T>(
        state().runtime(), scope, context, std::forward<Args>(args)...);
  }

private:
  State state_;

  state_type &state() { return detail::state_ref(state_); }
  auto &get_storage() { return state().storage(); }
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename Context, typename Fn>
  decltype(auto) with_source(construction_scope scope, Context &context,
                             Fn &&fn) {
    if constexpr (materialization_traits::can_retain_source) {
      if (materialization_traits::retains_source(get_storage())) {
        // The reset flag is selected by the callback, not carried by it.
        context.on_rollback(
            should_reset_storage_on_failure()
                ? +[](void *self, void *) noexcept {
                    auto *binding = static_cast<runtime_binding *>(self);
                    binding->reset_conversion_artifacts();
                    binding->reset_storage_after_failure(true);
                  }
                : +[](void *self, void *) noexcept {
                    auto *binding = static_cast<runtime_binding *>(self);
                    binding->reset_conversion_artifacts();
                    binding->reset_storage_after_failure(false);
                  },
            this);
        auto materialize = [&]() -> decltype(auto) {
          return state().template with_resolution_container<true>(
              context, [&](auto &container) -> decltype(auto) {
                return detail::with_materialized_source<
                    detail::source_lifetime::context>(persistent_scope, context,
                                                      get_storage(), container,
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
    return state().with_resolution_container(
        context, [&](auto &container) -> decltype(auto) {
          return detail::with_materialized_source<
              detail::source_lifetime::context>(
              scope, context, get_storage(), container, std::forward<Fn>(fn));
        });
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  struct runtime_resolution_resolver {
    runtime_binding &binding;
    construction_scope scope;

    template <typename T, typename Context, typename Source>
    decltype(auto) retain_conversion(Context &context, Source &&source) {
      return binding.template retain_conversion<T>(
          scope, context, std::forward<Source>(source));
    }

    template <typename T, typename Context, typename Source>
    decltype(auto) construct_conversion(construction_scope construction,
                                        Context &context, Source &&source) {
      return binding.template construct_conversion<T>(
          construction, context, std::forward<Source>(source));
    }
  };

  template <typename... Resolutions, typename Context>
  resolved_address
  resolve_candidates(construction_scope scope, type_list<Resolutions...>,
                     std::size_t index, type_descriptor requested_type,
                     Context &context) {
    runtime_resolution_resolver resolver{*this, scope};
    auto materialize = [&](auto &&source) -> resolved_address {
      return detail::resolve_candidate<Storage>(
          type_list<Resolutions...>{}, index, scope, resolver, context,
          std::forward<decltype(source)>(source), requested_type,
          detail::binding_registered_type<Storage>());
    };
    return with_source(scope, context, std::move(materialize));
  }

  void reset_conversion_artifacts() {
    conversion_cache_base::reset_conversions();
  }

  void reset_retained_source_runtime_artifacts() noexcept {
    reset_conversion_artifacts();
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
    return detail::binding_registered_type<Storage>();
  }

public:
  template <typename T, typename Context, typename Source>
  decltype(auto) retain_conversion(construction_scope scope, Context &context,
                                   Source &&source) {
    runtime_resolution_resolver resolver{*this, scope};
    return detail::retain_binding_conversion<T>(
        scope, get_storage(), resolver, context, std::forward<Source>(source));
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  static resolved_address dispatch_request(interface_base &erased,
                                           construction_scope scope,
                                           runtime_context_type &context,
                                           const request_type &request,
                                           const void *cache_key) {
    auto &binding = static_cast<runtime_binding &>(erased);
    if constexpr (type_list_size_v<request_resolutions> == 0) {
      (void)scope;
      (void)cache_key;
      throw ::dingo::request_route_not_found_exception(
          detail::make_type_not_convertible_exception(
              request.requested_type, registered_type(), context)
              .what());
    } else {
      const auto resolution_index = detail::resolution_request_index(
          request_resolutions{}, request.requested_type);
      if (resolution_index == type_list_size_v<request_resolutions>) {
        throw ::dingo::request_route_not_found_exception(
            detail::make_type_not_convertible_exception(
                request.requested_type, registered_type(), context)
                .what());
      }

      auto result = binding.resolve_candidates(scope, request_resolutions{},
                                               resolution_index,
                                               request.requested_type, context);
      // Request caching is intentionally stricter than conversion caching and
      // is published with transaction rollback tracking.
      if constexpr (Storage::conversions::is_stable) {
        if (cache_key != nullptr &&
            result.access == resolved_address::access_kind::borrow) {
          binding.publish_cache(context, cache_key, result.address);
        }
      }
      return result;
    }
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

public:
  template <typename... Args>
  runtime_binding(Args &&...args)
      : interface_base(&runtime_binding::dispatch_request),
        state_(std::forward<Args>(args)...) {
    this->cache_slot(state().cache_slot());
  }

  auto &get_container() { return state().container(); }
};

} // namespace dingo
