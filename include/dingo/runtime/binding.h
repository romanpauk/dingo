//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/binding/binding_activation.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/runtime_binding_interface.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/memory.h>
#include <dingo/type/rebind_type.h>

#include <cassert>
#include <memory>
#include <type_traits>

namespace dingo {
// The state of a runtime registration: its binding cell, the request cache of
// its interfaces and the child container its factory resolves through once
// the registration creates one.
template <typename Owner, typename InstanceContainer, typename Cell,
          typename RegistrationParent = typename Owner::parent_container_type>
class runtime_binding_state
    : private detail::cache::state<
          detail::cache::is_stable_storage_v<typename Cell::storage_type>> {
public:
  using cell_type = Cell;
  using container_type = InstanceContainer;

  template <typename... Args>
  explicit runtime_binding_state(Owner *owner, Args &&...args)
      : owner_(owner), cell_(std::forward<Args>(args)...) {}

  cell_type &cell() { return cell_; }
  typename Cell::storage_type &storage() { return cell_.storage(); }
  detail::cache::entry *cache_slot() noexcept { return this->slot(); }

  InstanceContainer &container() {
    if (!instance_container_) {
      instance_container_ = std::addressof(
          owner_->runtime().template construct<InstanceContainer>(
              parent(), owner_->get_allocator()));
    }
    return *instance_container_;
  }

  template <typename Fn> decltype(auto) with_resolution_container(Fn &&fn) {
    if (instance_container_ != nullptr) {
      return fn(*instance_container_);
    }
    return fn(*parent());
  }

private:
  RegistrationParent *parent() {
    if constexpr (std::is_same_v<RegistrationParent,
                                 typename Owner::parent_container_type>) {
      return owner_->parent();
    } else {
      return owner_->template runtime_registration_parent<RegistrationParent>();
    }
  }

  Owner *owner_ = nullptr;
  cell_type cell_;
  InstanceContainer *instance_container_ = nullptr;
};

template <typename T> struct runtime_binding_state_traits {
  using state_type = T;

  static T &ref(T &state) { return state; }
};

template <typename T> struct runtime_binding_state_traits<T *> {
  using state_type = T;

  static T &ref(T *state) { return *state; }
};

namespace detail {
template <typename Storage> using registered_type_t = typename Storage::type;

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

// Applies the resolution at a run-time index to a source.
template <typename Activation, typename Context, typename MaterializedSource,
          typename... Resolutions>
resolved_address
resolve_indexed_address(Activation &activation, Context &context,
                        MaterializedSource &&source,
                        std::size_t resolution_index, type_list<Resolutions...>,
                        type_descriptor requested_type) {
  resolved_address result{};
  std::size_t index = 0;
  // Short-circuiting guarantees that exactly one arm forwards the source.
  const bool matched =
      (((resolution_index == index++)
            ? (result = activation.template resolve_address<Resolutions>(
                   context,
                   std::forward<MaterializedSource>(
                       source), // NOLINT(bugprone-use-after-move)
                   requested_type),
               true)
            : false) ||
       ...);
  (void)index;
  (void)matched;
  assert(matched);
  return result;
}

} // namespace detail

// The erased binding of one interface. It selects the resolution of a request
// at run time and applies it through a binding activation of its cell.
template <typename Container, typename Type, typename Storage, typename State>
class runtime_binding
    : public runtime_binding_interface<
          typename Container::rtti_type,
          runtime_context<typename Container::allocator_type>> {
public:
  using storage_type = Storage;
  using runtime_context_type =
      runtime_context<typename Container::allocator_type>;
  using state_traits = runtime_binding_state_traits<State>;
  using state_type = typename state_traits::state_type;
  using container_type = typename state_type::container_type;
  using interface_type = Type;
  using rtti_type = typename Container::rtti_type;
  using request_type = instance_request<rtti_type>;
  using interface_base =
      runtime_binding_interface<rtti_type, runtime_context_type>;
  using request_resolutions =
      typename detail::binding_resolutions<Type, Storage>::type;

private:
  State state_;

  state_type &state() { return state_traits::ref(state_); }

  static constexpr type_descriptor registered_type() {
    return describe_type<detail::registered_type_t<Storage>>();
  }

  // The source of an unstable storage lives in the context, which ends before
  // the caller of the container uses an address into it.
  template <typename Source>
  static void check_caller_address(construction_scope scope,
                                   const resolved_address &resolved,
                                   const void *source,
                                   const request_type &request,
                                   runtime_context_type &context) {
    if constexpr (Storage::conversions::is_stable) {
      (void)scope, (void)resolved, (void)source, (void)request, (void)context;
    } else if (scope.is_caller() &&
               detail::refers_into<Source>(resolved.address, source)) {
      throw detail::make_type_not_convertible_exception(
          request.requested_type, registered_type(), context);
    }
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
      throw detail::make_type_not_convertible_exception(
          request.requested_type, registered_type(), context);
    } else {
      const auto resolution_index = detail::resolution_request_index(
          request_resolutions{}, request.requested_type);
      if (resolution_index == type_list_size_v<request_resolutions>) {
        throw detail::make_type_not_convertible_exception(
            request.requested_type, registered_type(), context);
      }

      auto &state = binding.state();
      auto result = state.with_resolution_container(
          [&](auto &container) -> resolved_address {
            using activation_type = detail::binding_activation<
                typename state_type::cell_type,
                std::remove_reference_t<decltype(container)> &>;
            activation_type activation{state.cell(), container, scope};
            return activation.template with_source<void>(
                context, [&](auto &&source) -> resolved_address {
                  const void *source_address = std::addressof(source);
                  auto resolved = detail::resolve_indexed_address(
                      activation, context,
                      std::forward<decltype(source)>(source), resolution_index,
                      request_resolutions{}, request.requested_type);
                  check_caller_address<decltype(source)>(
                      scope, resolved, source_address, request, context);
                  return resolved;
                });
          });
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
