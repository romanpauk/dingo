//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/cache.h>
#include <dingo/binding/materialization.h>
#include <dingo/binding/model.h>
#include <dingo/binding/resolution.h>
#include <dingo/context/resolution.h>
#include <dingo/conversion/source.h>
#include <dingo/storage/retention.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

// A context that cannot resolve dependencies, for factories that have none.
struct no_dependency_context {
  template <typename T, typename Container> T resolve(Container &) = delete;
};

template <typename Request, typename StorageType>
inline constexpr bool stored_request_identity_v = [] {
  using stored_type = std::remove_cv_t<
      std::remove_reference_t<typename StorageType::stored_type>>;
  return std::is_same_v<Request, stored_type> ||
         std::is_same_v<Request, stored_type &> ||
         std::is_same_v<Request, const stored_type &> ||
         std::is_same_v<Request, stored_type *> ||
         std::is_same_v<Request, const stored_type *>;
}();

template <typename Storage, typename = void>
struct storage_can_reset : std::false_type {};

template <typename Storage>
struct storage_can_reset<
    Storage,
    std::void_t<decltype(std::declval<Storage &>().reset()),
                decltype(std::declval<const Storage &>().is_resolved())>>
    : std::true_type {};

// What a storage that makes an instance for each request keeps of it:
// nothing.
struct no_retention {
  void clear() noexcept {}
};

// Whether a storage keeps what is converted of its instance itself, as a
// cyclical one does, for the graph it constructs may refer to it.
template <typename Storage, typename = void>
struct storage_retains : std::false_type {};

template <typename Storage>
struct storage_retains<
    Storage, std::void_t<decltype(std::declval<Storage &>().retained())>>
    : std::true_type {};

// What a cell keeps of the instance of a storage that keeps it, unless the
// storage keeps it itself.
template <typename Storage>
using cell_retention_t =
    std::conditional_t<cache::is_stable_storage_v<Storage> &&
                           !storage_retains<Storage>::value,
                       retention, no_retention>;

// The state of one registration, shared by the containers: the storage of its
// instance, and what is kept of the instance for its interfaces. Only a
// storage that keeps its instance keeps more of it.
template <typename Storage>
class binding_cell : private cell_retention_t<Storage> {
public:
  using storage_type = Storage;

  static constexpr bool keeps_conversions =
      cache::is_stable_storage_v<Storage> || storage_retains<Storage>::value;

  binding_cell() = default;

  template <typename Arg, typename... Args,
            std::enable_if_t<!std::is_same_v<std::decay_t<Arg>, binding_cell>,
                             int> = 0>
  explicit binding_cell(Arg &&arg, Args &&...args)
      : storage_(std::forward<Arg>(arg), std::forward<Args>(args)...) {}

  storage_type &storage() { return storage_; }

  // What is kept of the instance, of the types the binding converts it to
  // and composes of it.
  auto &retained() noexcept {
    if constexpr (storage_retains<Storage>::value) {
      return storage_.retained();
    } else {
      return static_cast<cell_retention_t<Storage> &>(*this);
    }
  }

  // Returns the cell to its unresolved state. The instance goes first, as
  // what it refers to of what is kept has to outlive it.
  void reset() noexcept {
    if constexpr (storage_can_reset<storage_type>::value) {
      storage_.reset();
    }
    static_cast<cell_retention_t<Storage> &>(*this).clear();
  }

private:
  storage_type storage_;
};

template <typename BindingModel>
using binding_cell_t = binding_cell<typename BindingModel::storage_type>;

// Whether the storage yields its instance by value rather than by reference.
template <typename Storage, typename Context, typename Owner>
inline constexpr bool materializes_value_source_v = [] {
  using source_result = decltype(std::declval<Storage &>().resolve(
      std::declval<construction_scope>(), std::declval<Context &>(),
      std::declval<Owner &>()));
  using source_type = std::remove_cv_t<std::remove_reference_t<source_result>>;
  return !std::is_reference_v<source_result> && !std::is_pointer_v<source_type>;
}();

// Whether the value a storage yields owns what an address request reaches
// apart from itself: an array a scope made.
template <typename Storage, typename Context, typename Owner>
inline constexpr bool owns_apart_v =
    is_made_array_v<remove_cvref_t<decltype(std::declval<Storage &>().resolve(
        std::declval<construction_scope>(), std::declval<Context &>(),
        std::declval<Owner &>()))>>;

// Whether a storage is resolved: one that keeps no instance always is.
template <typename Storage>
bool storage_resolved([[maybe_unused]] const Storage &storage) noexcept {
  if constexpr (storage_can_reset<Storage>::value) {
    return storage.is_resolved();
  } else {
    return true;
  }
}

// The materialization facts of a storage, in a context, through the
// container its factory resolves in; what it yields is known only of that.
template <typename Storage, typename Context, typename Owner = void>
constexpr materialization_facts materialization_facts_v() {
  using policy = scope_traits<typename Storage::tag_type>;
  constexpr bool guarded =
      materialization_guard_enabled_v<policy::guard, Storage, Context>;
  bool yields_value = false;
  bool owns_apart = false;
  if constexpr (!std::is_void_v<Owner>) {
    yields_value = materializes_value_source_v<Storage, Context, Owner>;
    owns_apart = owns_apart_v<Storage, Context, Owner>;
  }
  return {guarded && policy::guard == materialization_guard::always,
          guarded && policy::guard == materialization_guard::while_unresolved,
          policy::retains_unresolved_source,
          policy::consumes,
          yields_value,
          std::is_trivially_move_constructible_v<typename Storage::type>,
          owns_apart};
}

// The shape of a request of a type a source is materialized for.
template <typename Request> constexpr materialized_request materialized() {
  return {std::is_lvalue_reference_v<Request> || std::is_pointer_v<Request>,
          std::is_rvalue_reference_v<Request>};
}

template <> constexpr materialized_request materialized<void>() {
  return {false, false};
}

// The materialization of the source of a binding, which both containers run:
// the static container through the typed access of its binding, the runtime
// engine through the node of its binding. The access tells whether the
// binding is resolved, the leaf it stores, its cell, which the recursion guard
// names the binding by, and how the cell is reset: when a transaction rolls
// back (rollback, of the cell and of rollback_argument) and as the runtime is
// destroyed (reset). MayGuard and MayRetain are what the facts may decide, as
// the code compiles: a static binding that is never guarded or never retains
// has no code for it.
//
// It guards the binding against recursion, and an unresolved storage that
// retains its source in a transaction materializes it persistently: a failed
// transaction returns the storage to its unresolved state. The reset is
// registered once the cell is resolved, after what it depends on, so that a
// rollback, which runs the last registered first, resets the cell before
// what it refers to. A cell that fails as it resolves, or as its reset is
// registered, is reset at once. materialize materializes the source in a
inline void no_hook(void *) {}

// construction scope and applies to it what the caller does with it.
// materialize(construction, materialized) tells materialized() once the
// source exists, before it applies anything to it: a cell it retains is reset
// as a transaction rolls back after what is kept of it is undone.
template <bool MayGuard, bool MayRetain, typename Access, typename Context,
          typename Materialize>
auto run_materialization(const materialization_facts &facts,
                         const Access &access, construction_scope scope,
                         Context &context, Materialize &&materialize)
    -> decltype(materialize(scope, materialized_hook{})) {
  [[maybe_unused]] const bool resolved =
      (MayGuard || MayRetain) && access.resolved();
  [[maybe_unused]] auto guard = [&] {
    if constexpr (MayGuard) {
      return guard_wrapper(context, access.cell_address(), access.leaf(),
                           materialization_guarded(facts, resolved));
    } else {
      return no_scope();
    }
  }();
  if constexpr (MayRetain) {
    if (materialization_retains(facts, resolved, true)) {
      struct failure_reset {
        const Access &access;
        bool armed = true;
        ~failure_reset() {
          if (armed) {
            access.reset()(access.cell_address());
          }
        }
      } reset{access};
      using result_type = decltype(materialize(
          construction_scope(persistent_scope), materialized_hook{}));
      struct registration {
        Context &context;
        const Access &access;
        failure_reset &reset;
        bool done = false;
        void operator()() {
          if (!done) {
            done = true;
            context.defer_rollback(access.rollback(), access.cell_address(),
                                   access.rollback_argument());
            reset.armed = false;
            context.add_runtime_destructor(access.cell_address(),
                                           access.reset());
          }
        }
        static void call(void *self) { (*static_cast<registration *>(self))(); }
      } registered{context, access, reset};
      const materialized_hook hook{&registration::call, &registered};
      if constexpr (std::is_void_v<result_type>) {
        materialize(construction_scope(persistent_scope), hook);
        registered();
        return;
      } else {
        result_type result =
            materialize(construction_scope(persistent_scope), hook);
        registered();
        if constexpr (std::is_reference_v<result_type>) {
          return std::forward<result_type>(result);
        } else {
          return result;
        }
      }
    }
  }
  return materialize(scope, materialized_hook{&no_hook, nullptr});
}

// The typed access of a cell to its materialization, which the static
// container drives its binding by.
template <typename Cell> struct cell_access {
  Cell &cell;

  bool resolved() const noexcept { return storage_resolved(cell.storage()); }
  type_descriptor leaf() const noexcept {
    return described_type_v<leaf_type_t<typename Cell::storage_type::type>>;
  }
  void *cell_address() const noexcept { return std::addressof(cell); }

  static void reset_cell(void *self) noexcept {
    static_cast<Cell *>(self)->reset();
  }
  static void rollback_cell(void *self, void *) noexcept {
    static_cast<Cell *>(self)->reset();
  }
  auto reset() const noexcept { return &reset_cell; }
  auto rollback() const noexcept { return &rollback_cell; }
  void *rollback_argument() const noexcept { return nullptr; }
};

// One resolution of a binding: the cell it resolves, the dependencies its
// factory resolves through and the scope of the request. Both containers
// resolve their bindings through it; they differ only in how they select the
// resolution and in the dependencies they pass.
//
// Dependencies is what the factory of the binding receives as its container:
// a container reference, or a value that routes requests through the local
// bindings of a static registration first. In a runtime transaction the
// activation tracks what it leaves in the cell, so that a failed transaction
// rolls it back.
template <typename Cell, typename Dependencies,
          typename Scope = construction_scope>
struct binding_activation {
  using storage_type = typename Cell::storage_type;
  using owner_type = std::remove_reference_t<Dependencies>;
  using materialization_traits =
      scope_materialization<typename storage_type::tag_type>;

  template <typename Context> static constexpr materialization_facts facts() {
    return materialization_facts_v<storage_type, Context, owner_type>();
  }

  // Whether an address request is served from a value the storage yields,
  // which is placed in the context and lives only as long as the context: one
  // of what the value owns apart from itself is not, as only a plan that
  // takes it over gives it to the caller.
  template <typename Request, typename Context>
  static constexpr bool places_value =
      materialization_placement(facts<Context>(), materialized<Request>(),
                                is_runtime_context_v<Context>) ==
          source_placement::context_value &&
      !facts<Context>().owns_apart;

  Cell &cell;
  Dependencies dependencies;
  Scope scope;

  // Materializes the storage and passes its source to fn.
  //
  // An address request (an lvalue reference or a pointer) of a storage that
  // yields a value places the value in the context.
  template <typename Request, typename Context, typename Fn>
  decltype(auto) use_source(Context &context, Fn &&fn) {
    constexpr materialization_facts materialization = facts<Context>();
    constexpr bool may_guard =
        materialization.guards_always || materialization.guards_unresolved;
    if constexpr (is_runtime_context_v<Context>) {
      return run_materialization<may_guard, materialization.retains_unresolved>(
          materialization, cell_access<Cell>{cell}, construction_scope(scope),
          context,
          [&](construction_scope construction,
              const materialized_hook &materialized) -> decltype(auto) {
            return use_runtime_source(
                construction, context, [&](auto &&source) -> decltype(auto) {
                  materialized();
                  return std::forward<Fn>(fn)(
                      std::forward<decltype(source)>(source));
                });
          });
    } else if constexpr (!may_guard) {
      // A binding that is never guarded or retained materializes its source
      // where its placement puts it.
      return use_placed_source<Request>(context, std::forward<Fn>(fn));
    } else {
      // Without a transaction, the source is materialized in the scope of
      // the request, known as the code compiles where it is.
      return run_materialization<may_guard, false>(
          materialization, cell_access<Cell>{cell}, construction_scope(scope),
          context,
          [&](construction_scope, const materialized_hook &) -> decltype(auto) {
            return use_placed_source<Request>(context, std::forward<Fn>(fn));
          });
    }
  }

  // Materializes the source of a request where its placement puts it.
  template <typename Request, typename Context, typename Fn>
  decltype(auto) use_placed_source(Context &context, Fn &&fn) {
    auto &storage = cell.storage();
    constexpr materialization_facts materialization = facts<Context>();
    constexpr source_placement placement = materialization_placement(
        materialization, materialized<Request>(), false);
    if constexpr (placement == source_placement::context_value) {
      using source_type =
          std::remove_cv_t<std::remove_reference_t<decltype(storage.resolve(
              scope, context, dependencies))>>;
      if constexpr (refuses_caller_address(materialization,
                                           materialized<Request>())) {
        reject_escape<Request>(scope, detail::described_type_v<source_type>,
                               context);
      }
      auto &value = context.template construct_made<source_type>(
          scope, [&]() -> decltype(auto) {
            return storage.resolve(scope, context, dependencies);
          });
      auto source = lvalue_source<source_type>(value);
      return std::forward<Fn>(fn)(source);
    } else if constexpr (placement == source_placement::in_context) {
      // What an rvalue request refers to lives in the context: so does the
      // source it is converted from.
      auto &source = materialization_traits::materialize_context_source(
          scope, context, storage, dependencies);
      return std::forward<Fn>(fn)(std::move(source));
    } else {
      return std::forward<Fn>(fn)(materialization_traits::materialize_source(
          scope, context, storage, dependencies));
    }
  }

public:
  // A runtime context places a source it retains in the transaction, as it
  // does every source of a persistent request.
  template <typename Context, typename Fn, typename Construction>
  decltype(auto) use_runtime_source(Construction construction, Context &context,
                                    Fn &&fn) {
    auto &storage = cell.storage();
    if constexpr (materialization_traits::consumes) {
      const bool persistent = construction.is_persistent();
      auto &source = materialization_traits::materialize_context_source(
          construction, context, storage, dependencies);
      if (persistent) {
        return std::forward<Fn>(fn)(source);
      }
      return std::forward<Fn>(fn)(std::move(source));
    } else {
      return std::forward<Fn>(fn)(materialization_traits::materialize_source(
          construction, context, storage, dependencies));
    }
  }
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
