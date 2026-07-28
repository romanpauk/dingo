//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/type_list.h>

#include <type_traits>

namespace dingo {
struct borrow {};
struct consume {};

namespace detail {
template <typename Target, typename Source, typename Conversion>
struct type_resolution;

template <typename Operation, typename Storage>
using operation_cache_types_t =
    typename Operation::template cache_types<Storage>;

template <typename Operation, typename Storage>
using operation_temporary_types_t =
    typename Operation::template temporary_types<Storage>;

template <typename Operation, typename Storage>
inline constexpr bool operation_requires_source_retention_v =
    Operation::template requires_source_retention<Storage>;

template <typename Type> struct cv_types {
private:
  using value_type = std::remove_cv_t<Type>;

public:
  using type = type_list_unique_t<
      type_list<value_type, std::add_const_t<value_type>,
                std::add_volatile_t<value_type>, std::add_cv_t<value_type>>>;
};

template <typename Type> struct qualification_types : cv_types<Type> {};

template <typename Types> struct pointer_qualification_types;

template <typename... Types>
struct pointer_qualification_types<type_list<Types...>> {
  using type =
      type_list_cat_t<typename cv_types<std::add_pointer_t<Types>>::type...>;
};

template <typename Type> struct qualification_types<Type *> {
private:
  using pointee_types = typename qualification_types<Type>::type;

public:
  using type = type_list_unique_t<
      typename pointer_qualification_types<pointee_types>::type>;
};

template <typename Type>
struct qualification_types<Type *const> : qualification_types<Type *> {};

template <typename Type>
struct qualification_types<Type *volatile> : qualification_types<Type *> {};

template <typename Type>
struct qualification_types<Type *const volatile> : qualification_types<Type *> {
};

template <typename Reference, typename Types> struct reference_types;

template <typename Reference, typename... Types>
struct reference_types<Reference, type_list<Types...>> {
  using type =
      std::conditional_t<std::is_lvalue_reference_v<Reference>,
                         type_list<std::add_lvalue_reference_t<Types>...>,
                         type_list<std::add_rvalue_reference_t<Types>...>>;
};

template <typename Source, typename Types> struct convertible_types;

template <typename Source, typename... Types>
struct convertible_types<Source, type_list<Types...>> {
  using type =
      type_list_cat_t<std::conditional_t<std::is_convertible_v<Source, Types>,
                                         type_list<Types>, type_list<>>...>;
};

template <typename Target, typename = void> struct request_types {
  using type = type_list<Target>;
};

template <typename Target>
struct request_types<Target, std::enable_if_t<!std::is_reference_v<Target> &&
                                              !std::is_pointer_v<Target>>> {
private:
  using value_type = std::remove_cv_t<Target>;

public:
  using type =
      type_list<value_type, std::add_const_t<value_type>,
                std::add_volatile_t<value_type>, std::add_cv_t<value_type>>;
};

template <typename Target>
struct request_types<Target, std::enable_if_t<std::is_reference_v<Target>>> {
private:
  using value_type = std::remove_reference_t<Target>;
  using candidates = typename reference_types<
      Target, typename qualification_types<value_type>::type>::type;

public:
  using type = typename convertible_types<Target, candidates>::type;
};

template <typename Target>
struct request_types<Target, std::enable_if_t<std::is_pointer_v<Target>>> {
private:
  using candidates = typename qualification_types<Target>::type;

public:
  using type = typename convertible_types<Target, candidates>::type;
};
} // namespace detail

template <typename Target, typename Operation> struct resolution {
  using target_type = Target;
  using result_type = std::remove_reference_t<Target>;
  using operation = Operation;
  using request_types = typename detail::request_types<Target>::type;
};

namespace detail {
template <typename Resolutions, typename Storage> struct resolution_cache_types;

template <typename Storage, typename... Resolutions>
struct resolution_cache_types<type_list<Resolutions...>, Storage> {
  using type = type_list_unique_t<type_list_cat_t<
      operation_cache_types_t<typename Resolutions::operation, Storage>...>>;
};

template <typename Resolutions, typename Storage>
using resolution_cache_types_t =
    typename resolution_cache_types<Resolutions, Storage>::type;

template <typename Resolutions, typename Storage>
struct resolution_temporary_types;

template <typename Storage, typename... Resolutions>
struct resolution_temporary_types<type_list<Resolutions...>, Storage> {
  using type = type_list_unique_t<type_list_cat_t<operation_temporary_types_t<
      typename Resolutions::operation, Storage>...>>;
};

template <typename Resolutions, typename Storage>
using resolution_temporary_types_t =
    typename resolution_temporary_types<Resolutions, Storage>::type;
} // namespace detail
} // namespace dingo
