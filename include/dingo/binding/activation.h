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
#include <dingo/conversion/apply.h>
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

// Whether a storage makes a value for each request that a reference may
// refer to: one moved out of what it makes, or a wrapper it is moved into,
// rather than a copy of an element a handle shares.
template <typename Storage>
inline constexpr bool places_made_values_v =
    scope_materialization<typename Storage::tag_type>::consumes &&
    storage_form_traits<typename Storage::type>::form !=
        storage_form::shared_handle &&
    storage_form_traits<typename Storage::type>::form !=
        storage_form::shared_array_handle;

// Whether a storage hands over a pointer it makes for each request: to the
// object, or to the first element of an array.
template <typename Storage>
inline constexpr bool hands_over_made_pointers_v =
    scope_materialization<typename Storage::tag_type>::consumes &&
    (storage_form_traits<typename Storage::type>::form ==
         storage_form::pointer ||
     storage_form_traits<typename Storage::type>::form ==
         storage_form::bounded_array);

// Whether a pointer to Pointee the storage hands over is adopted for a
// reference to what it points to: one to an object, or one to the array it
// makes, not to the first element of the array.
template <typename Storage, typename Pointee>
inline constexpr bool adopts_made_pointer_v =
    hands_over_made_pointers_v<Storage> &&
    (storage_form_traits<typename Storage::type>::form ==
         storage_form::pointer ||
     std::is_array_v<Pointee>);

// Whether a value of what a pointer the storage hands over points to is
// moved out of it: one to an object, not to an array.
template <typename Storage>
inline constexpr bool takes_made_pointee_v =
    hands_over_made_pointers_v<Storage> &&
    storage_form_traits<typename Storage::type>::form == storage_form::pointer;

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
constexpr materialization_facts materialization_facts_of() {
  using policy = scope_traits<typename Storage::tag_type>;
  constexpr bool guarded =
      materialization_guard_enabled_v<policy::guard, Storage, Context>;
  bool yields_value = false;
  if constexpr (!std::is_void_v<Owner>) {
    yields_value = materializes_value_source_v<Storage, Context, Owner>;
  }
  return {guarded && policy::guard == materialization_guard::always,
          guarded && policy::guard == materialization_guard::while_unresolved,
          policy::retains_unresolved_source,
          policy::consumes,
          yields_value,
          std::is_trivially_move_constructible_v<typename Storage::type>};
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
// engine through the node of its binding. The access tells the storage of the
// binding, whether it is resolved, the leaf it stores, its cell, and how the
// cell is reset: when a transaction rolls back (rollback, of the cell and of
// rollback_argument) and as the runtime is destroyed (reset). MayGuard and
// MayRetain are what the facts may decide, as the code compiles: a static
// binding that is never guarded or never retains has no code for it.
//
// It guards the binding against recursion, and an unresolved storage that
// retains its source in a transaction materializes it persistently: a failed
// transaction returns the storage to its unresolved state. The reset is
// registered once the cell is resolved, after what it depends on, so that a
// rollback, which runs the last registered first, resets the cell before
// what it refers to. A cell that fails as it resolves, or as its reset is
// registered, is reset at once. materialize materializes the source in a
// construction scope and applies to it what the caller does with it.
template <bool MayGuard, bool MayRetain, typename Access, typename Context,
          typename Materialize>
auto drive_materialization(const materialization_facts &facts,
                           const Access &access, construction_scope scope,
                           Context &context, Materialize &&materialize)
    -> decltype(materialize(scope)) {
  [[maybe_unused]] const bool resolved =
      (MayGuard || MayRetain) && access.resolved();
  [[maybe_unused]] auto guard = [&] {
    if constexpr (MayGuard) {
      return recursion_guard_wrapper(context, access.storage(), access.leaf(),
                                     materialization_guarded(facts, resolved));
    } else {
      return no_materialization_scope();
    }
  }();
  if constexpr (MayRetain) {
    if (materialization_retains(facts, resolved, true)) {
      struct reset_on_failure {
        const Access &access;
        bool armed = true;
        ~reset_on_failure() {
          if (armed) {
            access.reset()(access.cell_address());
          }
        }
      } reset{access};
      using result_type =
          decltype(materialize(construction_scope(persistent_scope)));
      auto registered = [&] {
        context.on_rollback(access.rollback(), access.cell_address(),
                            access.rollback_argument());
        reset.armed = false;
        context.add_runtime_destructor(access.cell_address(), access.reset());
      };
      if constexpr (std::is_void_v<result_type>) {
        materialize(construction_scope(persistent_scope));
        registered();
        return;
      } else {
        result_type result = materialize(construction_scope(persistent_scope));
        registered();
        if constexpr (std::is_reference_v<result_type>) {
          return std::forward<result_type>(result);
        } else {
          return result;
        }
      }
    }
  }
  return materialize(scope);
}

// The typed access of a cell to its materialization, which the static
// container drives its binding by.
template <typename Cell> struct cell_access {
  Cell &cell;

  void *storage() const noexcept { return std::addressof(cell.storage()); }
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

// The application of the resolutions of a binding to the source its storage
// yields. It is apart from the activation, which also knows the container
// the factory resolves its dependencies through, so that a resolution is
// instantiated once for a storage, whatever container the factory resolves
// through: no conversion uses it.
template <typename Storage, typename Scope = construction_scope>
struct binding_application {
  using storage_type = Storage;

  // Whether the source of a request is placed in the context, as the
  // materialization of the storage places it.
  template <typename Request, typename Context>
  static constexpr bool places_source() {
    return materialization_placement(
               materialization_facts_of<storage_type, Context>(),
               materialized<Request>(),
               is_runtime_context_v<Context>) == source_placement::in_context;
  }

  Scope scope;

  // Applies a resolution to a source.
  template <typename Resolution, typename Context, typename Source>
  decltype(auto) apply(Context &context, Source &&source,
                       type_descriptor requested_type) {
    return Resolution::operation::apply(
        *this, context, std::forward<Source>(source), requested_type,
        detail::described_type_v<typename storage_type::type>);
  }

  // The address of the instance of a resolution of Request, applied to a
  // source, which places_source says whether the context holds.
  template <typename Resolution, typename Request = void, typename Context,
            typename Source>
  resolved_address resolve_address(Context &context, Source &&source,
                                   type_descriptor requested_type) {
    using target_type = typename Resolution::result_type;
    using applied_type = decltype(apply<Resolution>(
        context, std::forward<Source>(source), requested_type));
    if constexpr (!std::is_reference_v<applied_type> &&
                  !std::is_pointer_v<applied_type> &&
                  std::is_constructible_v<target_type, applied_type>) {
      // A value the resolution makes is made in the context.
      auto &value = context.template construct_from<target_type>(
          scope, [&]() -> target_type {
            return apply<Resolution>(context, std::forward<Source>(source),
                                     requested_type);
          });
      return {std::addressof(value), resolved_address::access_kind::consume};
    } else {
      decltype(auto) instance = apply<Resolution>(
          context, std::forward<Source>(source), requested_type);
      using instance_type = decltype(instance);
      using object_type = std::remove_reference_t<instance_type>;
      if constexpr (std::is_pointer_v<object_type>) {
        return {
            const_cast<std::remove_cv_t<std::remove_pointer_t<object_type>> *>(
                instance),
            resolved_address::access_kind::borrow};
      } else if constexpr (std::is_lvalue_reference_v<instance_type> ||
                           (std::is_rvalue_reference_v<instance_type> &&
                            places_source<Request, Context>())) {
        // What an lvalue reference refers to is borrowed. The context holds
        // the source a consuming scope places there, and what it is
        // converted to by rvalue reference lives there too: it is handed
        // over where it is.
        return {const_cast<std::remove_cv_t<object_type> *>(
                    std::addressof(instance)),
                std::is_rvalue_reference_v<instance_type>
                    ? resolved_address::access_kind::consume
                    : resolved_address::access_kind::borrow};
      } else if constexpr (std::is_constructible_v<target_type,
                                                   object_type &&>) {
        // A value, or what an rvalue reference refers to outside the
        // context, is moved into it.
        return {&context.template construct<target_type>(
                    scope, static_cast<object_type &&>(instance)),
                resolved_address::access_kind::consume};
      } else {
        static_assert(std::is_constructible_v<target_type, object_type &>,
                      "resolution result must construct the requested type");
        return {&context.template construct<target_type>(scope, instance),
                resolved_address::access_kind::consume};
      }
    }
  }
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
  using application_type = binding_application<storage_type, Scope>;

  template <typename Context> static constexpr materialization_facts facts() {
    return materialization_facts_of<storage_type, Context, owner_type>();
  }

  // Whether an address request is served from a value the storage yields,
  // which is placed in the context and lives only as long as the context.
  template <typename Request, typename Context>
  static constexpr bool places_value_in_context =
      materialization_placement(facts<Context>(), materialized<Request>(),
                                is_runtime_context_v<Context>) ==
      source_placement::value_in_context;

  Cell &cell;
  Dependencies dependencies;
  Scope scope;

  application_type application() const { return {scope}; }

  // Materializes the storage and passes its source to fn.
  //
  // An address request (an lvalue reference or a pointer) of a storage that
  // yields a value places the value in the context.
  template <typename Request, typename Context, typename Fn>
  decltype(auto) with_source(Context &context, Fn &&fn) {
    constexpr materialization_facts materialization = facts<Context>();
    constexpr bool may_guard =
        materialization.guards_always || materialization.guards_unresolved;
    if constexpr (is_runtime_context_v<Context>) {
      return drive_materialization<may_guard,
                                   materialization.retains_unresolved>(
          materialization, cell_access<Cell>{cell}, construction_scope(scope),
          context, [&](construction_scope construction) -> decltype(auto) {
            return with_runtime_source(construction, context,
                                       std::forward<Fn>(fn));
          });
    } else if constexpr (!may_guard) {
      // A binding that is never guarded or retained materializes its source
      // where its placement puts it.
      return with_placed_source<Request>(context, std::forward<Fn>(fn));
    } else {
      // Without a transaction, the source is materialized in the scope of
      // the request, known as the code compiles where it is.
      return drive_materialization<may_guard, false>(
          materialization, cell_access<Cell>{cell}, construction_scope(scope),
          context, [&](construction_scope) -> decltype(auto) {
            return with_placed_source<Request>(context, std::forward<Fn>(fn));
          });
    }
  }

  // Materializes the source of a request where its placement puts it.
  template <typename Request, typename Context, typename Fn>
  decltype(auto) with_placed_source(Context &context, Fn &&fn) {
    auto &storage = cell.storage();
    constexpr materialization_facts materialization = facts<Context>();
    constexpr source_placement placement = materialization_placement(
        materialization, materialized<Request>(), false);
    if constexpr (placement == source_placement::value_in_context) {
      using source_type =
          std::remove_cv_t<std::remove_reference_t<decltype(storage.resolve(
              scope, context, dependencies))>>;
      if constexpr (refuses_caller_address(materialization,
                                           materialized<Request>())) {
        refuse_caller_reference<Request>(
            scope, detail::described_type_v<source_type>, context);
      }
      auto &value = context.template construct_from<source_type>(
          scope, [&]() -> decltype(auto) {
            return storage.resolve(scope, context, dependencies);
          });
      auto source = lvalue_source<source_type>(value);
      return std::forward<Fn>(fn)(source);
    } else if constexpr (placement == source_placement::in_context) {
      // What an rvalue request refers to lives in the context: so does the
      // source it is converted from.
      auto &source = materialization_traits::materialize_source_in_context(
          scope, context, storage, dependencies);
      return std::forward<Fn>(fn)(std::move(source));
    } else {
      return std::forward<Fn>(fn)(materialization_traits::materialize_source(
          scope, context, storage, dependencies));
    }
  }

  // The address of the instance of a resolution selected at compile time.
  // The address is deduced: a deduced return type is instantiated at the
  // call and does not add a level to the template depth of a dependency
  // chain.
  template <typename Request, typename Resolution, typename Context>
  decltype(auto) resolve_request_address(Context &context) {
    return with_source<Request>(context, [&](auto &&source) {
      return application().template resolve_address<Resolution, Request>(
          context, std::forward<decltype(source)>(source),
          detail::described_type_v<Request>);
    });
  }

  // The value of a resolution selected at compile time, made where it is
  // requested: a value the resolution makes initializes it.
  template <typename Request, typename Resolution, typename Context>
  Request resolve_request_value(Context &context) {
    return with_source<Request>(context, [&](auto &&source) -> Request {
      using applied_type = decltype(application().template apply<Resolution>(
          context, std::forward<decltype(source)>(source),
          detail::described_type_v<Request>));
      if constexpr (!std::is_reference_v<applied_type>) {
        return Request(application().template apply<Resolution>(
            context, std::forward<decltype(source)>(source),
            detail::described_type_v<Request>));
      } else {
        auto applied = application();
        auto &&instance = applied.template apply<Resolution>(
            context, std::forward<decltype(source)>(source),
            detail::described_type_v<Request>);
        return consume_resolved_binding<Request>(
            std::forward<decltype(instance)>(instance),
            [](auto &&value) -> Request {
              return Request(std::forward<decltype(value)>(value));
            });
      }
    });
  }

  // Passes the instance of a resolution selected at compile time to fn.
  template <typename Request, typename Resolution, typename Context,
            typename Fn>
  decltype(auto) consume_request(Context &context, Fn &&fn) {
    return with_source<Request>(context, [&](auto &&source) -> decltype(auto) {
      auto applied = application();
      auto &&instance = applied.template apply<Resolution>(
          context, std::forward<decltype(source)>(source),
          detail::described_type_v<Request>);
      return consume_resolved_binding<Request>(
          std::forward<decltype(instance)>(instance), std::forward<Fn>(fn));
    });
  }

private:
public:
  // A runtime context places a source it retains in the transaction, as it
  // does every source of a persistent request.
  template <typename Context, typename Fn, typename Construction>
  decltype(auto) with_runtime_source(Construction construction,
                                     Context &context, Fn &&fn) {
    auto &storage = cell.storage();
    if constexpr (materialization_traits::consumes) {
      const bool persistent = construction.is_persistent();
      auto &source = materialization_traits::materialize_source_in_context(
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
