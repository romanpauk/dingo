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

#include <algorithm>
#include <cassert>
#include <memory>
#include <type_traits>

namespace dingo {
namespace detail {
// Selects the constructor of a level of the local bindings of a registration.
struct local_level_t {};
inline constexpr local_level_t local_level{};
} // namespace detail

namespace detail {
constexpr std::size_t aligned_to(std::size_t size,
                                 std::size_t alignment) noexcept {
  return (size + alignment - 1) / alignment * alignment;
}

// Where the cell of a registration lies in its record: after its header.
template <typename Cell>
inline constexpr std::size_t runtime_cell_offset_v =
    aligned_to(sizeof(runtime_binding_header), alignof(Cell));

template <typename Cell>
Cell &runtime_cell(runtime_binding_header &header) noexcept {
  return *reinterpret_cast<Cell *>(reinterpret_cast<unsigned char *>(&header) +
                                   runtime_cell_offset_v<Cell>);
}

// The record of a registration, in one allocation: the header the bindings
// of its interfaces share, the cell of its storage, and the binding of each
// of its interfaces. Only the edge of a binding knows the type of the cell.
template <typename Cell, typename Binding, std::size_t Interfaces>
struct runtime_binding_record {
  static constexpr std::size_t bindings_offset =
      aligned_to(runtime_cell_offset_v<Cell> + sizeof(Cell), alignof(Binding));
  static constexpr std::size_t size =
      bindings_offset + Interfaces * sizeof(Binding);
  static constexpr std::size_t alignment = std::max(
      {alignof(runtime_binding_header), alignof(Cell), alignof(Binding)});

  static Binding *binding(runtime_binding_header &header,
                          std::size_t index) noexcept {
    return reinterpret_cast<Binding *>(
               reinterpret_cast<unsigned char *>(&header) + bindings_offset) +
           index;
  }

  // Destroys the cell, the only part of the record that is not trivial.
  static void destroy(void *record) noexcept {
    runtime_cell<Cell>(*static_cast<runtime_binding_header *>(record)).~Cell();
  }
};
} // namespace detail

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
    const materialization_facts &materialization = node.materialization;
    const bool resolved = node.resolved(storage);
    recursion_guard_wrapper guard(
        context, storage, node.leaf,
        materialization_guarded(materialization, resolved));
    resolved_address result{};
    if (materialization_retains(materialization, resolved, true)) {
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
    // An address into a value placed in the context is refused to the
    // caller, which uses it after the context ends.
    if (refuses_caller_address(
            materialization, materialized_request_of(request.requested_type)) &&
        scope.is_caller() &&
        result.access == resolved_address::access_kind::borrow) {
      throw make_type_not_convertible_exception(request.requested_type,
                                                node.registered, context);
    }
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

// The edge of the runtime binding of an interface and a storage: its node,
// and the functions of its type the node points to, of the record of the
// registration. Everything else is the engine of the container.
template <typename Container, typename Type, typename Storage>
struct runtime_binding_edge {
  using runtime_context_type =
      runtime_context<typename Container::allocator_type>;
  using rtti_type = typename Container::rtti_type;
  using interface_base =
      runtime_binding_interface<rtti_type, runtime_context_type>;
  using node_type = runtime_binding_node<rtti_type, runtime_context_type>;
  using cell_type = detail::binding_cell<Storage>;
  using request_resolutions =
      typename detail::binding_resolutions<Type, Storage>::type;

private:
  static constexpr type_descriptor registered_type() {
    return detail::described_type_v<detail::registered_type_t<Storage>>;
  }

  static constexpr const detail::binding_facts &facts =
      detail::binding_facts_of<Type, Storage>::value;

  static cell_type &cell(interface_base &binding) noexcept {
    return detail::runtime_cell<cell_type>(binding.header());
  }

  // The container the factory resolves through: the level of local bindings
  // once there is one, and else the container the registration is made in.
  static Container &resolution_container(interface_base &binding) noexcept {
    auto &header = binding.header();
    return *static_cast<Container *>(
        header.level != nullptr ? header.level : header.container);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  // Materializes the source of the binding in a construction scope, through
  // the container its factory resolves in, and applies to it the resolution
  // at a run-time index, in the scope of the request.
  static resolved_address
  resolve_source(interface_base &binding, construction_scope scope,
                 construction_scope construction, runtime_context_type &context,
                 std::size_t resolution_index, type_descriptor requested) {
    using activation_type = detail::binding_activation<cell_type, Container &>;
    activation_type activation{cell(binding), resolution_container(binding),
                               scope};
    auto application = activation.application();
    return activation.with_runtime_source(
        construction, context, [&](auto &&source) -> resolved_address {
          return detail::resolve_indexed_address(
              application, context, std::forward<decltype(source)>(source),
              resolution_index, request_resolutions{}, requested);
        });
  }

  static void *storage_of(interface_base &binding) noexcept {
    return std::addressof(cell(binding).storage());
  }

  static void *cell_of(interface_base &binding) noexcept {
    return std::addressof(cell(binding));
  }

  // What the binding keeps of its instance, which a reference composed of it
  // may refer to; none when it keeps no instance.
  static detail::retention *retained(interface_base &binding) noexcept {
    if constexpr (cell_type::keeps_conversions) {
      return std::addressof(cell(binding).retained());
    } else {
      (void)binding;
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
    static_cast<cell_type *>(cell)->reset();
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

public:
  static constexpr node_type node{
      &detail::runtime_binding_engine<rtti_type,
                                      runtime_context_type>::dispatch,
      &facts,
      registered_type(),
      detail::described_type_v<leaf_type_t<typename Storage::type>>,
      detail::materialization_facts_of<Storage, runtime_context_type,
                                       Container>(),
      Storage::conversions::is_stable,
      resolver(),
      &storage_of,
      &cell_of,
      &retained,
      &is_resolved,
      &reset_cell};
};

} // namespace dingo
