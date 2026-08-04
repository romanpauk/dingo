//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/resolution/recursion_guard.h>
#include <dingo/resolution/resolution.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
template <typename...> inline constexpr bool always_false_v = false;

namespace detail {
struct no_materialization_scope {
  no_materialization_scope() = default;

  template <typename... Args> explicit no_materialization_scope(Args &&...) {}
};
} // namespace detail

template <typename StorageTag, typename Type>
struct storage_materialization_traits {
  static constexpr bool can_retain_source = false;

  template <typename Leaf, typename Context, typename Storage>
  static auto make_guard(Context &, const Storage &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
  }

  template <typename Storage> static bool retains_source(const Storage &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
    return false;
  }

  template <typename Context, typename Storage, typename Container>
  static auto materialize_source(Context &, Storage &, Container &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
  }
};

template <typename StorageTag, typename Type, typename U, typename = void>
struct resolution_traits {
  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<>;
};

namespace detail {
template <typename AccessTraits, typename ResolutionTraits>
struct combined_storage_types {
  using value_types = type_list_cat_t<typename AccessTraits::value_types,
                                      typename ResolutionTraits::value_types>;
  using lvalue_reference_types =
      type_list_cat_t<typename AccessTraits::lvalue_reference_types,
                      typename ResolutionTraits::lvalue_reference_types>;
  using rvalue_reference_types =
      type_list_cat_t<typename AccessTraits::rvalue_reference_types,
                      typename ResolutionTraits::rvalue_reference_types>;
  using pointer_types =
      type_list_cat_t<typename AccessTraits::pointer_types,
                      typename ResolutionTraits::pointer_types>;
};

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Storage, typename = void> struct storage_source_type {
  using type = std::remove_reference_t<typename Storage::type>;
};

template <typename Storage>
struct storage_source_type<Storage,
                           std::void_t<typename Storage::resolved_type>> {
  using type = std::remove_reference_t<typename Storage::resolved_type>;
};

template <typename Storage>
using storage_source_type_t = typename storage_source_type<Storage>::type;

template <typename Storage> struct storage_borrow_source {
private:
  using source_type = storage_source_type_t<Storage>;

public:
  using type = std::conditional_t<std::is_pointer_v<source_type>, source_type,
                                  source_type &>;
};

template <typename Storage>
using storage_borrow_source_t = typename storage_borrow_source<Storage>::type;

template <typename Storage>
using storage_consume_source_t = storage_source_type_t<Storage> &&;

template <typename Storage> struct storage_source_traits {
private:
  using stored_value_type = remove_cvref_t<typename Storage::type>;
  using resolved_type = storage_source_type_t<Storage>;
  using resolved_leaf_type =
      std::conditional_t<std::is_pointer_v<resolved_type>,
                         std::remove_pointer_t<resolved_type>, resolved_type>;

public:
  using stored_type = stored_value_type;
  using direct_leaf_type =
      std::remove_cv_t<std::remove_pointer_t<stored_value_type>>;
  using borrowed_type = storage_borrow_source_t<Storage>;
  using consumed_type = storage_consume_source_t<Storage>;
  using qualification_type = resolved_leaf_type;

  static constexpr bool is_pointer = std::is_pointer_v<stored_value_type>;
  static constexpr bool is_const = std::is_const_v<resolved_leaf_type>;

  template <typename Interface>
  using interface_type = copy_cv_t<resolved_leaf_type, Interface>;

  template <typename Interface>
  static constexpr bool publishes_interface =
      !is_pointer ||
      std::is_convertible_v<stored_value_type, interface_type<Interface> *>;
};

template <typename Target, typename Qualification> struct qualified_target {
  using type = Target;
};

template <typename Target, typename Qualification>
struct qualified_target<Target &, Qualification> {
  using type = copy_cv_t<Qualification, Target> &;
};

template <typename Target, typename Qualification>
struct qualified_target<Target &&, Qualification> {
  using type = copy_cv_t<Qualification, Target> &&;
};

template <typename Target, typename Qualification>
struct qualified_target<Target *, Qualification> {
  using type = copy_cv_t<Qualification, Target> *;
};

template <typename Target, typename Qualification>
using qualified_target_t =
    typename qualified_target<Target, Qualification>::type;

template <typename Request>
using request_qualification_t = std::conditional_t<
    std::is_reference_v<Request>, std::remove_reference_t<Request>,
    std::conditional_t<std::is_pointer_v<remove_cvref_t<Request>>,
                       std::remove_pointer_t<remove_cvref_t<Request>>,
                       Request>>;

template <typename Request, typename Interface>
using request_target_t = qualified_target_t<resolved_type_t<Request, Interface>,
                                            request_qualification_t<Request>>;

template <typename Request, typename Leaf> struct rebind_request_leaf {
private:
  using outer = outer_traits<Request>;
  using rebound = wrapper_rebind_leaf_t<typename outer::type, Leaf>;
  using qualified =
      std::conditional_t<std::is_reference_v<Request> ||
                             std::is_pointer_v<remove_cvref_t<Request>>,
                         rebound, std::remove_cv_t<rebound>>;

public:
  using type = typename outer::template rebind_t<qualified>;
};

template <typename Request, typename Leaf>
using rebind_request_leaf_t = typename rebind_request_leaf<Request, Leaf>::type;

template <typename Request, typename Interface, typename Storage,
          typename Access, bool PublishValue>
struct storage_resolution {
private:
  using source = storage_source_traits<Storage>;
  static constexpr bool uses_stored_leaf =
      source::is_pointer && !source::template publishes_interface<Interface>;
  using target_leaf_type =
      std::conditional_t<uses_stored_leaf, typename source::direct_leaf_type,
                         std::remove_cv_t<Interface>>;
  using unqualified_published_type =
      std::conditional_t<source::is_pointer,
                         rebind_request_leaf_t<Request, target_leaf_type>,
                         request_target_t<Request, target_leaf_type>>;
  using published_type =
      qualified_target_t<unqualified_published_type,
                         typename source::qualification_type>;

public:
  using target_type =
      std::conditional_t<PublishValue, remove_cvref_t<published_type>,
                         published_type>;
  using conversion_source_type =
      std::conditional_t<std::is_same_v<Access, borrow>,
                         typename source::borrowed_type,
                         typename source::consumed_type>;
  using type =
      conversion_resolution<target_type, conversion_source_type, Access>;
};

template <typename Requests, typename Interface, typename Storage,
          typename Access, bool PublishValue>
struct storage_resolutions;

template <typename Interface, typename Storage, typename Access,
          bool PublishValue>
struct storage_resolutions<type_list<>, Interface, Storage, Access,
                           PublishValue> {
  using type = type_list<>;
};

template <typename Interface, typename Storage, typename Access,
          bool PublishValue, typename Request>
struct storage_resolutions<type_list<Request>, Interface, Storage, Access,
                           PublishValue> {
private:
  using candidate =
      storage_resolution<Request, Interface, Storage, Access, PublishValue>;

public:
  using type = std::conditional_t<
      is_type_conversion_available_v<
          conversion_target_t<typename candidate::type::target_type>,
          typename candidate::conversion_source_type, Access>,
      type_list<typename candidate::type>, type_list<>>;
};

template <typename Interface, typename Storage, typename Access,
          bool PublishValue, typename First, typename Second,
          typename... Requests>
struct storage_resolutions<type_list<First, Second, Requests...>, Interface,
                           Storage, Access, PublishValue> {
private:
  template <typename Request>
  using candidate =
      storage_resolution<Request, Interface, Storage, Access, PublishValue>;

  template <typename Request>
  using selected = std::conditional_t<
      is_type_conversion_available_v<
          conversion_target_t<typename candidate<Request>::type::target_type>,
          typename candidate<Request>::conversion_source_type, Access>,
      type_list<typename candidate<Request>::type>, type_list<>>;

public:
  using type =
      type_list_unique_t<type_list_cat_t<selected<First>, selected<Second>,
                                         selected<Requests>...>>;
};

template <typename Requests, typename Interface, typename Storage,
          typename Access, bool PublishValue = false>
using storage_resolutions_t =
    typename storage_resolutions<Requests, Interface, Storage, Access,
                                 PublishValue>::type;

// Avoid probing conversions for empty storage categories.
template <bool Enabled, typename Target, typename ConversionTarget,
          typename Source, typename Access>
struct interface_resolution_if {
  using type = type_list<>;
};

template <typename Target, typename ConversionTarget, typename Source,
          typename Access>
struct interface_resolution_if<true, Target, ConversionTarget, Source, Access> {
  using type = std::conditional_t<
      is_type_conversion_available_v<ConversionTarget, Source, Access>,
      type_list<conversion_resolution<Target, Source, Access>>, type_list<>>;
};

template <bool Enabled, typename Source, typename Interface, typename Access>
struct wrapper_resolution_if {
  using type = type_list<>;
};

template <typename Source, typename Interface, typename Access>
struct wrapper_resolution_if<true, Source, Interface, Access>
    : wrapper_resolution_traits<Source, Interface, Access> {};

template <typename Interface, typename Storage> struct interface_resolutions {
private:
  using conversions = typename Storage::conversions;
  using source = storage_source_traits<Storage>;
  using interface_type = std::remove_cv_t<Interface>;
  using borrowed_interface_type =
      typename source::template interface_type<interface_type>;
  static constexpr bool interface_is_resolvable =
      type_traits<interface_type>::enabled &&
      !std::is_pointer_v<interface_type>;

  template <bool Enabled, typename Target, typename SourceType, typename Access>
  using if_convertible =
      typename interface_resolution_if<Enabled && interface_is_resolvable,
                                       Target, Target, SourceType,
                                       Access>::type;

  using borrowed_interface_value =
      if_convertible<type_list_size_v<typename conversions::value_types> != 0,
                     interface_type, typename source::borrowed_type, borrow>;
  using consumed_interface_value = if_convertible<
      type_list_size_v<typename conversions::rvalue_reference_types> != 0,
      interface_type, typename source::consumed_type, consume>;
  using consumed_interface_rvalue = typename interface_resolution_if<
      interface_is_resolvable &&
          type_list_size_v<typename conversions::rvalue_reference_types> != 0,
      interface_type &&, interface_type, typename source::consumed_type,
      consume>::type;
  using borrowed_interface_reference = if_convertible<
      type_list_size_v<typename conversions::lvalue_reference_types> != 0,
      borrowed_interface_type &, typename source::borrowed_type, borrow>;
  using borrowed_interface_pointer =
      if_convertible<type_list_size_v<typename conversions::pointer_types> != 0,
                     borrowed_interface_type *, typename source::borrowed_type,
                     borrow>;

public:
  using value_resolutions =
      type_list_cat_t<borrowed_interface_value, consumed_interface_value>;
  using lvalue_reference_resolutions = borrowed_interface_reference;
  using rvalue_reference_resolutions = consumed_interface_rvalue;
  using pointer_resolutions = borrowed_interface_pointer;
};

template <typename Interface, typename Storage> struct wrapper_resolutions {
private:
  using conversions = typename Storage::conversions;
  using source = storage_source_traits<Storage>;

  using borrowed_composition = typename wrapper_resolution_if<
      type_list_size_v<typename conversions::value_types> != 0,
      typename source::borrowed_type, Interface, borrow>::type;
  using consumed_composition = typename wrapper_resolution_if<
      type_list_size_v<typename conversions::rvalue_reference_types> != 0,
      typename source::consumed_type, Interface, consume>::type;

public:
  using value_resolutions =
      type_list_cat_t<borrowed_composition, consumed_composition>;
};

template <typename Interface, typename Storage> struct binding_resolutions {
private:
  using conversions = typename Storage::conversions;
  using interface_values =
      typename interface_resolutions<Interface, Storage>::value_resolutions;
  using wrapper_values =
      typename wrapper_resolutions<Interface, Storage>::value_resolutions;
  using stored_values = storage_resolutions_t<typename conversions::value_types,
                                              Interface, Storage, borrow>;
  using consumed_values =
      storage_resolutions_t<typename conversions::rvalue_reference_types,
                            Interface, Storage, consume, true>;
  using converted_values = type_list_merge_t<interface_values, wrapper_values>;
  using published_values = type_list_merge_t<stored_values, consumed_values>;

public:
  using value_resolutions =
      type_list_merge_t<converted_values, published_values>;
  using lvalue_reference_resolutions = type_list_merge_t<
      typename interface_resolutions<Interface,
                                     Storage>::lvalue_reference_resolutions,
      storage_resolutions_t<typename conversions::lvalue_reference_types,
                            Interface, Storage, borrow>>;
  using rvalue_reference_resolutions = type_list_merge_t<
      typename interface_resolutions<Interface,
                                     Storage>::rvalue_reference_resolutions,
      storage_resolutions_t<typename conversions::rvalue_reference_types,
                            Interface, Storage, consume>>;
  using pointer_resolutions = type_list_merge_t<
      typename interface_resolutions<Interface, Storage>::pointer_resolutions,
      storage_resolutions_t<typename conversions::pointer_types, Interface,
                            Storage, borrow>>;
  // Target forms are disjoint across the value, reference, and pointer
  // categories, so concatenating their already-unique lists cannot duplicate
  // a resolution.
  using type =
      type_list_cat_t<value_resolutions, lvalue_reference_resolutions,
                      rvalue_reference_resolutions, pointer_resolutions>;
};

} // namespace detail

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename StorageTag, typename Type, typename U>
struct type_storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<storage_traits<StorageTag, Type, U>::enabled>>
    : detail::combined_storage_types<storage_traits<StorageTag, Type, U>,
                                     resolution_traits<StorageTag, Type, U>> {
public:
  static constexpr bool is_stable =
      storage_traits<StorageTag, Type, U>::is_stable;
};
} // namespace dingo
