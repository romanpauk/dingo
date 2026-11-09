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
#include <dingo/type/shape.h>

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

// Whether a resolution of a pointer resolves a reference request as the
// pointer, which an owner placed in the context adopts.
template <typename Storage, typename Resolution>
constexpr bool matches_adopted_request(type_descriptor requested_type) {
  using target = typename Resolution::target_type;
  if constexpr (std::is_pointer_v<target> &&
                adopts_made_pointer_v<Storage, std::remove_pointer_t<target>>) {
    return matches_qualification_conversion(
        detail::described_type_v<std::remove_pointer_t<target> &>,
        requested_type);
  } else {
    return false;
  }
}

// Whether a value request is resolved as the pointer of a resolution, which
// the storage hands over, and moved out of what it points to: an object that
// can be moved or copied.
template <typename Storage, typename Resolution>
constexpr bool matches_taken_request(type_descriptor requested_type) {
  using target = typename Resolution::target_type;
  if constexpr (std::is_pointer_v<target>) {
    using pointee = std::remove_cv_t<std::remove_pointer_t<target>>;
    if constexpr (std::is_constructible_v<pointee, pointee &&>) {
      return matches_adopted_request<Storage, Resolution>(requested_type);
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <typename Storage, typename... Resolutions>
std::size_t adopted_request_index(type_list<Resolutions...>,
                                  type_descriptor requested_type) {
  std::size_t index = 0;
  std::size_t result = sizeof...(Resolutions);
  (void)(((matches_adopted_request<Storage, Resolutions>(requested_type)
               ? (result = index, true)
               : (++index, false)) ||
          ...));
  return result;
}

template <typename Storage, typename... Resolutions>
std::size_t taken_request_index(type_list<Resolutions...>,
                                type_descriptor requested_type) {
  std::size_t index = 0;
  std::size_t result = sizeof...(Resolutions);
  (void)(((matches_taken_request<Storage, Resolutions>(requested_type)
               ? (result = index, true)
               : (++index, false)) ||
          ...));
  return result;
}

// Applies a resolution to a source. A source the transaction keeps is an
// lvalue, so that a reference or a pointer can refer to it; a value is moved
// out of it as out of any other source. Only the resolutions that refer are
// then made for an lvalue source as well.
template <typename Resolution, typename Activation, typename Context,
          typename MaterializedSource>
DINGO_ALWAYS_INLINE resolved_address resolve_address_from(
    Activation &activation, Context &context, MaterializedSource &&source,
    type_descriptor requested_type) {
  if constexpr (std::is_lvalue_reference_v<MaterializedSource> &&
                !request_may_escape_v<typename Resolution::result_type>) {
    // The source is an lvalue here; it is moved on purpose.
    return activation.template resolve_address<Resolution>(
        context,
        static_cast<std::remove_reference_t<MaterializedSource> &&>(source),
        requested_type);
  } else {
    return activation.template resolve_address<Resolution>(
        context, std::forward<MaterializedSource>(source), requested_type);
  }
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
            ? (result = resolve_address_from<Resolutions>(
                   activation, context,
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
    return detail::described_type_v<detail::registered_type_t<Storage>>;
  }

  // Whether a request is of an address: an lvalue reference or a pointer. A
  // value is copied out of what it is resolved from before that ends.
  static constexpr bool refers(type_descriptor requested) {
    return requested.reference == type_reference_kind::lvalue ||
           requested.pointee != nullptr;
  }

  // The value an address request refers to.
  static constexpr type_descriptor referred_value(type_descriptor requested) {
    type_descriptor value =
        requested.pointee != nullptr ? *requested.pointee : requested;
    value.reference = type_reference_kind::none;
    value.cv = type_cv_flags::none;
    return value;
  }

  // A storage that makes a value for the request places it in the context,
  // which ends before the caller of the container uses an address into it,
  // or into what it owns, as the static container knows as it compiles. A
  // pointer the storage makes hands over what it points to.
  template <bool PlacesValue>
  static void check_caller_address(construction_scope scope,
                                   const resolved_address &resolved,
                                   const request_type &request,
                                   runtime_context_type &context) {
    if constexpr (!PlacesValue) {
      (void)scope, (void)resolved, (void)request, (void)context;
    } else if (scope.is_caller() && refers(request.requested_type) &&
               resolved.access == resolved_address::access_kind::borrow) {
      throw detail::make_type_not_convertible_exception(
          request.requested_type, registered_type(), context);
    }
  }

  // Whether a reference request refers to what a pointer the storage makes
  // points to: the storage publishes the pointer.
  static bool adopts(type_descriptor requested) {
    if constexpr (detail::hands_over_made_pointers_v<Storage>) {
      return requested.reference == type_reference_kind::lvalue &&
             detail::adopted_request_index<Storage>(request_resolutions{},
                                                    requested) !=
                 type_list_size_v<request_resolutions>;
    } else {
      (void)requested;
      return false;
    }
  }

  // Whether a value request is taken out of what a pointer the storage makes
  // points to: the storage publishes the pointer, and the value can be moved.
  static bool takes(type_descriptor requested) {
    if constexpr (detail::takes_made_pointee_v<Storage>) {
      if (refers(requested)) {
        return false;
      }
      type_descriptor pointee = referred_value(requested);
      pointee.reference = type_reference_kind::lvalue;
      return detail::taken_request_index<Storage>(request_resolutions{},
                                                  pointee) !=
             type_list_size_v<request_resolutions>;
    } else {
      (void)requested;
      return false;
    }
  }

  // The key a composed reference is published in the request cache under;
  // none when the binding has no cache.
  static const void *cached(const void *cache_key) noexcept {
    if constexpr (Storage::conversions::is_stable) {
      return cache_key;
    } else {
      (void)cache_key;
      return nullptr;
    }
  }

  // What the binding keeps of its instance, which a reference composed of it
  // may refer to; none when it keeps no instance.
  static detail::retention *retained(runtime_binding &binding) noexcept {
    if constexpr (state_type::cell_type::keeps_conversions) {
      return std::addressof(binding.state().cell().retained());
    } else {
      (void)binding;
      return nullptr;
    }
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  // Derives a request the binding does not publish, from what it tells of
  // itself, through the derivation the request carries.
  static resolved_address derive(interface_base &erased,
                                 construction_scope scope,
                                 runtime_context_type &context,
                                 const request_type &request,
                                 const void *cache_key) {
    if (request.derive == nullptr) {
      throw detail::make_type_not_convertible_exception(
          request.requested_type, registered_type(), context);
    }
    auto &binding = static_cast<runtime_binding &>(erased);
    const detail::derivation_site site{
        {state_type::cell_type::keeps_conversions,
         detail::places_made_values_v<Storage>, adopts(request.requested_type),
         takes(request.requested_type)},
        retained(binding),
        registered_type(),
        cached(cache_key)};
    return request.derive(&erased, site, scope, &context);
  }

  static resolved_address dispatch_request(interface_base &erased,
                                           construction_scope scope,
                                           runtime_context_type &context,
                                           const request_type &request,
                                           const void *cache_key) {
    if constexpr (type_list_size_v<request_resolutions> == 0) {
      return derive(erased, scope, context, request, cache_key);
    } else {
      const auto resolution_index = detail::resolution_request_index(
          request_resolutions{}, request.requested_type);
      if (resolution_index == type_list_size_v<request_resolutions>) {
        return derive(erased, scope, context, request, cache_key);
      }

      auto &binding = static_cast<runtime_binding &>(erased);
      auto &state = binding.state();
      auto result = state.with_resolution_container(
          [&](auto &container) -> resolved_address {
            using activation_type = detail::binding_activation<
                typename state_type::cell_type,
                std::remove_reference_t<decltype(container)> &>;
            activation_type activation{state.cell(), container, scope};
            return activation.template with_source<void>(
                context, [&](auto &&source) -> resolved_address {
                  auto resolved = detail::resolve_indexed_address(
                      activation, context,
                      std::forward<decltype(source)>(source), resolution_index,
                      request_resolutions{}, request.requested_type);
                  check_caller_address<detail::materializes_value_source_v<
                      Storage, runtime_context_type,
                      std::remove_reference_t<decltype(container)>>>(
                      scope, resolved, request, context);
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
