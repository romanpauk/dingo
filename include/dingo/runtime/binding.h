//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/meta.h>

#include <dingo/binding/activation.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/resolution.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/execution.h>
#include <dingo/runtime/memory.h>
#include <dingo/runtime/request.h>
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

namespace detail {
constexpr std::size_t align_up(std::size_t size,
                               std::size_t alignment) noexcept {
  return (size + alignment - 1) / alignment * alignment;
}

// Where the cell of a registration lies in its record: after its header.
template <typename Cell>
inline constexpr std::size_t runtime_cell_offset_v =
    align_up(sizeof(runtime_binding_header), alignof(Cell));

template <typename Cell>
Cell &runtime_cell(runtime_binding_header &header) noexcept {
  return *reinterpret_cast<Cell *>(reinterpret_cast<unsigned char *>(&header) +
                                   runtime_cell_offset_v<Cell>);
}

// The container the factory of a binding resolves through: the level of local
// bindings once there is one, and else the container the registration is made
// in.
template <typename Container>
Container &resolution_container(runtime_binding_header &header) noexcept {
  return *static_cast<Container *>(header.level != nullptr ? header.level
                                                           : header.container);
}

// The record of a registration, in one allocation: the header the bindings
// of its interfaces share, the cell of its storage, and the binding of each
// of its interfaces. Only the edge of a binding knows the type of the cell.
template <typename Cell, typename Binding, std::size_t Interfaces>
struct runtime_binding_record {
  static constexpr std::size_t bindings_offset =
      align_up(runtime_cell_offset_v<Cell> + sizeof(Cell), alignof(Binding));
  static constexpr std::size_t size =
      bindings_offset + Interfaces * sizeof(Binding);
  static constexpr std::size_t alignment =
      strictest_alignment<runtime_binding_header, Cell, Binding>();

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

// The engine of the runtime bindings of a container. It plans a request of a
// binding once, from what the binding publishes, and runs the plan of the
// lifetime the request needs: one function for every binding of the
// container.
template <typename RTTI, typename Context> struct runtime_binding_engine {
  using node_type = runtime_binding_node<RTTI, Context>;
  using interface_type = typename node_type::interface_type;
  using request_type = typename node_type::request_type;

  // Plans a request, guards the binding against recursion, retains the
  // source an unresolved storage retains in the transaction, which resets
  // the cell if it fails, and publishes an address that lives as long as the
  // container in the request cache under its key, if it has one.
  static resolved_address dispatch(const node_type &node,
                                   interface_type &binding,
                                   construction_scope scope, Context &context,
                                   const request_type &request,
                                   const void *cache_key) {
    if (node.layout == nullptr || !request.modeled ||
        request.interface_key != node.interface_key) {
      throw make_type_not_convertible_exception(request.requested,
                                                node.registered, context);
    }
    const auto &plans = request.plans->find(&node, [&] {
      return execution::plan_runtime(*node.layout, request.layout);
    });
    // What a request passes to the caller lives as long as the container.
    const auto &plan = scope.is_caller() ? plans.stable : plans.transient;
    if (!plan.found) {
      throw make_type_not_convertible_exception(request.requested,
                                                node.registered, context);
    }
    const resolved_address result = run_materialization<true, true>(
        node.materialization, node_access{node, binding}, scope, context,
        [&](construction_scope construction, auto &materialized) {
          return node.run(binding, scope, construction, context, plan, request,
                          erase_hook(materialized));
        });
    if (cache_key != nullptr && node.caches && plan.stable_address &&
        result.access == resolved_address::access_kind::borrow) {
      binding.publish_cache(context, cache_key, result.address);
    }
    return result;
  }

  // The access of a binding to its materialization, through its node.
  struct node_access {
    const node_type &node;
    interface_type &binding;

    bool resolved() const noexcept { return node.resolved(cell_address()); }
    type_descriptor leaf() const noexcept { return node.leaf; }
    void *cell_address() const noexcept {
      return reinterpret_cast<unsigned char *>(&binding.header()) +
             node.cell_offset;
    }
    auto reset() const noexcept { return node.reset; }
    auto rollback() const noexcept { return &rollback_cell; }
    void *rollback_argument() const noexcept {
      return const_cast<void *>(static_cast<const void *>(&node));
    }

    static void rollback_cell(void *cell, void *node) noexcept {
      static_cast<const node_type *>(node)->reset(cell);
    }
  };
};

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
  using store_type = detail::context_store<runtime_context_type>;
  using execution_type =
      detail::binding_execution<Storage, std::remove_cv_t<Type>>;
  using plan_type = typename execution_type::plan;
  using stored_type = typename execution_type::stored_type;

private:
  static constexpr type_descriptor registered_type() {
    return detail::described_type_v<detail::registered_type_t<Storage>>;
  }

  // The layer sources the binding publishes, and the tables of their types.
  struct planned {
    static constexpr bool abstract = execution_type::abstract;
    using identities = typename plan_type::universe_type::identities;
    using sources = detail::execution::binding_sources<
        plan_type, plan_type::template shape<abstract>::sources, identities,
        stored_type, typename execution_type::leaf_type,
        typename execution_type::interface_type>;
    using layers = detail::execution::runtime_layers<identities>;
    static constexpr detail::execution::binding_layout value{
        sources::closed.states,
        sources::used.count,
        sources::closure.count,
        plan_type::context(abstract),
        layers::value,
        layers::count,
        sources::closed_tables.at,
        sources::route_finishes.at,
        sources::groups.at,
        sources::shapes.count,
        sources::answers.at};
  };

  static constexpr const detail::execution::binding_layout *layout() {
    if constexpr (execution_type::modeled) {
      return &planned::value;
    } else {
      return nullptr;
    }
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  // What the binding keeps of its instance, which what a plan makes may be
  // kept with; none when it keeps no instance.
  static detail::retention *retained(interface_base &binding) noexcept {
    if constexpr (cell_type::keeps_conversions) {
      return std::addressof(
          detail::runtime_cell<cell_type>(binding.header()).retained());
    } else {
      (void)binding;
      return nullptr;
    }
  }

  // Materializes the source of the binding in a construction scope, through
  // the container its factory resolves in, and runs a plan of a request of
  // it, in the scope of the request.
  static resolved_address run(interface_base &binding, construction_scope scope,
                              construction_scope construction,
                              runtime_context_type &context,
                              const typename node_type::plan_type &plan,
                              const typename node_type::request_type &request,
                              const detail::materialized_hook &materialized) {
    using activation_type = detail::binding_activation<cell_type, Container &>;
    activation_type activation{
        detail::runtime_cell<cell_type>(binding.header()),
        detail::resolution_container<Container>(binding.header()), scope};
    return activation.use_runtime_source(
        construction, context, [&](auto &&source) -> resolved_address {
          // What the plan keeps is undone before the cell is reset.
          materialized();
          store_type store(
              context, scope, retained(binding), request.requested,
              detail::described_type_v<detail::registered_type_t<Storage>>);
          detail::execution::store_view transient{store, false};
          detail::execution::erased_carrier reached = detail::execution::erase(
              detail::plan_start<stored_type, execution_type::scope.makes>(
                  std::forward<decltype(source)>(source), transient));
          for (std::size_t index = 0; index < plan.count; ++index) {
            const auto &call = plan.calls[index];
            reached = (reached.owned ? call.owned : call.borrowed)(
                reached, call.stable, store);
          }
          return detail::finish_request(reached,
                                        detail::execution::run_of(plan));
        });
  }

  // Resolves a request of the binding: by the answer of its closure, as the
  // code compiled it, when it answers the request; else by the plan of the
  // request, made once, from what the binding publishes.
  static resolved_address
  dispatch(const node_type &node, interface_base &binding,
           construction_scope scope, runtime_context_type &context,
           const typename node_type::request_type &request,
           const void *cache_key) {
    if (node.layout == nullptr || !request.modeled ||
        request.interface_key != node.interface_key) {
      throw detail::make_type_not_convertible_exception(
          request.requested, node.registered, context);
    }
    if constexpr (!execution_type::modeled) {
      return {};
    } else {
      const detail::execution::binding_layout &layout = *node.layout;
      // What a request passes to the caller lives as long as the container.
      const bool stable = scope.is_caller();
      for (std::size_t group = 0; group < layout.group_count; ++group) {
        if (layout.groups[group].shape == request.shape) {
          const detail::execution::closed_answers &answers =
              layout.answers[layout.groups[group].first +
                             static_cast<std::size_t>(request.layout.category) *
                                 4 +
                             request.layout.goal.quals[0]];
          const detail::execution::closed_answer &answer =
              stable ? answers.stable : answers.transient;
          if (answer.found) {
            return resolve_closed(binding, scope, context, request, answer,
                                  layout, node.caches ? cache_key : nullptr);
          }
          break;
        }
      }
      const auto &plans = request.plans->find(&node, [&] {
        return detail::execution::plan_runtime(layout, request.layout);
      });
      const auto &plan = stable ? plans.stable : plans.transient;
      if (!plan.found) {
        throw detail::make_type_not_convertible_exception(
            request.requested, node.registered, context);
      }
      return resolve(binding, scope, context, request, plan,
                     node.caches ? cache_key : nullptr);
    }
  }

  // Resolves a request the closure of the binding answers: runs the route of
  // the answer, compiled for the types of the binding, and takes what it
  // reaches as the answer says.
  static resolved_address
  resolve_closed(interface_base &binding, construction_scope scope,
                 runtime_context_type &context,
                 const typename node_type::request_type &request,
                 const detail::execution::closed_answer &answer,
                 const detail::execution::binding_layout &layout,
                 const void *cache_key) {
    using access = resolved_address::access_kind;
    using sources = typename planned::sources;
    auto &cell = detail::runtime_cell<cell_type>(binding.header());
    detail::binding_activation<cell_type, Container &> activation{
        cell, detail::resolution_container<Container>(binding.header()), scope};
    // A reset of the cell this request registers clears the cache too.
    const bool registers_reset =
        activation.template registers_reset<runtime_context_type>();
    const resolved_address result = activation.use_source(
        context, cached_cell_access{{cell}, binding.cache_slot()},
        [&](auto &&source) -> resolved_address {
          store_type store(
              context, scope, retained(binding), request.requested,
              detail::described_type_v<detail::registered_type_t<Storage>>);
          auto start =
              detail::plan_start<stored_type, execution_type::scope.makes>(
                  std::forward<decltype(source)>(source), store);
          if constexpr (sources::answer_table::any_value) {
            if (answer.value_route) {
              const detail::execution::erased_carrier reached =
                  sources::template run_route<true>(answer.route, start, store);
              return {reached.object, access::borrow,
                      resolved_address::made_kind::copy, 0, answer.quals};
            }
          }
          const detail::execution::erased_carrier reached =
              sources::template run_route<false>(answer.route, start, store);
          switch (answer.how) {
          case semantics::satisfaction::point:
            return {layout.tables[answer.route]->pointee(reached.object),
                    access::borrow};
          case semantics::satisfaction::take:
          case semantics::satisfaction::move:
            return {reached.object, access::consume};
          default:
            return {reached.object, access::borrow};
          }
        });
    if (cache_key != nullptr && answer.stable_address &&
        result.access == access::borrow) {
      if (registers_reset) {
        *binding.cache_slot() = {cache_key, result.address};
      } else {
        binding.publish_cache(context, cache_key, result.address);
      }
    }
    return result;
  }

  // The access of a cell whose reset, as a transaction rolls back, clears
  // the cache of its record, which may hold an address in it.
  struct cached_cell_access : detail::cell_access<cell_type> {
    detail::cache::entry *cache;

    auto rollback() const noexcept { return &rollback_cell; }
    void *rollback_argument() const noexcept { return cache; }

    static void rollback_cell(void *cell, void *cache) noexcept {
      static_cast<cell_type *>(cell)->reset();
      *static_cast<detail::cache::entry *>(cache) = {};
    }
  };

  // Resolves a request by a plan the closure of the binding does not answer:
  // runs the route of the plan, compiled for the types of the binding, and
  // the steps the request takes after it.
  static resolved_address
  resolve(interface_base &binding, construction_scope scope,
          runtime_context_type &context,
          const typename node_type::request_type &request,
          const detail::execution::runtime_plan &plan, const void *cache_key) {
    using sources = typename planned::sources;
    auto &cell = detail::runtime_cell<cell_type>(binding.header());
    detail::binding_activation<cell_type, Container &> activation{
        cell, detail::resolution_container<Container>(binding.header()), scope};
    const resolved_address result = activation.template use_source<void>(
        context, [&](auto &&source) -> resolved_address {
          store_type store(
              context, scope, retained(binding), request.requested,
              detail::described_type_v<detail::registered_type_t<Storage>>);
          auto start =
              detail::plan_start<stored_type, execution_type::scope.makes>(
                  std::forward<decltype(source)>(source), store);
          // The closure answers what a plan of a value of no step after its
          // route is: a plan takes steps after it.
          assert(!plan.value_route);
          detail::execution::erased_carrier reached =
              sources::template run_route<false>(plan.route, start, store);
          for (std::size_t index = 0; index < plan.count; ++index) {
            const auto &call = plan.calls[index];
            reached = (reached.owned ? call.owned : call.borrowed)(
                reached, call.stable, store);
          }
          return detail::finish_request(reached,
                                        detail::execution::run_of(plan));
        });
    if (cache_key != nullptr && plan.stable_address &&
        result.access == resolved_address::access_kind::borrow) {
      binding.publish_cache(context, cache_key, result.address);
    }
    return result;
  }

  static bool is_resolved([[maybe_unused]] void *cell) noexcept {
    if constexpr (detail::storage_can_reset<Storage>::value) {
      return static_cast<cell_type *>(cell)->storage().is_resolved();
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

public:
  static constexpr node_type node{
      &dispatch,
      registered_type(),
      detail::described_type_v<leaf_type_t<typename Storage::type>>,
      &detail::execution::shape_key<detail::execution::interface_of_t<Type>>,
      layout(),
      detail::materialization_facts_v<Storage, runtime_context_type,
                                      Container>(),
      detail::cache::is_stable_storage_v<Storage>,
      &run,
      detail::runtime_cell_offset_v<cell_type>,
      &retained,
      &is_resolved,
      &reset_cell};
};

} // namespace dingo
