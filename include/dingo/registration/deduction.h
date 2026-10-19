//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/registration/options.h>
#include <dingo/type/rebind.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
struct unique;

namespace detail {
template <typename Type, typename = void> struct deduced_interface_types {
  using type = type_list<std::remove_cv_t<std::remove_reference_t<Type>>>;
};

template <typename Type>
struct deduced_interface_types<
    Type,
    std::enable_if_t<
        type_traits<std::remove_cv_t<std::remove_reference_t<Type>>>::enabled &&
        type_traits<std::remove_cv_t<std::remove_reference_t<Type>>>::
            is_value_borrowable>> {
private:
  using value_type = std::remove_cv_t<std::remove_reference_t<Type>>;

public:
  using type =
      type_list_cat_t<type_list<value_type>,
                      typename deduced_interface_types<
                          typename type_traits<value_type>::value_type>::type>;
};

template <typename Type>
struct deduced_interface_types<
    Type, std::enable_if_t<is_alternative_type_v<
              std::remove_cv_t<std::remove_reference_t<Type>>>>> {
private:
  using value_type = std::remove_cv_t<std::remove_reference_t<Type>>;

  template <typename Alternatives> struct alternatives_interface_types;

  template <typename... Alternatives>
  struct alternatives_interface_types<type_list<Alternatives...>> {
    using type = type_list_cat_t<
        typename deduced_interface_types<Alternatives>::type...>;
  };

public:
  using type = type_list_unique_t<
      type_list_cat_t<type_list<value_type>,
                      typename alternatives_interface_types<
                          alternative_type_alternatives_t<value_type>>::type>>;
};

template <typename StorageType, typename ScopeType, typename = void>
struct deduced_interface_type {
  using type = ::dingo::interfaces<std::remove_cv_t<leaf_type_t<StorageType>>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::void_t<typename ::dingo::detail::alternative_type_interface_types<
        std::remove_cv_t<std::remove_reference_t<StorageType>>>::type>> {
  using type =
      ::dingo::interfaces<typename ::dingo::detail::deduced_interface_types<
          std::remove_cv_t<std::remove_reference_t<StorageType>>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<
        !std::is_same_v<typename ScopeType::type, unique> &&
        type_traits<
            std::remove_cv_t<std::remove_reference_t<StorageType>>>::enabled &&
        type_traits<std::remove_cv_t<std::remove_reference_t<StorageType>>>::
            is_value_borrowable &&
        is_alternative_type_v<std::remove_cv_t<leaf_type_t<StorageType>>>>> {
  using type =
      ::dingo::interfaces<typename ::dingo::detail::deduced_interface_types<
          std::remove_cv_t<leaf_type_t<StorageType>>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<
        !std::is_same_v<typename ScopeType::type, unique> &&
        type_traits<StorageType>::enabled && !std::is_pointer_v<StorageType> &&
        std::is_array_v<typename type_traits<StorageType>::value_type>>> {
  using type =
      ::dingo::interfaces<typename type_traits<StorageType>::value_type,
                          leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<std::is_array_v<StorageType> &&
                     (std::rank_v<StorageType> > 1)>> {
  using type = ::dingo::interfaces<std::remove_extent_t<StorageType>,
                                   StorageType, leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<std::is_array_v<StorageType> &&
                     (std::rank_v<StorageType> == 1)>> {
  using type = ::dingo::interfaces<StorageType, leaf_type_t<StorageType>>;
};
} // namespace detail
} // namespace dingo
