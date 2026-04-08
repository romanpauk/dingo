//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/registration/annotated.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/type/complete_type.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <type_traits>

namespace dingo {
template <typename... Args> struct interfaces;

namespace detail {
template <typename Storage, typename TypeInterface, typename Type>
struct interface_registration_requirements {
    using interface_type = typename annotated_traits<TypeInterface>::type;
    using normalized_type = normalized_type_t<Type>;
    using normalized_interface_type = normalized_type_t<interface_type>;

    static constexpr bool is_alternative_type_interface =
        is_alternative_type_interface_compatible_v<normalized_type,
                                                   normalized_interface_type>;
    static constexpr bool is_reference_interface =
        std::is_reference_v<interface_type>;
    static constexpr bool is_void_interface =
        std::is_void_v<normalized_interface_type>;
    static constexpr bool is_function_interface =
        std::is_function_v<interface_type>;
    static constexpr bool registered_type_is_complete =
        !requires_complete_type_v<normalized_type> ||
        is_complete_v<normalized_type>;
    static constexpr bool interface_type_is_complete =
        !requires_complete_type_v<normalized_interface_type> ||
        is_complete_v<normalized_interface_type>;

    static constexpr bool array_shape_matches = [] {
        if constexpr (!detail::is_array_like_type_v<Type> ||
                      !std::is_array_v<interface_type>) {
            return true;
        } else {
            using exact_interface_type =
                detail::array_like_exact_interface_type_t<Type>;
            return std::is_same_v<std::remove_cv_t<interface_type>,
                                  std::remove_cv_t<exact_interface_type>> ||
                   (std::is_array_v<Type> && (std::rank_v<Type> > 1) &&
                    std::is_same_v<std::remove_cv_t<interface_type>,
                                   std::remove_cv_t<std::remove_extent_t<Type>>>);
        }
    }();

    static constexpr bool array_element_matches = [] {
        if constexpr (!detail::is_array_like_type_v<Type> ||
                      std::is_array_v<interface_type>) {
            return true;
        } else {
            return std::is_same_v<normalized_type, normalized_interface_type>;
        }
    }();

    static constexpr bool pointer_convertible =
        std::is_convertible_v<normalized_type*, normalized_interface_type*> ||
        is_alternative_type_interface;

    static constexpr bool storage_supported = [] {
        if constexpr (std::is_same_v<normalized_type,
                                     normalized_interface_type>) {
            return true;
        } else {
            return storage_interface_requirements_v<
                Storage, normalized_type, normalized_interface_type>;
        }
    }();

    static constexpr bool valid =
        !is_reference_interface && !is_void_interface &&
        !is_function_interface && registered_type_is_complete &&
        interface_type_is_complete && array_shape_matches &&
        array_element_matches && pointer_convertible && storage_supported;

    static constexpr void assert_valid() {
        static_assert(!is_reference_interface,
                      "interfaces must not contain reference types");
        static_assert(!is_void_interface,
                      "interfaces<void> is not a valid registration target");
        static_assert(!is_function_interface,
                      "interfaces must not contain function types");
        static_assert(registered_type_is_complete,
                      "registered types must be complete");
        static_assert(interface_type_is_complete,
                      "registered interfaces must be complete");
        if constexpr (detail::is_array_like_type_v<Type>) {
            if constexpr (std::is_array_v<interface_type>) {
                static_assert(
                    array_shape_matches,
                    "array registrations require matching array-shape interfaces");
            } else {
                static_assert(
                    array_element_matches,
                    "array registrations require matching element-type interfaces");
            }
        }

        if constexpr (registered_type_is_complete) {
            if constexpr ((!detail::is_array_like_type_v<Type>) ||
                          (std::is_array_v<interface_type> && array_shape_matches) ||
                          (!std::is_array_v<interface_type> && array_element_matches)) {
                static_assert(
                    pointer_convertible,
                    "registered type must be pointer-convertible to the interface");
                if constexpr (pointer_convertible &&
                              !std::is_same_v<normalized_type,
                                              normalized_interface_type>) {
                    static_assert(storage_supported, "storage requirements not met");
                }
            }
        }
    }
};

template <typename... Args>
inline constexpr bool has_explicit_void_interface_v =
    (std::is_same_v<std::decay_t<Args>, interfaces<void>> || ...);

template <typename Storage, typename TypeList, typename Type>
struct registration_requirements;

template <typename Storage, typename Type, typename... TypeInterfaces>
struct registration_requirements<Storage, type_list<TypeInterfaces...>, Type> {
    static constexpr bool valid =
        (interface_registration_requirements<Storage, TypeInterfaces,
                                             Type>::valid &&
         ...);

    static constexpr void assert_valid() {
        (interface_registration_requirements<Storage, TypeInterfaces,
                                             Type>::assert_valid(),
         ...);
    }
};
} // namespace detail
} // namespace dingo
