//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/resolution/recursion_guard.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
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
  using copy_value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<>;
  using conversion_types = type_list<>;
};

namespace detail {
template <typename Traits, typename = void> struct copy_value_types {
  using type = type_list<>;
};

template <typename Traits>
struct copy_value_types<Traits,
                        std::void_t<typename Traits::copy_value_types>> {
  using type = typename Traits::copy_value_types;
};

template <typename AccessTraits, typename ResolutionTraits>
struct combined_storage_types {
  using value_types = type_list_cat_t<typename AccessTraits::value_types,
                                      typename ResolutionTraits::value_types>;
  using copy_value_types =
      type_list_cat_t<typename copy_value_types<AccessTraits>::type,
                      typename copy_value_types<ResolutionTraits>::type>;
  using lvalue_reference_types =
      type_list_cat_t<typename AccessTraits::lvalue_reference_types,
                      typename ResolutionTraits::lvalue_reference_types>;
  using rvalue_reference_types =
      type_list_cat_t<typename AccessTraits::rvalue_reference_types,
                      typename ResolutionTraits::rvalue_reference_types>;
  using pointer_types =
      type_list_cat_t<typename AccessTraits::pointer_types,
                      typename ResolutionTraits::pointer_types>;
  using conversion_types =
      type_list_cat_t<typename AccessTraits::conversion_types,
                      typename ResolutionTraits::conversion_types>;
};

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Capability, typename ExactLeaf>
struct exact_capability_type {
private:
  using outer = outer_traits<Capability>;
  static constexpr bool is_runtime_interface =
      std::is_same_v<typename outer::type, runtime_interface>;
  using rebound = wrapper_rebind_leaf_t<typename outer::type, ExactLeaf>;
  using exact_type =
      std::conditional_t<std::is_reference_v<Capability> ||
                             std::is_pointer_v<remove_cvref_t<Capability>>,
                         rebound, std::remove_cv_t<rebound>>;

public:
  using type = std::conditional_t<
      is_runtime_interface, Capability,
      typename outer::template rebind_t<exact_lookup<exact_type>>>;
};

template <typename Types, typename ExactLeaf> struct exact_capability_types;

template <typename ExactLeaf, typename... Types>
struct exact_capability_types<type_list<Types...>, ExactLeaf> {
  using type =
      type_list<typename exact_capability_type<Types, ExactLeaf>::type...>;
};

template <typename Types, typename ExactLeaf>
using exact_capability_types_t =
    typename exact_capability_types<Types, ExactLeaf>::type;

template <typename Capability, typename Interface>
struct runtime_interface_capability {
private:
  using outer = outer_traits<Capability>;
  static constexpr bool is_runtime_interface =
      std::is_same_v<typename outer::type, runtime_interface>;
  static constexpr bool supports_exact_interface =
      type_traits<Interface>::enabled && !std::is_pointer_v<Interface>;
  using resolved =
      typename outer::template rebind_t<std::remove_cv_t<Interface>>;

public:
  using type =
      std::conditional_t<is_runtime_interface,
                         std::conditional_t<supports_exact_interface,
                                            type_list<resolved>, type_list<>>,
                         type_list<Capability>>;
};

template <typename Types, typename Interface>
struct runtime_interface_capabilities;

template <typename Interface, typename... Types>
struct runtime_interface_capabilities<type_list<Types...>, Interface> {
  using type = type_list_cat_t<
      typename runtime_interface_capability<Types, Interface>::type...>;
};

template <typename Types, typename Interface>
using runtime_interface_capabilities_t =
    typename runtime_interface_capabilities<Types, Interface>::type;

template <typename Types, typename Interface> struct copyable_capabilities;

template <typename Interface, typename... Types>
struct copyable_capabilities<type_list<Types...>, Interface> {
  template <typename Capability>
  using copyable_type = std::conditional_t<
      is_copy_constructible_v<resolved_type_t<Capability, Interface>>,
      type_list<Capability>, type_list<>>;

  using type = type_list_cat_t<copyable_type<Types>...>;
};

template <typename Types, typename Interface>
using copyable_capabilities_t =
    typename copyable_capabilities<Types, Interface>::type;

template <bool PublishesLeafCapabilities, typename Types, typename ExactLeaf>
struct storage_capability_types {
  using type = exact_capability_types_t<Types, ExactLeaf>;
};

template <typename Types, typename ExactLeaf>
struct storage_capability_types<true, Types, ExactLeaf> {
  using type = Types;
};

template <typename Interface, typename Storage>
struct binding_capability_types {
private:
  using conversions = typename Storage::conversions;
  using stored_type = remove_cvref_t<typename Storage::type>;
  using stored_pointee = std::remove_pointer_t<stored_type>;
  using generic_pointer_type = resolved_type_t<runtime_type *, Interface>;

  static constexpr bool has_pointer_source = std::is_pointer_v<stored_type>;
  static constexpr bool publishes_leaf_capabilities =
      !has_pointer_source ||
      std::is_convertible_v<stored_type, generic_pointer_type>;

  template <typename Types>
  using storage_capabilities =
      typename storage_capability_types<publishes_leaf_capabilities, Types,
                                        stored_pointee>::type;

  template <typename Types>
  using declared_capabilities =
      runtime_interface_capabilities_t<storage_capabilities<Types>, Interface>;

  using exact_lvalue_reference_types =
      std::conditional_t<conversions::is_stable &&
                             type_traits<Interface>::enabled &&
                             !std::is_pointer_v<Interface>,
                         type_list<Interface &>, type_list<>>;
  using exact_pointer_types =
      std::conditional_t<conversions::is_stable &&
                             type_traits<Interface>::enabled &&
                             !std::is_pointer_v<Interface>,
                         type_list<Interface *>, type_list<>>;
  using copy_value_types = declared_capabilities<
      typename detail::copy_value_types<conversions>::type>;

public:
  using value_types =
      type_list_cat_t<declared_capabilities<typename conversions::value_types>,
                      copyable_capabilities_t<copy_value_types, Interface>>;
  using lvalue_reference_types = type_list_cat_t<
      exact_lvalue_reference_types,
      declared_capabilities<typename conversions::lvalue_reference_types>>;
  using rvalue_reference_types =
      declared_capabilities<typename conversions::rvalue_reference_types>;
  using pointer_types = type_list_cat_t<
      exact_pointer_types,
      declared_capabilities<typename conversions::pointer_types>>;
};

template <typename T, typename List> struct has_distinct_conversion_type;

template <typename T>
struct has_distinct_conversion_type<T, type_list<>> : std::false_type {};

template <typename T, typename Head, typename... Tail>
struct has_distinct_conversion_type<T, type_list<Head, Tail...>>
    : std::bool_constant<
          !std::is_same_v<T, remove_cvref_t<Head>> ||
          has_distinct_conversion_type<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr bool has_distinct_conversion_type_v =
    has_distinct_conversion_type<T, List>::value;

template <typename T, typename List> struct max_distinct_conversion_size;

template <typename T>
struct max_distinct_conversion_size<T, type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename T, typename Head, typename... Tail>
struct max_distinct_conversion_size<T, type_list<Head, Tail...>>
    : std::integral_constant<
          std::size_t,
          !std::is_same_v<T, remove_cvref_t<Head>>
              ? (sizeof(remove_cvref_t<Head>) >
                         max_distinct_conversion_size<T,
                                                      type_list<Tail...>>::value
                     ? sizeof(remove_cvref_t<Head>)
                     : max_distinct_conversion_size<T,
                                                    type_list<Tail...>>::value)
              : max_distinct_conversion_size<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr std::size_t max_distinct_conversion_size_v =
    max_distinct_conversion_size<T, List>::value;

template <typename T, typename List> struct max_distinct_conversion_align;

template <typename T>
struct max_distinct_conversion_align<T, type_list<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename T, typename Head, typename... Tail>
struct max_distinct_conversion_align<T, type_list<Head, Tail...>>
    : std::integral_constant<
          std::size_t,
          !std::is_same_v<T, remove_cvref_t<Head>>
              ? (alignof(remove_cvref_t<Head>) >
                         max_distinct_conversion_align<
                             T, type_list<Tail...>>::value
                     ? alignof(remove_cvref_t<Head>)
                     : max_distinct_conversion_align<T,
                                                     type_list<Tail...>>::value)
              : max_distinct_conversion_align<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr std::size_t max_distinct_conversion_align_v =
    max_distinct_conversion_align<T, List>::value;

template <typename T, typename List>
struct has_nontrivial_distinct_conversion_destructor;

template <typename T>
struct has_nontrivial_distinct_conversion_destructor<T, type_list<>>
    : std::false_type {};

template <typename T, typename Head, typename... Tail>
struct has_nontrivial_distinct_conversion_destructor<T,
                                                     type_list<Head, Tail...>>
    : std::bool_constant<
          (!std::is_same_v<T, remove_cvref_t<Head>> &&
           !std::is_trivially_destructible_v<remove_cvref_t<Head>>) ||
          has_nontrivial_distinct_conversion_destructor<
              T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr bool has_nontrivial_distinct_conversion_destructor_v =
    has_nontrivial_distinct_conversion_destructor<T, List>::value;

template <typename Storage, typename = void>
struct static_conversion_temporary_slots_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_temporary_slots_base<
    Storage, std::void_t<decltype(Storage::static_conversion_temporary_slots)>>
    : std::integral_constant<std::size_t,
                             Storage::static_conversion_temporary_slots> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_size_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_temporary_size_base<
    Storage, std::void_t<decltype(Storage::static_conversion_temporary_size)>>
    : std::integral_constant<std::size_t,
                             Storage::static_conversion_temporary_size> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_align_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_temporary_align_base<
    Storage, std::void_t<decltype(Storage::static_conversion_temporary_align)>>
    : std::integral_constant<std::size_t,
                             Storage::static_conversion_temporary_align> {};

template <typename Storage, typename = void>
struct static_conversion_destructible_slots_base
    : std::integral_constant<std::size_t, 0> {};

template <typename Storage>
struct static_conversion_destructible_slots_base<
    Storage,
    std::void_t<decltype(Storage::static_conversion_destructible_slots)>>
    : std::integral_constant<std::size_t,
                             Storage::static_conversion_destructible_slots> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_slots
    : static_conversion_temporary_slots_base<Storage> {};

template <typename Storage>
struct static_conversion_temporary_slots<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_temporary_slots_base<typename Storage::conversions> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_size
    : static_conversion_temporary_size_base<Storage> {};

template <typename Storage>
struct static_conversion_temporary_size<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_temporary_size_base<typename Storage::conversions> {};

template <typename Storage, typename = void>
struct static_conversion_temporary_align
    : static_conversion_temporary_align_base<Storage> {};

template <typename Storage>
struct static_conversion_temporary_align<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_temporary_align_base<typename Storage::conversions> {};

template <typename Storage, typename = void>
struct static_conversion_destructible_slots
    : static_conversion_destructible_slots_base<Storage> {};

template <typename Storage>
struct static_conversion_destructible_slots<
    Storage, std::void_t<typename Storage::conversions>>
    : static_conversion_destructible_slots_base<typename Storage::conversions> {
};
} // namespace detail

template <typename Storage>
inline constexpr std::size_t static_conversion_temporary_slots_v =
    detail::static_conversion_temporary_slots<Storage>::value;

template <typename Storage>
inline constexpr std::size_t static_conversion_temporary_size_v =
    detail::static_conversion_temporary_size<Storage>::value;

template <typename Storage>
inline constexpr std::size_t static_conversion_temporary_align_v =
    detail::static_conversion_temporary_align<Storage>::value;

template <typename Storage>
inline constexpr std::size_t static_conversion_destructible_slots_v =
    detail::static_conversion_destructible_slots<Storage>::value;

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename StorageTag, typename Type, typename U>
struct type_storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<storage_traits<StorageTag, Type, U>::enabled>>
    : detail::combined_storage_types<storage_traits<StorageTag, Type, U>,
                                     resolution_traits<StorageTag, Type, U>> {
private:
  using combined_types =
      detail::combined_storage_types<storage_traits<StorageTag, Type, U>,
                                     resolution_traits<StorageTag, Type, U>>;

public:
  static constexpr bool is_stable =
      storage_traits<StorageTag, Type, U>::is_stable;
  static constexpr std::size_t static_conversion_temporary_slots =
      detail::has_distinct_conversion_type_v<
          Type, typename combined_types::conversion_types> &&
              !is_stable
          ? 1
          : 0;
  static constexpr std::size_t static_conversion_temporary_size =
      !is_stable ? detail::max_distinct_conversion_size_v<
                       Type, typename combined_types::conversion_types>
                 : 0;
  static constexpr std::size_t static_conversion_temporary_align =
      !is_stable ? detail::max_distinct_conversion_align_v<
                       Type, typename combined_types::conversion_types>
                 : 0;
  static constexpr std::size_t static_conversion_destructible_slots =
      !is_stable && detail::has_nontrivial_distinct_conversion_destructor_v<
                        Type, typename combined_types::conversion_types>
          ? 1
          : 0;
};
} // namespace dingo
