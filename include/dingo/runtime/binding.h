//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/binding/activation.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/resolution.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/memory.h>
#include <dingo/type/shape.h>

#include <cassert>
#include <memory>
#include <type_traits>

namespace dingo {
namespace detail {
// Selects the constructor of a level of the local bindings of a registration.
struct local_level_t {};
inline constexpr local_level_t local_level{};
} // namespace detail

// The state of a runtime registration: its binding cell, the request cache of
// its interfaces and the level of local bindings its factory resolves through
// once the registration creates one. The level is a container of the type of
// the container the registration is made in, so the factory is instantiated
// once, whichever of the two it resolves through.
template <typename Owner, typename InstanceContainer, typename Cell>
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
              detail::local_level, parent(), owner_->get_allocator()));
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
  // The container the registration is made in.
  InstanceContainer *parent() {
    static_assert(
        std::is_same_v<
            InstanceContainer,
            std::remove_reference_t<decltype(owner_->registration_owner())>>);
    return std::addressof(owner_->registration_owner());
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

// The resolution a runtime binding of a storage publishes, as data.
template <typename Storage, typename Resolution>
constexpr published_resolution published_resolution_of() {
  using target = typename Resolution::target_type;
  published_resolution resolution{described_type_v<target>, {}, false, false};
  if constexpr (std::is_pointer_v<target>) {
    using pointee = std::remove_pointer_t<target>;
    if constexpr (adopts_made_pointer_v<Storage, pointee>) {
      resolution.adopted = described_type_v<pointee &>;
      resolution.adopts = true;
      using value = std::remove_cv_t<pointee>;
      resolution.takes = takes_made_pointee_v<Storage> &&
                         std::is_constructible_v<value, value &&>;
    }
  }
  return resolution;
}

template <typename Storage, typename Resolutions>
struct published_resolution_table;

template <typename Storage, typename... Resolutions>
struct published_resolution_table<Storage, type_list<Resolutions...>> {
  static constexpr published_resolution value[] = {
      published_resolution_of<Storage, Resolutions>()...};
};

template <typename Storage>
struct published_resolution_table<Storage, type_list<>> {
  static constexpr const published_resolution *value = nullptr;
};

// What a runtime binding of an interface and a storage tells of itself.
template <typename Interface, typename Storage, typename Resolutions>
struct runtime_binding_facts {
  using interface_type = std::remove_cv_t<Interface>;
  static constexpr binding_facts value{
      published_resolution_table<Storage, Resolutions>::value,
      type_list_size_v<Resolutions>,
      flags<binding_trait>{}
          .set(binding_trait::keeps, binding_cell<Storage>::keeps_conversions)
          .set(binding_trait::places, places_made_values_v<Storage>)
          .set(binding_trait::publishes_early,
               scope_allows_cycles_v<typename Storage::tag_type>),
      slices_v<interface_type, leaf_type_t<typename Storage::type>>,
      described_type_v<interface_type>};
};

// Derives a request a binding does not publish, from what it tells of
// itself, through the derivation the request carries. It is kept out of the
// dispatch, where a request the binding publishes is resolved, and it is of
// the context alone, not of the binding.
template <typename Context, typename Request>
DINGO_NOINLINE resolved_address derive_unpublished(
    void *binding, retention *kept, const binding_facts &facts,
    type_descriptor registered, construction_scope scope, Context &context,
    const Request &request, const void *cache_key) {
  if (request.derive == nullptr) {
    throw make_type_not_convertible_exception(request.requested_type,
                                              registered, context);
  }
  const derivation_site site{kept, registered, request.requested_type,
                             cache_key, &facts};
  return request.derive(binding, site, scope, &context, *request.node);
}

// Refuses the caller an address into a value a storage places in the
// context, which ends before the caller uses it.
template <typename Context>
void check_caller_address(bool places, construction_scope scope,
                          const resolved_address &resolved,
                          type_descriptor requested, type_descriptor registered,
                          Context &context) {
  if (places && scope.is_caller() &&
      (requested.reference == type_reference_kind::lvalue ||
       requested.pointee != nullptr) &&
      resolved.access == resolved_address::access_kind::borrow) {
    throw make_type_not_convertible_exception(requested, registered, context);
  }
}

// Applies a resolution to a source. A source the transaction keeps is an
// lvalue, so that a reference or a pointer can refer to it; a value is moved
// out of it as out of any other source. Only the resolutions that refer are
// then made for an lvalue source as well.
template <typename Resolution, typename Application, typename Context,
          typename MaterializedSource>
DINGO_ALWAYS_INLINE resolved_address resolve_address_from(
    Application &application, Context &context, MaterializedSource &&source,
    type_descriptor requested_type) {
  if constexpr (std::is_lvalue_reference_v<MaterializedSource> &&
                !request_may_escape_v<typename Resolution::result_type>) {
    // The source is an lvalue here; it is moved on purpose.
    return application.template resolve_address<Resolution>(
        context,
        static_cast<std::remove_reference_t<MaterializedSource> &&>(source),
        requested_type);
  } else {
    return application.template resolve_address<Resolution>(
        context, std::forward<MaterializedSource>(source), requested_type);
  }
}

// Applies the resolution at a run-time index to a source. It is given the
// application of the binding, not its activation, so that it is instantiated
// once for both containers a factory may resolve through.
template <typename Application, typename Context, typename MaterializedSource,
          typename... Resolutions>
resolved_address
resolve_indexed_address(Application &application, Context &context,
                        MaterializedSource &&source,
                        std::size_t resolution_index, type_list<Resolutions...>,
                        type_descriptor requested_type) {
  resolved_address result{};
  std::size_t index = 0;
  // Short-circuiting guarantees that exactly one arm forwards the source.
  const bool matched =
      (((resolution_index == index++)
            ? (result = resolve_address_from<Resolutions>(
                   application, context,
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

  static constexpr const detail::binding_facts &facts =
      detail::runtime_binding_facts<Type, Storage, request_resolutions>::value;

  // A storage that makes a value for the request places it in the context,
  // which ends before the caller of the container uses an address into it,
  // or into what it owns, as the static container knows as it compiles. A
  // pointer the storage makes hands over what it points to.
  template <typename Resolver>
  struct places_value : std::bool_constant<detail::materializes_value_source_v<
                            Storage, runtime_context_type, Resolver>> {};

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
    return detail::derive_unpublished(
        &erased, retained(static_cast<runtime_binding &>(erased)), facts,
        registered_type(), scope, context, request, cached(cache_key));
  }

  // Applies the resolution at a run-time index through the container the
  // binding resolves in, and tells whether its storage places a value, which
  // the caller is refused an address into. It is a function of its own:
  // MSVC's permissive mode loses the names a generic lambda in a generic
  // lambda captures.
  template <typename Resolver>
  static resolved_address
  resolve_in(Resolver &container, state_type &state, construction_scope scope,
             runtime_context_type &context, std::size_t resolution_index,
             const request_type &request, bool &places) {
    using activation_type =
        detail::binding_activation<typename state_type::cell_type, Resolver &>;
    places = places_value<Resolver>::value;
    activation_type activation{state.cell(), container, scope};
    auto application = activation.application();
    return activation.template with_source<void>(
        context, [&](auto &&source) -> resolved_address {
          return detail::resolve_indexed_address(
              application, context, std::forward<decltype(source)>(source),
              resolution_index, request_resolutions{}, request.requested_type);
        });
  }

  static resolved_address dispatch_request(interface_base &erased,
                                           construction_scope scope,
                                           runtime_context_type &context,
                                           const request_type &request,
                                           const void *cache_key) {
    if constexpr (type_list_size_v<request_resolutions> == 0) {
      return derive(erased, scope, context, request, cache_key);
    } else {
      const auto resolution_index =
          detail::published_index(facts, request.requested_type);
      if (resolution_index == type_list_size_v<request_resolutions>) {
        return derive(erased, scope, context, request, cache_key);
      }

      auto &binding = static_cast<runtime_binding &>(erased);
      auto &state = binding.state();
      bool places = false;
      auto result = state.with_resolution_container(
          [&](auto &container) -> resolved_address {
            return resolve_in(container, state, scope, context,
                              resolution_index, request, places);
          });
      detail::check_caller_address(places, scope, result,
                                   request.requested_type, registered_type(),
                                   context);
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
