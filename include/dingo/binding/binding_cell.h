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
#include <dingo/resolution/cache.h>
#include <dingo/resolution/conversion_cache.h>
#include <dingo/type/annotated.h>

#include <type_traits>
#include <utility>

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

// The cache types are only enumerated for stable conversions; naming them in
// the primary template would instantiate them for every binding.
template <typename BindingModel,
          bool Stable =
              cache::is_stable_storage_v<typename BindingModel::storage_type>>
inline constexpr bool has_binding_conversion_cache_v = false;

template <typename BindingModel>
inline constexpr bool has_binding_conversion_cache_v<BindingModel, true> =
    type_list_size_v<binding_cache_types_t<BindingModel>> != 0;

template <typename BindingModel,
          bool Enabled = has_binding_conversion_cache_v<BindingModel>>
struct binding_cell_cache_types {
  using type = binding_cache_types_t<BindingModel>;
};

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

  // A cached conversion outlives the request. Tracked, it is forgotten again
  // when the transaction that constructed it rolls back.
  template <typename T, bool Tracked, typename Context, typename... Args>
  T &construct_conversion(construction_scope scope, Context &context,
                          Args &&...args) {
    if constexpr (!has_conversion_cache) {
      return context.template construct<T>(scope, std::forward<Args>(args)...);
    } else if constexpr (Tracked) {
      return conversions().template construct_tracked<T>(
          context, std::forward<Args>(args)...);
    } else {
      return conversions().template construct<T>(scope, context,
                                                 std::forward<Args>(args)...);
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

} // namespace detail
} // namespace dingo
