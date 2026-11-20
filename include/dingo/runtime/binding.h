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
#include <dingo/runtime/conversion.h>
#include <dingo/runtime/memory.h>
#include <dingo/runtime/request.h>
#include <dingo/runtime/route.h>
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

// Selects the conversion a request of a binding is resolved by, as
// find_conversion finds it. A request the binding does not convert to is
// rejected.
template <typename Node, typename Context>
DINGO_NOINLINE selected_conversion
select_conversion(const Node &node, bool stable, Context &context,
                  const runtime_request &request) {
  return find_conversion(node.layout, node.interface_key, stable, request,
                         [&]() -> selected_conversion {
                           throw_type_not_convertible(request.requested,
                                                      node.registered, context);
                         });
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// The record of a registration, in one allocation: the header the bindings
// of its interfaces share, the cell of its storage, and the binding of each
// of its interfaces. Only the edge of a binding has the type of the cell.
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

} // namespace detail

// The edge of the runtime binding of an interface and a storage: its node,
// and the functions of its type the node points to, of the record of the
// registration.
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
  using semantics_type = typename execution_type::semantics_type;
  using stored_type = typename execution_type::stored_type;

private:
  static constexpr type_descriptor registered_type() {
    return detail::type_descriptor_v<detail::registered_type_t<Storage>>;
  }

  // The layer sources the binding publishes, and the tables of their types.
  using published = detail::execution::published_sources<execution_type>;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  // What the binding keeps of its instance, which what a conversion makes may
  // be kept with; none when it keeps no instance.
  static detail::retention *retained(interface_base &binding) noexcept {
    if constexpr (cell_type::keeps_conversions) {
      return std::addressof(
          detail::runtime_cell<cell_type>(binding.header()).retained());
    } else {
      (void)binding;
      return nullptr;
    }
  }

  // Resolves a request of the binding, by the conversion selected for it, in
  // one materialization of its source: the route of the conversion, compiled
  // for the types of the binding, and what the conversion takes after it.
  static resolved_address
  dispatch(const node_type &binding_node, interface_base &binding,
           construction_scope scope, runtime_context_type &context,
           const typename node_type::request_type &request,
           const void *cache_key) {
    // What a request passes to the caller lives as long as the container.
    const detail::selected_conversion selected = detail::select_conversion(
        binding_node, scope.is_caller(), context, request);
    if constexpr (!execution_type::modeled) {
      (void)selected;
      (void)cache_key;
      return {};
    } else {
      using access = resolved_address::access_kind;
      using sources = typename published::sources;
      const detail::execution::binding_layout &layout = *binding_node.layout;
      auto &cell = detail::runtime_cell<cell_type>(binding.header());
      detail::binding_activation<cell_type, Container &> activation{
          cell, detail::resolution_container<Container>(binding.header()),
          scope};
      // A reset of the cell this request registers clears the cache too.
      const bool registers_reset =
          activation.template registers_reset<runtime_context_type>();
      const resolved_address result = activation.use_source(
          context, cached_cell_access{{cell}, binding.cache_slot()},
          [&](auto &&source) -> resolved_address {
            store_type store(
                context, scope, retained(binding), request.requested,
                detail::type_descriptor_v<detail::registered_type_t<Storage>>);
            auto start = detail::conversion_start<stored_type,
                                                  execution_type::scope.makes>(
                std::forward<decltype(source)>(source), store);
            if (selected.derived != nullptr) {
              return selected.finish(sources::template run_route<false>(
                                         selected.route, start, store),
                                     *selected.derived,
                                     layout.tables[selected.route], store);
            }
            const detail::execution::closed_answer &answer = *selected.answer;
            if constexpr (sources::answer_table::any_value) {
              if (answer.value_route) {
                const detail::execution::erased_carrier reached =
                    sources::template run_route<true>(answer.route, start,
                                                      store);
                return {reached.object, access::borrow,
                        resolved_address::made_kind::copy, 0, answer.quals};
              }
            }
            return detail::finish_answer(
                sources::template run_route<false>(answer.route, start, store)
                    .object,
                answer, layout);
          });
      if (cache_key != nullptr && binding_node.caches &&
          selected.stable_address() && result.access == access::borrow) {
        if (registers_reset) {
          *binding.cache_slot() = {cache_key, result.address};
        } else {
          binding.publish_cache(context, cache_key, result.address);
        }
      }
      return result;
    }
  }

  // The access of a cell whose reset, as a transaction rolls back, clears
  // the cache of its record, which may hold an address in it.
  struct cached_cell_access : detail::cell_access<cell_type> {
    detail::cache::entry *cache;

    auto rollback() const noexcept { return &rollback_cell; }
    void *rollback_argument() const noexcept { return cache; }

    static void rollback_cell(void *cell, void *cache) noexcept {
      detail::cell_access<cell_type>::reset_cell(cell);
      *static_cast<detail::cache::entry *>(cache) = {};
    }
  };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

public:
  static constexpr node_type node{
      &dispatch,
      registered_type(),
      &detail::execution::shape_key<
          detail::execution::request_interface_t<Type>>,
      detail::execution::published_layout<execution_type>(),
      detail::cache::is_stable_storage_v<Storage>,
      detail::runtime_cell_offset_v<cell_type>};
};

} // namespace dingo
