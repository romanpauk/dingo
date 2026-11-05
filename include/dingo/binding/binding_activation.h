//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/context/context_base.h>
#include <dingo/resolution/cache.h>
#include <dingo/resolution/conversion_cache.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>

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

template <typename BindingModel, bool StoredInterfaces, typename... Interfaces>
struct binding_cache_types_impl;

template <typename BindingModel, typename... Interfaces>
struct binding_cache_types_impl<BindingModel, false, Interfaces...> {
private:
  using storage_type = typename BindingModel::storage_type;

  template <typename Interface>
  using interface_cache_types = resolution_cache_types_t<
      typename binding_resolutions<typename annotated_traits<Interface>::type,
                                   storage_type>::type,
      storage_shape_t<storage_type>>;

public:
  using type =
      type_list_unique_t<type_list_cat_t<interface_cache_types<Interfaces>...>>;
};

template <typename BindingModel, typename... Interfaces>
struct binding_cache_types_impl<BindingModel, true, Interfaces...> {
  using type = type_list<>;
};

template <typename BindingModel, typename Interfaces>
struct binding_cache_types;

template <typename BindingModel>
inline constexpr bool binding_has_cacheless_conversion_shape_v = [] {
  using conversions = typename BindingModel::conversions_type;
  return conversions::is_stable &&
         std::is_same_v<typename conversions::value_types,
                        type_list<runtime_type>> &&
         std::is_same_v<typename conversions::lvalue_reference_types,
                        type_list<runtime_type &>> &&
         std::is_same_v<typename conversions::rvalue_reference_types,
                        type_list<>> &&
         std::is_same_v<typename conversions::pointer_types,
                        type_list<runtime_type *>>;
}();

template <typename BindingModel>
inline constexpr bool binding_uses_default_conversions_v = std::is_same_v<
    typename BindingModel::conversions_type,
    detail::conversions<typename BindingModel::storage_tag,
                        typename BindingModel::registered_storage_type,
                        runtime_type>>;

template <typename Request, typename BindingModel>
inline constexpr bool binding_has_cacheless_default_leaf_v =
    binding_uses_default_conversions_v<BindingModel> &&
    BindingModel::conversions_type::is_stable &&
    std::is_same_v<std::remove_cv_t<Request>,
                   std::remove_cv_t<typename BindingModel::stored_leaf_type>>;

template <typename BindingModel, typename... Interfaces>
struct binding_cache_types<BindingModel, type_list<Interfaces...>>
    // Exact stored requests and stable default leaf requests cannot retain an
    // adapted value, so enumerating every resolution route cannot add a slot.
    : binding_cache_types_impl<
          BindingModel,
          (binding_has_cacheless_conversion_shape_v<BindingModel> &&
           (stored_request_identity_v<
                typename annotated_traits<Interfaces>::type,
                typename BindingModel::storage_type> &&
            ...)) ||
              (binding_has_cacheless_default_leaf_v<
                   typename annotated_traits<Interfaces>::type, BindingModel> &&
               ...),
          Interfaces...> {};

template <typename BindingModel>
using binding_cache_types_t =
    typename binding_cache_types<BindingModel,
                                 typename BindingModel::interface_types>::type;

// Only stable storage caches conversions; the routes of other storage are not
// enumerated.
template <typename BindingModel,
          bool Stable =
              cache::is_stable_storage_v<typename BindingModel::storage_type>>
struct binding_cell_cache_types
    : binding_cache_types<BindingModel,
                          typename BindingModel::interface_types> {};

template <typename BindingModel>
struct binding_cell_cache_types<BindingModel, false> {
  using type = type_list<>;
};

template <typename Storage, typename = void>
struct storage_can_reset : std::false_type {};

template <typename Storage>
struct storage_can_reset<
    Storage,
    std::void_t<decltype(std::declval<Storage &>().reset()),
                decltype(std::declval<const Storage &>().is_resolved())>>
    : std::true_type {};

// The state of one registration, shared by the containers: the storage of its
// instance and the conversions of the instance it retains for its interfaces.
template <typename Storage, typename CacheTypes = type_list<>>
class binding_cell : private conversion_cache<CacheTypes> {
  using conversion_cache_type = conversion_cache<CacheTypes>;

public:
  using storage_type = Storage;

  static constexpr bool has_conversion_cache =
      type_list_size_v<CacheTypes> != 0;

  binding_cell() = default;

  template <typename Arg, typename... Args,
            std::enable_if_t<!std::is_same_v<std::decay_t<Arg>, binding_cell>,
                             int> = 0>
  explicit binding_cell(Arg &&arg, Args &&...args)
      : storage_(std::forward<Arg>(arg), std::forward<Args>(args)...) {}

  storage_type &storage() { return storage_; }

  // A cached conversion outlives the request; it is forgotten again when the
  // transaction that constructed it rolls back.
  template <typename T, typename Context, typename... Args>
  T &construct_conversion(construction_scope scope, Context &context,
                          Args &&...args) {
    if constexpr (has_conversion_cache) {
      return conversions().template construct<T>(context,
                                                 std::forward<Args>(args)...);
    } else {
      return context.template construct<T>(scope, std::forward<Args>(args)...);
    }
  }

  void reset_conversions() noexcept { conversions().reset(); }

  // Whether a failed transaction must also return the storage to its
  // unresolved state.
  bool resets_storage_on_failure() const {
    if constexpr (storage_can_reset<storage_type>::value) {
      return !storage_.is_resolved();
    } else {
      return false;
    }
  }

  void reset(bool reset_storage) noexcept {
    reset_conversions();
    if constexpr (storage_can_reset<storage_type>::value) {
      if (reset_storage) {
        storage_.reset();
      }
    } else {
      (void)reset_storage;
    }
  }

private:
  conversion_cache_type &conversions() { return *this; }

  storage_type storage_;
};

template <typename BindingModel>
using binding_cell_t =
    binding_cell<typename BindingModel::storage_type,
                 typename binding_cell_cache_types<BindingModel>::type>;

template <typename Storage, typename T, typename Context, typename Source,
          typename = void>
struct has_storage_resolve_conversion : std::false_type {};

template <typename Storage, typename T, typename Context, typename Source>
struct has_storage_resolve_conversion<
    Storage, T, Context, Source,
    std::void_t<
        decltype(std::declval<Storage &>().template resolve_conversion<T>(
            std::declval<Context &>(), std::declval<Source>()))>>
    : std::true_type {};

template <typename Traits, typename Context, typename Storage, typename Owner,
          typename = void>
struct has_context_materialize_source : std::false_type {};

template <typename Traits, typename Context, typename Storage, typename Owner>
struct has_context_materialize_source<
    Traits, Context, Storage, Owner,
    std::void_t<decltype(Traits::materialize_source_in_context(
        std::declval<construction_scope>(), std::declval<Context &>(),
        std::declval<Storage &>(), std::declval<Owner &>()))>>
    : std::true_type {};

// Whether the storage yields its instance by value rather than by reference.
template <typename Storage, typename Context, typename Owner>
inline constexpr bool materializes_value_source_v = [] {
  using source_result = decltype(std::declval<Storage &>().resolve(
      std::declval<construction_scope>(), std::declval<Context &>(),
      std::declval<Owner &>()));
  using source_type = std::remove_cv_t<std::remove_reference_t<source_result>>;
  return !std::is_reference_v<source_result> && !std::is_pointer_v<source_type>;
}();

template <typename Target, typename Context, typename T>
void *result_address(construction_scope scope, Context &context, T &&instance) {
  using instance_type = std::remove_reference_t<T>;

  if constexpr (std::is_pointer_v<instance_type>) {
    return const_cast<std::remove_cv_t<std::remove_pointer_t<instance_type>> *>(
        instance);
  } else if constexpr (std::is_reference_v<T>) {
    return const_cast<std::remove_cv_t<instance_type> *>(&instance);
  } else if constexpr (std::is_constructible_v<Target, T &&>) {
    return &context.template construct<Target>(scope,
                                               std::forward<T>(instance));
  } else {
    static_assert(std::is_constructible_v<Target, T &>,
                  "resolution result must construct the requested type");
    return &context.template construct<Target>(scope, instance);
  }
}

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
template <typename Cell, typename Dependencies> struct binding_activation {
  using storage_type = typename Cell::storage_type;
  using owner_type = std::remove_reference_t<Dependencies>;
  using materialization_traits =
      storage_materialization_traits<typename storage_type::tag_type,
                                     typename storage_type::type>;

  Cell &cell;
  Dependencies dependencies;
  construction_scope scope;

  template <typename T, typename Context, typename Source>
  decltype(auto) construct_conversion(construction_scope construction,
                                      Context &context, Source &&source) {
    return cell.template construct_conversion<T>(construction, context,
                                                 std::forward<Source>(source));
  }

  template <typename T, typename Context, typename Source>
  decltype(auto) retain_conversion(Context &context, Source &&source) {
    if constexpr (has_storage_resolve_conversion<storage_type, T, Context,
                                                 Source &&>::value) {
      return cell.storage().template resolve_conversion<T>(
          context, std::forward<Source>(source));
    } else {
      return construct_conversion<T>(scope, context,
                                     std::forward<Source>(source));
    }
  }

  // Materializes the storage and passes its source to fn.
  //
  // An address request (an lvalue reference or a pointer) of a storage that
  // yields a value places the value in the context.
  template <typename Request, typename Context, typename Fn>
  decltype(auto) with_source(Context &context, Fn &&fn) {
    auto &storage = cell.storage();
    [[maybe_unused]] auto guard = materialization_traits::template make_guard<
        leaf_type_t<typename storage_type::type>>(context, storage);
    if constexpr (is_runtime_context_v<Context>) {
      return with_transaction_source(context, std::forward<Fn>(fn));
    } else if constexpr ((std::is_lvalue_reference_v<Request> ||
                          std::is_pointer_v<Request>) &&
                         materializes_value_source_v<storage_type, Context,
                                                     owner_type>) {
      using source_type =
          std::remove_cv_t<std::remove_reference_t<decltype(storage.resolve(
              scope, context, dependencies))>>;
      auto &value = context.template construct<source_type>(
          scope, storage.resolve(scope, context, dependencies));
      auto source = lvalue_source<source_type>(value);
      return std::forward<Fn>(fn)(source);
    } else {
      return std::forward<Fn>(fn)(materialization_traits::materialize_source(
          scope, context, storage, dependencies));
    }
  }

  // Applies a resolution to a source.
  template <typename Resolution, typename Context, typename Source>
  decltype(auto) apply(Context &context, Source &&source,
                       type_descriptor requested_type) {
    return Resolution::operation::template apply<storage_type>(
        *this, context, std::forward<Source>(source), requested_type,
        describe_type<typename storage_type::type>());
  }

  // The address of the instance of a resolution, applied to a source.
  template <typename Resolution, typename Context, typename Source>
  resolved_address resolve_address(Context &context, Source &&source,
                                   type_descriptor requested_type) {
    auto &&instance = apply<Resolution>(context, std::forward<Source>(source),
                                        requested_type);
    using instance_type = decltype(instance);
    constexpr bool owns_value =
        !std::is_lvalue_reference_v<instance_type> &&
        !std::is_pointer_v<std::remove_reference_t<instance_type>>;
    return {result_address<typename Resolution::result_type>(
                scope, context, std::forward<instance_type>(instance)),
            owns_value ? resolved_address::access_kind::consume
                       : resolved_address::access_kind::borrow};
  }

  // The address of the instance of a resolution selected at compile time.
  // The address is deduced: a deduced return type is instantiated at the
  // call and does not add a level to the template depth of a dependency
  // chain.
  template <typename Request, typename Resolution, typename Context>
  decltype(auto) resolve_request_address(Context &context) {
    return with_source<Request>(context, [&](auto &&source) {
      return resolve_address<Resolution>(context,
                                         std::forward<decltype(source)>(source),
                                         describe_type<Request>());
    });
  }

  // Passes the instance of a resolution selected at compile time to fn.
  template <typename Request, typename Resolution, typename Context,
            typename Fn>
  decltype(auto) consume_request(Context &context, Fn &&fn) {
    return with_source<Request>(context, [&](auto &&source) -> decltype(auto) {
      auto &&instance =
          apply<Resolution>(context, std::forward<decltype(source)>(source),
                            describe_type<Request>());
      return consume_resolved_binding<Request>(
          std::forward<decltype(instance)>(instance), std::forward<Fn>(fn));
    });
  }

private:
  // A runtime context keeps a retained source in the transaction; the cell is
  // reset when the transaction fails or, once sealed, when its runtime is
  // destroyed.
  template <typename Context, typename Fn>
  decltype(auto) with_transaction_source(Context &context, Fn &&fn) {
    if constexpr (materialization_traits::can_retain_source) {
      if (materialization_traits::retains_source(cell.storage())) {
        context.on_rollback(cell.resets_storage_on_failure()
                                ? +[](void *self, void *) noexcept {
                                    static_cast<Cell *>(self)->reset(true);
                                  }
                                : +[](void *self, void *) noexcept {
                                    static_cast<Cell *>(self)->reset(false);
                                  },
                            std::addressof(cell));
        return invoke_then(
            [&]() -> decltype(auto) {
              return with_runtime_source(persistent_scope, context,
                                         std::forward<Fn>(fn));
            },
            [&] {
              context.add_runtime_destructor(
                  std::addressof(cell), +[](void *self) noexcept {
                    static_cast<Cell *>(self)->reset(true);
                  });
            });
      }
    }
    return with_runtime_source(scope, context, std::forward<Fn>(fn));
  }

  // Runs after once fn has succeeded and returns what fn returned.
  template <typename Fn, typename After>
  static std::invoke_result_t<Fn &> invoke_then(Fn &&fn, After &&after) {
    using result_type = std::invoke_result_t<Fn &>;
    if constexpr (std::is_void_v<result_type>) {
      fn();
      after();
    } else {
      result_type result = fn();
      after();
      return std::forward<result_type>(result);
    }
  }

  // A runtime context places a source it retains in the transaction, as it
  // does every source of a persistent request.
  template <typename Context, typename Fn>
  decltype(auto) with_runtime_source(construction_scope construction,
                                     Context &context, Fn &&fn) {
    auto &storage = cell.storage();
    const bool retains = materialization_traits::retains_source(storage);
    if constexpr (has_context_materialize_source<materialization_traits,
                                                 Context, storage_type,
                                                 owner_type>::value) {
      const bool persistent = retains || construction.is_persistent();
      auto &source = materialization_traits::materialize_source_in_context(
          persistent ? persistent_scope : construction, context, storage,
          dependencies);
      if (persistent) {
        return std::forward<Fn>(fn)(source);
      }
      return std::forward<Fn>(fn)(std::move(source));
    } else {
      return std::forward<Fn>(fn)(materialization_traits::materialize_source(
          retains ? persistent_scope : construction, context, storage,
          dependencies));
    }
  }
};

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
