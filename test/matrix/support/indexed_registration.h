//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/indexed_containers.h"
#include "matrix/support/values.h"

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

namespace dingo::matrix {

template <typename Container> struct indexed_key {
    using type = std::tuple_element_t<
        0, std::tuple_element_t<0, typename std::remove_reference_t<
                                       Container>::index_definition_type>>;
};

template <typename Container>
using indexed_key_t = typename indexed_key<Container>::type;

template <typename T> T indexed_value(int value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return std::to_string(value);
    } else {
        return static_cast<T>(value);
    }
}

template <typename Container>
void exercise_indexed_registration(Container& container) {
    using key_type = indexed_key_t<Container>;

    container.template register_indexed_type<
        dingo::scope<dingo::unique>,
        dingo::storage<std::unique_ptr<element_type<0>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(0));
    container.template register_indexed_type<
        dingo::scope<dingo::unique>,
        dingo::storage<std::unique_ptr<element_type<1>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(1));
    ASSERT_THROW(
        (container.template register_indexed_type<
            dingo::scope<dingo::unique>,
            dingo::storage<std::unique_ptr<element_type<1>>>,
            dingo::interfaces<element_interface>>(indexed_value<key_type>(1))),
        type_already_registered_exception);

    ASSERT_EQ(container
                  .template resolve<std::shared_ptr<element_interface>>(
                      indexed_value<key_type>(0))
                  ->id(),
              0);
    ASSERT_EQ(container
                  .template resolve<std::shared_ptr<element_interface>>(
                      indexed_value<key_type>(1))
                  ->id(),
              1);
    ASSERT_THROW(
        (container.template resolve<std::shared_ptr<element_interface>>(
            indexed_value<key_type>(-1))),
        type_not_found_exception);

    container.template register_indexed_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<element_type<2>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(2));
    container.template register_indexed_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<element_type<3>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(3));
    ASSERT_THROW(
        (container.template register_indexed_type<
            dingo::scope<dingo::shared>,
            dingo::storage<std::shared_ptr<element_type<3>>>,
            dingo::interfaces<element_interface>>(indexed_value<key_type>(3))),
        type_already_registered_exception);

    ASSERT_EQ(
        container
            .template resolve<element_interface&>(indexed_value<key_type>(2))
            .id(),
        2);
    ASSERT_EQ(
        container
            .template resolve<element_interface&>(indexed_value<key_type>(3))
            .id(),
        3);
    ASSERT_THROW((container.template resolve<element_interface&>(
                     indexed_value<key_type>(-2))),
                 type_not_found_exception);
}

} // namespace dingo::matrix
