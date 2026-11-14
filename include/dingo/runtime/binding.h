//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/binding/activation.h>
#include <dingo/binding/facts.h>
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

  // The container the factory resolves through: the level of local bindings
  // once there is one, and else the container the registration is made in.
  InstanceContainer &resolution_container() {
    return instance_container_ != nullptr ? *instance_container_ : *parent();
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

// Resets the cell of a binding whose resolution failed, unless it succeeded.
class node_reset_guard {
public:
  node_reset_guard(void (*reset)(void *) noexcept, void *cell) noexcept
      : reset_(reset), cell_(cell) {}
  node_reset_guard(const node_reset_guard &) = delete;
  node_reset_guard &operator=(const node_reset_guard &) = delete;
  ~node_reset_guard() {
    if (cell_ != nullptr) {
      reset_(cell_);
    }
  }
  void release() noexcept { cell_ = nullptr; }

private:
  void (*reset_)(void *) noexcept;
  void *cell_;
};

template <typename Context>
void check_caller_address(bool places, construction_scope scope,
                          const resolved_address &resolved,
                          type_descriptor requested, type_descriptor registered,
                          Context &context);

// The engine of the runtime bindings of a container. It resolves the request
// a binding publishes, or derives one it does not, of the node of the
// binding: one function for every binding of the container.
template <typename RTTI, typename Context> struct runtime_binding_engine {
  using node_type = runtime_binding_node<RTTI, Context>;
  using interface_type = typename node_type::interface_type;
  using request_type = typename node_type::request_type;

  static resolved_address dispatch(const node_type &node,
                                   interface_type &binding,
                                   construction_scope scope, Context &context,
                                   const request_type &request,
                                   const void *cache_key) {
    const binding_facts &facts = *node.facts;
    const void *key = node.caches ? cache_key : nullptr;
    const std::size_t index = published_index(facts, request.requested_type);
    if (index == facts.published_count) {
      return derive(node, binding, scope, context, request, key);
    }
    return resolve(node, binding, scope, context, request, index, key);
  }

  // Derives a request the binding does not publish, from what it tells of
  // itself, through the derivation the request carries.
  DINGO_NOINLINE static resolved_address
  derive(const node_type &node, interface_type &binding,
         construction_scope scope, Context &context,
         const request_type &request, const void *cache_key) {
    if (request.derive == nullptr) {
      throw make_type_not_convertible_exception(request.requested_type,
                                                node.registered, context);
    }
    const derivation_site site{node.retained(binding), node.registered,
                               request.requested_type, cache_key, node.facts};
    return request.derive(std::addressof(binding), site, scope, &context,
                          *request.node);
  }

  // Resolves the request a binding publishes at an index: guards the binding
  // against recursion, retains the source an unresolved storage retains in
  // the transaction, which resets the cell if it fails, refuses the caller an
  // address into a value the storage places in the context, and publishes a
  // borrowed address in the request cache under its key, if it has one.
  static resolved_address resolve(const node_type &node,
                                  interface_type &binding,
                                  construction_scope scope, Context &context,
                                  const request_type &request,
                                  std::size_t index, const void *cache_key) {
    void *storage = node.storage(binding);
    recursion_guard_wrapper guard(
        context, storage, node.leaf,
        node.guards_always ||
            (node.guards_unresolved && !node.resolved(storage)));
    resolved_address result{};
    if (node.retains_unresolved && !node.resolved(storage)) {
      // A source is retained only while the storage is unresolved, so a
      // failed transaction returns the storage to that state. The reset is
      // registered once the cell is resolved, after what it depends on, so
      // that a rollback, which runs the last registered first, resets the
      // cell before what it refers to. A cell that fails as it resolves, or as
      // its reset is registered, is reset at once.
      void *cell = node.cell(binding);
      node_reset_guard reset{node.reset, cell};
      result =
          node.resolve(binding, scope, construction_scope(persistent_scope),
                       context, index, request.requested_type);
      context.on_rollback(&reset_cell, cell,
                          const_cast<void *>(static_cast<const void *>(&node)));
      reset.release();
      context.add_runtime_destructor(cell, node.reset);
    } else {
      result = node.resolve(binding, scope, scope, context, index,
                            request.requested_type);
    }
    check_caller_address(node.places, scope, result, request.requested_type,
                         node.registered, context);
    // Request caching is intentionally stricter than conversion caching and
    // is published with transaction rollback tracking.
    if (cache_key != nullptr &&
        result.access == resolved_address::access_kind::borrow) {
      binding.publish_cache(context, cache_key, result.address);
    }
    return result;
  }

  static void reset_cell(void *cell, void *node) noexcept {
    static_cast<const node_type *>(node)->reset(cell);
  }
};

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
  using node_type = runtime_binding_node<rtti_type, runtime_context_type>;
  using request_resolutions =
      typename detail::binding_resolutions<Type, Storage>::type;

private:
  State state_;

  state_type &state() { return state_traits::ref(state_); }

  static constexpr type_descriptor registered_type() {
    return detail::described_type_v<detail::registered_type_t<Storage>>;
  }

  static constexpr const detail::binding_facts &facts =
      detail::binding_facts_of<Type, Storage>::value;

  // A storage that makes a value for the request places it in the context,
  // which ends before the caller of the container uses an address into it,
  // or into what it owns, as the static container knows as it compiles. A
  // pointer the storage makes hands over what it points to.
  template <typename Resolver>
  struct places_value : std::bool_constant<detail::materializes_value_source_v<
                            Storage, runtime_context_type, Resolver>> {};

  static runtime_binding &of(interface_base &erased) noexcept {
    return static_cast<runtime_binding &>(erased);
  }

  // The functions of the type of the binding, which its node gives the
  // engine of the container.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

  // Materializes the source of the binding in a construction scope, through
  // the container its factory resolves in, and applies to it the resolution
  // at a run-time index, in the scope of the request.
  static resolved_address
  resolve_source(interface_base &erased, construction_scope scope,
                 construction_scope construction, runtime_context_type &context,
                 std::size_t resolution_index, type_descriptor requested) {
    auto &state = of(erased).state();
    using activation_type =
        detail::binding_activation<typename state_type::cell_type,
                                   container_type &>;
    activation_type activation{state.cell(), state.resolution_container(),
                               scope};
    auto application = activation.application();
    return activation.with_runtime_source(
        construction, context, [&](auto &&source) -> resolved_address {
          return detail::resolve_indexed_address(
              application, context, std::forward<decltype(source)>(source),
              resolution_index, request_resolutions{}, requested);
        });
  }

  static void *storage_of(interface_base &erased) noexcept {
    return std::addressof(of(erased).state().storage());
  }

  static void *cell_of(interface_base &erased) noexcept {
    return std::addressof(of(erased).state().cell());
  }

  // What the binding keeps of its instance, which a reference composed of it
  // may refer to; none when it keeps no instance.
  static detail::retention *retained(interface_base &erased) noexcept {
    if constexpr (state_type::cell_type::keeps_conversions) {
      return std::addressof(of(erased).state().cell().retained());
    } else {
      (void)erased;
      return nullptr;
    }
  }

  static bool is_resolved([[maybe_unused]] void *storage) noexcept {
    if constexpr (detail::storage_can_reset<Storage>::value) {
      return static_cast<Storage *>(storage)->is_resolved();
    } else {
      return true;
    }
  }

  static void reset_cell(void *cell) noexcept {
    static_cast<typename state_type::cell_type *>(cell)->reset();
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  // A binding that publishes no resolution resolves nothing: the engine
  // derives every request of it.
  static constexpr auto resolver() {
    if constexpr (type_list_size_v<request_resolutions> == 0) {
      return static_cast<decltype(&resolve_source)>(nullptr);
    } else {
      return &resolve_source;
    }
  }

  using scope_policy = scope_traits<typename Storage::tag_type>;
  static constexpr bool guarded =
      detail::materialization_guard_enabled_v<scope_policy::guard, Storage,
                                              runtime_context_type>;

  static constexpr node_type node{
      &detail::runtime_binding_engine<rtti_type,
                                      runtime_context_type>::dispatch,
      &facts,
      registered_type(),
      detail::described_type_v<leaf_type_t<typename Storage::type>>,
      guarded &&scope_policy::guard == materialization_guard::always,
      guarded &&scope_policy::guard == materialization_guard::while_unresolved,
      detail::scope_materialization<
          typename Storage::tag_type>::retains_unresolved_source,
      places_value<container_type>::value,
      Storage::conversions::is_stable,
      resolver(),
      &storage_of,
      &cell_of,
      &retained,
      &is_resolved,
      &reset_cell};

public:
  template <typename... Args>
  runtime_binding(Args &&...args)
      : interface_base(&node), state_(std::forward<Args>(args)...) {
    this->cache_slot(state().cache_slot());
  }

  auto &get_container() { return state().container(); }
};

} // namespace dingo
