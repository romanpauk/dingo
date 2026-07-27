//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/assertions.h"
#include "support/resolution_test_types.h"

#include <dingo/container.h>
#include <dingo/core/exceptions.h>
#include <dingo/factory/function.h>
#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <variant>

namespace dingo::matrix {

struct conversion_qualification_regression_scenario {
  template <typename Container> static void run(Container &container) {
    const int const_value = 11;
    container.template register_type<dingo::scope<dingo::external>,
                                     dingo::storage<const int &>>(const_value);

    ASSERT_EQ(&container.template resolve<const int &>(), &const_value);
    ASSERT_EQ(&container.template resolve<const volatile int &>(),
              &const_value);
    ASSERT_EQ(container.template resolve<const int *>(), &const_value);
    ASSERT_EQ(container.template resolve<const volatile int *>(), &const_value);
    ASSERT_THROW((void)container.template resolve<int &>(),
                 dingo::type_not_convertible_exception);
    ASSERT_THROW(
        (void)std::addressof(container.template resolve<volatile int &>()),
        dingo::type_not_convertible_exception);
    ASSERT_THROW((void)container.template resolve<int *>(),
                 dingo::type_not_convertible_exception);
    ASSERT_THROW((void)container.template resolve<volatile int *>(),
                 dingo::type_not_convertible_exception);

    volatile int volatile_value = 7;
    Container volatile_container;
    volatile_container.template register_type<dingo::scope<dingo::external>,
                                              dingo::storage<volatile int &>>(
        volatile_value);

    ASSERT_EQ(volatile_container.template resolve<int>(), 7);
    ASSERT_TRUE(&volatile_container.template resolve<volatile int &>() ==
                &volatile_value);
    ASSERT_TRUE(&volatile_container.template resolve<const volatile int &>() ==
                &volatile_value);
    ASSERT_TRUE(volatile_container.template resolve<volatile int *>() ==
                &volatile_value);
    ASSERT_TRUE(volatile_container.template resolve<const volatile int *>() ==
                &volatile_value);
    ASSERT_THROW((void)volatile_container.template resolve<int &>(),
                 dingo::type_not_convertible_exception);
    ASSERT_THROW((void)volatile_container.template resolve<const int &>(),
                 dingo::type_not_convertible_exception);
    ASSERT_THROW((void)volatile_container.template resolve<int *>(),
                 dingo::type_not_convertible_exception);
    ASSERT_THROW((void)volatile_container.template resolve<const int *>(),
                 dingo::type_not_convertible_exception);

    int value = 13;
    int *pointer = &value;
    Container nested_pointer_container;
    nested_pointer_container.template register_type<
        dingo::scope<dingo::external>, dingo::storage<int **>>(&pointer);

    ASSERT_EQ(nested_pointer_container.template resolve<int **>(), &pointer);
    ASSERT_EQ(nested_pointer_container.template resolve<const int *const *>(),
              static_cast<const int *const *>(&pointer));
    ASSERT_THROW(
        (void)nested_pointer_container.template resolve<const int **>(),
        dingo::type_not_convertible_exception);
  }
};

namespace conversion_delivery {

inline resolution_test::move_target make_move_target() {
  return resolution_test::move_target{};
}

template <typename Container>
void check_const_move_only_value(Container &container) {
  auto resolved =
      container.template resolve<const resolution_test::move_target>();
  ASSERT_EQ(resolved.value, 23);
}

template <typename Container>
void check_copy_only_alternative(Container &container, int expected) {
  resolution_test::copy_only::copies = 0;
  auto resolved = container.template resolve<resolution_test::copy_only>();
  ASSERT_EQ(resolved.value, expected);
  ASSERT_EQ(resolution_test::copy_only::copies, 1);
}

template <typename Container>
void check_fresh_move_only_conversions(Container &container) {
  auto first = container.template resolve<resolution_test::move_target>();
  auto second = container.template resolve<resolution_test::move_target>();
  ASSERT_EQ(first.value, 23);
  ASSERT_EQ(second.value, 23);
}

template <typename Container>
void check_copy_only_conversion(Container &container) {
  auto resolved =
      container
          .template resolve<resolution_test::copy_only_conversion_target>();
  ASSERT_EQ(resolved.value, 53);
}

template <typename Container> void exercise_runtime() {
  {
    Container container;
    container.template register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<resolution_test::move_target>,
        dingo::factory<dingo::function<&make_move_target>>>();
    check_const_move_only_value(container);
  }
  {
    resolution_test::copy_only source(41);
    std::variant<resolution_test::copy_only, int> value(
        std::in_place_type<resolution_test::copy_only>, source);
    Container container;
    container.template register_type<
        dingo::scope<dingo::external>,
        dingo::storage<std::variant<resolution_test::copy_only, int> &>,
        dingo::interfaces<resolution_test::copy_only>>(value);
    check_copy_only_alternative(container, 41);
    ASSERT_EQ(std::get<resolution_test::copy_only>(value).value, 41);
  }
  {
    resolution_test::move_source source;
    Container container;
    container
        .template register_type<dingo::scope<dingo::external>,
                                dingo::storage<resolution_test::move_source *>,
                                dingo::interfaces<resolution_test::move_target,
                                                  resolution_test::secondary>>(
            &source);
    check_fresh_move_only_conversions(container);
    ASSERT_EQ(source.value, 23);
  }
  {
    resolution_test::copy_only_conversion_source source;
    Container container;
    container.template register_type<
        dingo::scope<dingo::external>,
        dingo::storage<resolution_test::copy_only_conversion_source &>,
        dingo::interfaces<resolution_test::copy_only_conversion_target>>(
        source);
    check_copy_only_conversion(container);
  }
}

template <template <typename...> typename Container> void exercise_static() {
  {
    using bindings = dingo::bindings<
        dingo::bind<dingo::scope<dingo::unique>,
                    dingo::storage<resolution_test::move_target>,
                    dingo::factory<dingo::function<&make_move_target>>>>;
    Container<bindings> container;
    check_const_move_only_value(container);
  }
  {
    using bindings = dingo::bindings<dingo::bind<
        dingo::scope<dingo::shared>,
        dingo::storage<std::variant<resolution_test::copy_only, int>>,
        dingo::interfaces<resolution_test::copy_only>>>;
    Container<bindings> container;
    check_copy_only_alternative(container, 31);
  }
  {
    using bindings = dingo::bindings<
        dingo::bind<dingo::scope<dingo::shared>,
                    dingo::storage<resolution_test::move_source>,
                    dingo::interfaces<resolution_test::move_target,
                                      resolution_test::secondary>>>;
    Container<bindings> container;
    check_fresh_move_only_conversions(container);
  }
  {
    using bindings = dingo::bindings<dingo::bind<
        dingo::scope<dingo::shared>,
        dingo::storage<resolution_test::copy_only_conversion_source>,
        dingo::interfaces<resolution_test::copy_only_conversion_target>>>;
    Container<bindings> container;
    check_copy_only_conversion(container);
  }
}

} // namespace conversion_delivery

inline void exercise_conversion_delivery_regressions() {
  conversion_delivery::exercise_runtime<dingo::runtime_container<>>();
  conversion_delivery::exercise_runtime<dingo::container<>>();
  conversion_delivery::exercise_static<dingo::static_container>();
  conversion_delivery::exercise_static<dingo::container>();
}

} // namespace dingo::matrix
