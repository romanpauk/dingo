//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/rebind_type.h>
#include <dingo/type_list.h>
#include <dingo/wrapper_traits.h>

namespace dingo {
namespace detail {

template <typename StorageType, typename Interface>
using storage_rebind_t = wrapper_rebind_t<StorageType, Interface>;

template <typename StorageType, typename Interface, typename = void>
struct unique_optional_result_types {
    using type = type_list<>;
};

template <typename StorageType, typename Interface>
struct unique_optional_result_types<
    StorageType, Interface,
    std::enable_if_t<
        has_wrapper_optional_rebind_v<StorageType, Interface> &&
        !std::is_same_v<wrapper_optional_rebind_t<StorageType, Interface>,
                        storage_rebind_t<StorageType, Interface>>>> {
    using type = type_list<wrapper_optional_rebind_t<StorageType, Interface>>;
};

template <typename StorageType, typename Interface>
using unique_optional_result_types_t =
    typename unique_optional_result_types<StorageType, Interface>::type;

template <typename StorageType, typename Interface, typename = void>
struct unique_optional_rvalue_types {
    using type = type_list<>;
};

template <typename StorageType, typename Interface>
struct unique_optional_rvalue_types<
    StorageType, Interface,
    std::enable_if_t<
        has_wrapper_optional_rebind_v<StorageType, Interface> &&
        !std::is_same_v<wrapper_optional_rebind_t<StorageType, Interface>,
                        storage_rebind_t<StorageType, Interface>>>> {
    using type = type_list<wrapper_optional_rebind_t<StorageType, Interface>&&>;
};

template <typename StorageType, typename Interface>
using unique_optional_rvalue_types_t =
    typename unique_optional_rvalue_types<StorageType, Interface>::type;

template <typename StorageType, typename Interface>
inline constexpr bool is_raw_pointer_registration_v =
    !std::is_reference_v<StorageType> &&
    std::is_pointer_v<wrapper_base_t<StorageType>>;

template <typename StorageType, typename Interface>
inline constexpr bool is_plain_value_registration_v =
    !std::is_reference_v<StorageType> &&
    !wrapper_traits<wrapper_base_t<StorageType>>::is_indirect &&
    has_wrapper_rebind_v<StorageType, Interface> &&
    std::is_same_v<storage_rebind_t<StorageType, Interface>, Interface>;

template <typename StorageType, typename Interface>
inline constexpr bool is_wrapper_registration_v =
    !std::is_reference_v<StorageType> &&
    !is_plain_value_registration_v<StorageType, Interface> &&
    !is_raw_pointer_registration_v<StorageType, Interface>;

template <typename StorageType, typename Interface>
inline constexpr bool has_copyable_wrapper_rebind_v =
    is_wrapper_registration_v<StorageType, Interface> &&
    has_wrapper_rebind_v<StorageType, Interface> &&
    std::is_copy_constructible_v<storage_rebind_t<StorageType, Interface>>;

template <typename StorageType, typename Interface>
using unique_owned_result_t = std::conditional_t<
    is_raw_pointer_registration_v<StorageType, Interface> &&
        has_wrapper_owned_rebind_v<StorageType, Interface>,
    wrapper_owned_rebind_t<StorageType, Interface>,
    storage_rebind_t<StorageType, Interface>>;

template <typename StorageType, typename Interface>
inline constexpr bool can_convert_unique_result_to_shared_v =
    has_wrapper_shared_rebind_v<StorageType, Interface> &&
    (std::is_constructible_v<wrapper_shared_rebind_t<StorageType, Interface>,
                             unique_owned_result_t<StorageType, Interface>> ||
     can_adopt_released_wrapper_v<unique_owned_result_t<StorageType, Interface>,
                                  wrapper_shared_rebind_t<StorageType,
                                                          Interface>>);

template <typename StorageType, typename Interface, typename = void>
struct unique_shared_result_types {
    using type = type_list<>;
};

template <typename StorageType, typename Interface>
struct unique_shared_result_types<
    StorageType, Interface,
    std::enable_if_t<
        can_convert_unique_result_to_shared_v<StorageType, Interface> &&
        !std::is_same_v<wrapper_shared_rebind_t<StorageType, Interface>,
                        unique_owned_result_t<StorageType, Interface>>>> {
    using type = type_list<wrapper_shared_rebind_t<StorageType, Interface>>;
};

template <typename StorageType, typename Interface>
using unique_shared_result_types_t =
    typename unique_shared_result_types<StorageType, Interface>::type;

template <typename StorageType, typename Interface, typename = void>
struct unique_shared_rvalue_types {
    using type = type_list<>;
};

template <typename StorageType, typename Interface>
struct unique_shared_rvalue_types<
    StorageType, Interface,
    std::enable_if_t<
        can_convert_unique_result_to_shared_v<StorageType, Interface> &&
        !std::is_same_v<wrapper_shared_rebind_t<StorageType, Interface>,
                        unique_owned_result_t<StorageType, Interface>>>> {
    using type = type_list<wrapper_shared_rebind_t<StorageType, Interface>&&>;
};

template <typename StorageType, typename Interface>
using unique_shared_rvalue_types_t =
    typename unique_shared_rvalue_types<StorageType, Interface>::type;

template <typename StorageType, typename Interface>
using unique_result_value_types_t =
    type_list_cat_t<type_list<unique_owned_result_t<StorageType, Interface>>,
                    unique_shared_result_types_t<StorageType, Interface>>;

template <typename StorageType, typename Interface>
using unique_result_rvalue_types_t =
    type_list_cat_t<type_list<unique_owned_result_t<StorageType, Interface>&&>,
                    unique_shared_rvalue_types_t<StorageType, Interface>>;

} // namespace detail
} // namespace dingo
