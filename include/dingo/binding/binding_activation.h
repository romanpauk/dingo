//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/binding_cell.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/context/context_base.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/type_storage_traits.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

// One resolution of a binding: the cell it resolves, the dependencies its
// factory resolves through and the scope of the request. Both containers
// resolve their bindings through it; they differ only in how they select the
// resolution and in the dependencies they pass.
//
// Dependencies is what the factory of the binding receives as its container:
// a container reference, or a value that routes requests through the local
// bindings of a static registration first. A tracked activation belongs to a
// runtime transaction: it rolls back what a failed resolution left in the
// cell.
template <typename Cell, typename Dependencies, bool Tracked>
struct binding_activation {
  using cell_type = Cell;
  using storage_type = typename Cell::storage_type;
  using owner_type = std::remove_reference_t<Dependencies>;

  Cell &cell;
  Dependencies dependencies;
  construction_scope scope;

  template <typename T, typename Context, typename Source>
  decltype(auto) construct_conversion(construction_scope construction,
                                      Context &context, Source &&source) {
    return cell.template construct_conversion<T, Tracked>(
        construction, context, std::forward<Source>(source));
  }

  template <typename T, typename Context, typename Source>
  decltype(auto) retain_conversion(Context &context, Source &&source) {
    return detail::retain_binding_conversion<T>(
        scope, cell.storage(), *this, context, std::forward<Source>(source));
  }

  // Materializes the storage and passes its source to fn. An address request
  // (an lvalue reference or a pointer) of a storage that yields a value
  // places the value in the context.
  template <typename Request, typename Operation, typename Context, typename Fn>
  decltype(auto) with_source(Context &context, Fn &&fn) {
    if constexpr (Tracked) {
      static_assert(is_runtime_context_v<Context>,
                    "a tracked activation resolves in a runtime transaction");
      return with_tracked_source(context, std::forward<Fn>(fn));
    } else if constexpr ((std::is_lvalue_reference_v<Request> ||
                          std::is_pointer_v<Request>) &&
                         materializes_value_source_v<storage_type, Context,
                                                     owner_type>) {
      return with_context_lvalue_source(scope, context, cell.storage(),
                                        dependencies, std::forward<Fn>(fn));
    } else if constexpr (is_runtime_context_v<Context>) {
      return with_runtime_binding_source(scope, context, cell.storage(),
                                         dependencies, std::forward<Fn>(fn));
    } else if constexpr (operation_requires_source_retention_v<Operation> &&
                         !std::is_same_v<std::remove_cv_t<Context>,
                                         no_dependency_context>) {
      return with_retained_binding_source(scope, context, cell.storage(),
                                          dependencies, std::forward<Fn>(fn));
    } else {
      return with_binding_source(scope, context, cell.storage(), dependencies,
                                 std::forward<Fn>(fn));
    }
  }

  // The address of the instance of a resolution, applied to a source.
  template <typename Resolution, typename Context, typename Source>
  resolved_address resolve_address(Context &context, Source &&source,
                                   type_descriptor requested_type) {
    return resolve_address_from_source<Resolution, storage_type>(
        scope, *this, context, std::forward<Source>(source), requested_type,
        describe_type<typename storage_type::type>());
  }

  // The address of the instance of a resolution selected at compile time.
  // The address is deduced: a deduced return type is instantiated at the
  // call and does not add a level to the template depth of a dependency
  // chain.
  template <typename Request, typename Resolution, typename Context>
  decltype(auto) resolve_request_address(Context &context) {
    return with_source<Request, typename Resolution::operation>(
        context, [&](auto &&source) {
          return resolve_address<Resolution>(
              context, std::forward<decltype(source)>(source),
              describe_type<Request>());
        });
  }

  // Passes the instance of a resolution selected at compile time to fn.
  template <typename Request, typename Resolution, typename Context,
            typename Fn>
  decltype(auto) consume_request(Context &context, Fn &&fn) {
    return with_source<Request, typename Resolution::operation>(
        context, [&](auto &&source) -> decltype(auto) {
          return apply_consumed_resolution<Request, Resolution, storage_type>(
              *this, context, std::forward<decltype(source)>(source),
              std::forward<Fn>(fn));
        });
  }

private:
  // A source retained by unresolved storage persists in the transaction. A
  // failed transaction resets what it left in the cell; a sealed one resets
  // the cell when its runtime is destroyed.
  template <typename Context, typename Fn>
  decltype(auto) with_tracked_source(Context &context, Fn &&fn) {
    using materialization_traits =
        storage_materialization_traits<typename storage_type::tag_type,
                                       typename storage_type::type>;
    auto &storage = cell.storage();
    if constexpr (materialization_traits::can_retain_source) {
      if (materialization_traits::retains_source(storage)) {
        // The reset flag is selected by the callback, not carried by it.
        context.on_rollback(cell.resets_storage_on_failure()
                                ? +[](void *self, void *) noexcept {
                                    static_cast<Cell *>(self)->reset(true);
                                  }
                                : +[](void *self, void *) noexcept {
                                    static_cast<Cell *>(self)->reset(false);
                                  },
                            std::addressof(cell));
        auto materialize = [&]() -> decltype(auto) {
          return with_runtime_binding_source(persistent_scope, context, storage,
                                             dependencies,
                                             std::forward<Fn>(fn));
        };
        using result_type = std::invoke_result_t<decltype(materialize) &>;
        auto reset_on_destruction = [&] {
          context.add_runtime_destructor(
              std::addressof(cell), +[](void *self) noexcept {
                static_cast<Cell *>(self)->reset(true);
              });
        };
        if constexpr (std::is_void_v<result_type>) {
          materialize();
          reset_on_destruction();
        } else if constexpr (std::is_reference_v<result_type>) {
          result_type result = materialize();
          reset_on_destruction();
          return std::forward<result_type>(result);
        } else {
          auto result = materialize();
          reset_on_destruction();
          return result;
        }
      }
    }
    return with_runtime_binding_source(scope, context, storage, dependencies,
                                       std::forward<Fn>(fn));
  }
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
