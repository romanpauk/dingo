//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/type/exact_lookup.h>
#include <dingo/type/primary.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename Type, typename Pointer, typename = void>
struct has_type_from_pointer : std::false_type {};

template <typename Type, typename Pointer>
struct has_type_from_pointer<
    Type, Pointer,
    std::void_t<decltype(type_traits<Type>::from_pointer(
        std::declval<Pointer>()))>> : std::true_type {};

template <typename Type, typename Pointer>
inline constexpr bool has_type_from_pointer_v =
    has_type_from_pointer<Type, Pointer>::value;

template <typename Type, typename U, typename = void>
struct wrapper_rebind_leaf {
private:
  using const_type =
      std::conditional_t<std::is_const_v<Type>, std::add_const_t<U>, U>;

public:
  using type = std::conditional_t<std::is_volatile_v<Type>,
                                  std::add_volatile_t<const_type>, const_type>;
};

template <typename Type, size_t N, typename U>
struct wrapper_rebind_leaf<Type[N], U, void> {
  using type = typename wrapper_rebind_leaf<Type, U>::type[N];
};

template <typename Type, typename U>
struct wrapper_rebind_leaf<Type[], U, void> {
  using type = typename wrapper_rebind_leaf<Type, U>::type[];
};

template <typename Type, typename U>
struct wrapper_rebind_leaf<
    Type, U,
    std::enable_if_t<type_traits<Type>::enabled && !std::is_pointer_v<Type>>> {
  using type = typename type_traits<Type>::template rebind_t<
      typename wrapper_rebind_leaf<typename type_traits<Type>::value_type,
                                   U>::type>;
};

template <typename Type, typename U>
using wrapper_rebind_leaf_t = typename wrapper_rebind_leaf<Type, U>::type;

template <typename Handle, typename T, typename U, typename = void>
struct smart_array_pointer_types {
  using type = type_list<U *, Handle *>;
};

template <typename Handle, typename T, typename U>
struct smart_array_pointer_types<Handle, T, U,
                                 std::enable_if_t<std::is_array_v<T>>> {
  using type =
      type_list<exact_lookup<typename wrapper_rebind_leaf<T, U>::type> *,
                Handle *>;
};

template <typename Handle, typename = void> struct wrapper_storage_types_impl {
  using lvalue_reference_types = type_list<Handle &>;
  using pointer_types = type_list<Handle *>;
  using copyable_value_types =
      std::conditional_t<std::is_copy_constructible_v<Handle>,
                         type_list<Handle>, type_list<>>;
};

template <typename Handle>
struct wrapper_storage_types_impl<
    Handle, std::enable_if_t<type_traits<Handle>::enabled &&
                             type_traits<Handle>::is_pointer_like>> {
private:
  // Walk the wrapper chain once and collect all derived interface types
  // together instead of recursing over the same chain separately for each
  // result list.
  using next =
      wrapper_storage_types_impl<typename type_traits<Handle>::value_type>;
  using copyable_handle_types =
      std::conditional_t<std::is_copy_constructible_v<Handle>,
                         type_list<Handle>, type_list<>>;

public:
  using lvalue_reference_types =
      type_list_cat_t<type_list<Handle &>,
                      typename next::lvalue_reference_types>;
  using pointer_types =
      type_list_cat_t<type_list<Handle *>, typename next::pointer_types>;
  using copyable_value_types =
      type_list_cat_t<copyable_handle_types,
                      typename next::copyable_value_types>;
};

template <typename Handle>
using wrapper_storage_types = wrapper_storage_types_impl<Handle>;
} // namespace detail
} // namespace dingo
