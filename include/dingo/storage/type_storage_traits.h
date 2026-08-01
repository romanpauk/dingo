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

template <typename Left, typename Right> struct merge_unique_resolution_lists {
  using type = type_list_unique_t<type_list_cat_t<Left, Right>>;
};

template <typename... Types>
struct merge_unique_resolution_lists<type_list<>, type_list<Types...>> {
  using type = type_list<Types...>;
};

template <typename... Types>
struct merge_unique_resolution_lists<type_list<Types...>, type_list<>> {
  using type = type_list<Types...>;
};

template <> struct merge_unique_resolution_lists<type_list<>, type_list<>> {
  using type = type_list<>;
};

template <typename Left, typename Right>
using merge_unique_resolution_lists_t =
    typename merge_unique_resolution_lists<Left, Right>::type;

// Factory and storage implementation details do not affect resolution. Keep
// them out of the template identity so equivalent registrations share probes.
template <typename Type, typename ResolvedType, typename Conversions>
struct resolution_storage {
  using type = Type;
  using resolved_type = ResolvedType;
  using conversions = Conversions;
};

template <typename Storage>
using resolution_storage_t =
    resolution_storage<typename Storage::type, storage_source_type_t<Storage>,
                       typename Storage::conversions>;

template <typename Interface, typename Storage>
struct exact_binding_resolutions {
private:
  using canonical_storage = resolution_storage_t<Storage>;
  using conversions = typename canonical_storage::conversions;
  using interface_values =
      typename interface_resolutions<Interface,
                                     canonical_storage>::value_resolutions;
  using wrapper_values =
      typename wrapper_resolutions<Interface,
                                   canonical_storage>::value_resolutions;
  using stored_values =
      storage_resolutions_t<typename conversions::value_types, Interface,
                            canonical_storage, borrow>;
  using consumed_values =
      storage_resolutions_t<typename conversions::rvalue_reference_types,
                            Interface, canonical_storage, consume, true>;
  using converted_values =
      merge_unique_resolution_lists_t<interface_values, wrapper_values>;
  using published_values =
      merge_unique_resolution_lists_t<stored_values, consumed_values>;

public:
  using value_resolutions =
      merge_unique_resolution_lists_t<converted_values, published_values>;
  using lvalue_reference_resolutions = merge_unique_resolution_lists_t<
      typename interface_resolutions<
          Interface, canonical_storage>::lvalue_reference_resolutions,
      storage_resolutions_t<typename conversions::lvalue_reference_types,
                            Interface, canonical_storage, borrow>>;
  using rvalue_reference_resolutions = merge_unique_resolution_lists_t<
      typename interface_resolutions<
          Interface, canonical_storage>::rvalue_reference_resolutions,
      storage_resolutions_t<typename conversions::rvalue_reference_types,
                            Interface, canonical_storage, consume>>;
  using pointer_resolutions = merge_unique_resolution_lists_t<
      typename interface_resolutions<Interface,
                                     canonical_storage>::pointer_resolutions,
      storage_resolutions_t<typename conversions::pointer_types, Interface,
                            canonical_storage, borrow>>;
  // Each category has already removed its own duplicates, and its target
  // forms are disjoint (value, lvalue, rvalue, and pointer respectively).
  using type =
      type_list_cat_t<value_resolutions, lvalue_reference_resolutions,
                      rvalue_reference_resolutions, pointer_resolutions>;
};

template <typename Types, typename Leaf> struct rebind_resolution_types;

template <typename Leaf, typename... Types>
struct rebind_resolution_types<type_list<Types...>, Leaf> {
  using type = type_list<rebind_leaf_t<Types, Leaf>...>;
};

template <typename Types, typename Leaf>
using rebind_resolution_types_t =
    typename rebind_resolution_types<Types, Leaf>::type;

// The normalized members, rather than the original Storage specialization,
// form the type identity so equivalent user-defined wrappers share this work.
template <typename Type, typename ResolvedType, typename ValueTypes,
          typename LvalueReferenceTypes, typename RvalueReferenceTypes,
          typename PointerTypes>
struct binding_resolution_shape_storage {
  struct conversions {
    using value_types = ValueTypes;
    using lvalue_reference_types = LvalueReferenceTypes;
    using rvalue_reference_types = RvalueReferenceTypes;
    using pointer_types = PointerTypes;
  };

  using type = Type;
  using resolved_type = ResolvedType;
};

template <typename Storage>
using binding_resolution_shape_storage_t = binding_resolution_shape_storage<
    rebind_leaf_t<typename Storage::type, runtime_type>,
    rebind_leaf_t<typename Storage::resolved_type, runtime_type>,
    rebind_resolution_types_t<typename Storage::conversions::value_types,
                              runtime_type>,
    rebind_resolution_types_t<
        typename Storage::conversions::lvalue_reference_types, runtime_type>,
    rebind_resolution_types_t<
        typename Storage::conversions::rvalue_reference_types, runtime_type>,
    rebind_resolution_types_t<typename Storage::conversions::pointer_types,
                              runtime_type>>;

template <typename Recipe, typename Target, typename Source>
struct materialize_resolution_recipe {
  static constexpr bool supported = false;
  using type = unavailable_type_conversion;
};

// Rebuild the structural recipe with the actual leaf while keeping route
// selection shared by every binding with the same storage shape.
template <typename ShapeTarget, typename ShapeSource, typename Target,
          typename Source>
struct materialize_resolution_recipe<
    identity_type_conversion<ShapeTarget, ShapeSource>, Target, Source> {
  using type = identity_type_conversion<Target, Source>;

  static constexpr bool supported = true;
};

template <typename ShapeTarget, typename ShapeSource, typename RequiredAccess,
          typename Argument, typename Target, typename Source>
struct materialize_resolution_recipe<
    traits_type_conversion<ShapeTarget, ShapeSource, RequiredAccess, Argument>,
    Target, Source> {
  using type = traits_type_conversion<Target, Source, RequiredAccess, Source>;

  static constexpr bool supported = std::is_same_v<Argument, ShapeSource>;
};

template <typename ShapeTarget, typename ShapeSource, typename Target,
          typename Source>
struct materialize_resolution_recipe<
    address_type_conversion<ShapeTarget, ShapeSource>, Target, Source> {
  using type = address_type_conversion<Target, Source>;

  static constexpr bool supported = true;
};

template <typename ShapeTarget, typename ShapeSource, typename Target,
          typename Source>
struct materialize_resolution_recipe<
    pointer_access_type_conversion<ShapeTarget, ShapeSource>, Target, Source> {
  using type = pointer_access_type_conversion<Target, Source>;

  static constexpr bool supported = true;
};

template <typename ShapeTarget, typename ShapeSource, typename Next,
          typename Target, typename Source>
struct materialize_resolution_recipe<
    borrowed_type_conversion<ShapeTarget, ShapeSource, Next>, Target, Source> {
private:
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using borrowed_source =
      decltype(type_traits<source_type>::borrow(std::declval<Source>()));
  using next = materialize_resolution_recipe<Next, Target, borrowed_source>;

public:
  static constexpr bool supported = next::supported;
  using type = borrowed_type_conversion<Target, Source, typename next::type>;
};

template <typename ShapeResolution, typename Interface>
struct materialize_binding_resolution;

template <typename ShapeTarget, typename ShapeConversionTarget,
          typename ShapeSource, typename Recipe, typename Interface>
struct materialize_binding_resolution<
    resolution<ShapeTarget,
               type_resolution<ShapeConversionTarget, ShapeSource, Recipe>>,
    Interface> {
  using target = rebind_leaf_t<ShapeTarget, Interface>;
  using conversion_target = rebind_leaf_t<ShapeConversionTarget, Interface>;
  using source = rebind_leaf_t<ShapeSource, Interface>;
  using conversion =
      materialize_resolution_recipe<Recipe, conversion_target, source>;

  static constexpr bool supported = conversion::supported;
  using type = resolution<target, type_resolution<conversion_target, source,
                                                  typename conversion::type>>;
};

template <typename Resolutions, typename Interface>
struct materialize_binding_resolution_list;

template <typename Interface, typename... Resolutions>
struct materialize_binding_resolution_list<type_list<Resolutions...>,
                                           Interface> {
  static constexpr bool supported =
      (materialize_binding_resolution<Resolutions, Interface>::supported &&
       ...);
  using type =
      type_list_unique_t<type_list<typename materialize_binding_resolution<
          Resolutions, Interface>::type...>>;
};

template <typename Interface, typename Storage>
struct normalized_binding_resolutions {
private:
  using shape_storage = binding_resolution_shape_storage_t<Storage>;
  using shape = exact_binding_resolutions<runtime_type, shape_storage>;

  using values =
      materialize_binding_resolution_list<typename shape::value_resolutions,
                                          Interface>;
  using lvalue_references = materialize_binding_resolution_list<
      typename shape::lvalue_reference_resolutions, Interface>;
  using rvalue_references = materialize_binding_resolution_list<
      typename shape::rvalue_reference_resolutions, Interface>;
  using pointers =
      materialize_binding_resolution_list<typename shape::pointer_resolutions,
                                          Interface>;

public:
  static constexpr bool supported =
      values::supported && lvalue_references::supported &&
      rvalue_references::supported && pointers::supported;
  using value_resolutions = typename values::type;
  using lvalue_reference_resolutions = typename lvalue_references::type;
  using rvalue_reference_resolutions = typename rvalue_references::type;
  using pointer_resolutions = typename pointers::type;
  using type = type_list_unique_t<
      type_list_cat_t<value_resolutions, lvalue_reference_resolutions,
                      rvalue_reference_resolutions, pointer_resolutions>>;
};

template <typename Interface, typename Storage, typename = void>
struct binding_resolution_shape_eligible : std::false_type {};

// Reject unsupported shapes before normalized_binding_resolutions is named;
// otherwise uncommon storage forms pay for both the normalized and exact path.
template <typename Interface, typename Storage>
struct binding_resolution_shape_eligible<
    Interface, Storage,
    std::void_t<typename Storage::type, typename Storage::resolved_type,
                typename Storage::conversions,
                typename type_traits<std::remove_cv_t<std::remove_reference_t<
                    typename Storage::resolved_type>>>::value_type>>
    : std::bool_constant<
          std::is_lvalue_reference_v<typename Storage::resolved_type> &&
          type_traits<std::remove_cv_t<std::remove_reference_t<
              typename Storage::resolved_type>>>::is_pointer_like &&
          type_traits<std::remove_cv_t<std::remove_reference_t<
              typename Storage::resolved_type>>>::is_value_borrowable &&
          std::is_same_v<
              typename type_traits<std::remove_cv_t<std::remove_reference_t<
                  typename Storage::resolved_type>>>::value_type,
              Interface> &&
          !type_traits<Interface>::enabled && !std::is_array_v<Interface> &&
          std::is_same_v<Interface, std::remove_cv_t<Interface>> &&
          is_copy_constructible_v<Interface> &&
          type_list_size_v<
              typename Storage::conversions::rvalue_reference_types> == 0> {};

template <typename Interface, typename Storage,
          bool Eligible =
              binding_resolution_shape_eligible<Interface, Storage>::value>
struct can_normalize_binding_resolutions : std::false_type {};

template <typename Interface, typename Storage>
struct can_normalize_binding_resolutions<Interface, Storage, true>
    : std::bool_constant<
          normalized_binding_resolutions<Interface, Storage>::supported> {};

template <typename Interface, typename Storage,
          bool Normalize =
              can_normalize_binding_resolutions<Interface, Storage>::value>
struct binding_resolutions : exact_binding_resolutions<Interface, Storage> {};

template <typename Interface, typename Storage>
struct binding_resolutions<Interface, Storage, true>
    : normalized_binding_resolutions<Interface, Storage> {};

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
