//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/containers/indexed.h"
#include "matrix/fixtures/values.h"

#include <memory>
#include <string>
#include <type_traits>

namespace dingo::matrix {

template <typename T> struct indexed_definition_key_arg {
  using type = T;
};

template <typename T, auto... Values>
struct indexed_definition_key_arg<dingo::key_type<T, Values...>> {
  using type = T;
};

template <typename Definition> struct indexed_definition_key;

template <typename Key, typename Interface>
struct indexed_definition_key<dingo::associative<Key, Interface, dingo::one>> {
  using type = typename indexed_definition_key_arg<Key>::type;
};

template <typename Head, typename... Tail>
struct indexed_definition_key<dingo::lookups<Head, Tail...>> {
  using type = typename indexed_definition_key<Head>::type;
};

template <typename Container> struct indexed_key {
  using type = typename indexed_definition_key<typename std::remove_reference_t<
      Container>::lookup_definition_type>::type;
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

template <typename Key, Key Value> struct fixed_indexed_dependency_consumer {
  explicit fixed_indexed_dependency_consumer(
      dingo::dependency<element_interface &, dingo::key_type<Key, Value>> value)
      : dependency(value) {}

  element_interface &dependency;
};

template <typename Container, typename Key,
          bool Supported = std::is_integral_v<Key>>
struct fixed_indexed_dependency_check {
  static void run(Container &) {}
};

template <typename Container, typename Key>
struct fixed_indexed_dependency_check<Container, Key, true> {
  static void run(Container &container) {
    using first_consumer_type =
        fixed_indexed_dependency_consumer<Key, static_cast<Key>(2)>;
    using second_consumer_type =
        fixed_indexed_dependency_consumer<Key, static_cast<Key>(3)>;
    using missing_consumer_type =
        fixed_indexed_dependency_consumer<Key, static_cast<Key>(6)>;

    auto first = container.template construct<first_consumer_type>();
    auto second = container.template construct<second_consumer_type>();

    ASSERT_EQ(first.dependency.id(), 2);
    ASSERT_EQ(second.dependency.id(), 3);
    ASSERT_THROW((container.template construct<missing_consumer_type>()),
                 type_not_found_exception);

    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<first_consumer_type>>();

    auto &registered = container.template resolve<first_consumer_type &>();

    ASSERT_EQ(registered.dependency.id(), 2);
  }
};

template <typename Container>
void exercise_indexed_registration_container(Container &container) {
  using key_type = indexed_key_t<Container>;

  container
      .template register_type<dingo::scope<dingo::unique>,
                              dingo::storage<std::unique_ptr<element_type<0>>>,
                              dingo::interfaces<element_interface>>(
          dingo::key_value{indexed_value<key_type>(0)});
  container
      .template register_type<dingo::scope<dingo::unique>,
                              dingo::storage<std::unique_ptr<element_type<1>>>,
                              dingo::interfaces<element_interface>>(
          dingo::key_value{indexed_value<key_type>(1)});
  ASSERT_THROW((container.template register_type<
                   dingo::scope<dingo::unique>,
                   dingo::storage<std::unique_ptr<element_type<1>>>,
                   dingo::interfaces<element_interface>>(
                   dingo::key_value{indexed_value<key_type>(1)})),
               lookup_already_registered_exception);

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
  ASSERT_THROW((container.template resolve<std::shared_ptr<element_interface>>(
                   indexed_value<key_type>(-1))),
               type_not_found_exception);

  container
      .template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<std::shared_ptr<element_type<2>>>,
                              dingo::interfaces<element_interface>>(
          dingo::key_value{indexed_value<key_type>(2)});
  container
      .template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<std::shared_ptr<element_type<3>>>,
                              dingo::interfaces<element_interface>>(
          dingo::key_value{indexed_value<key_type>(3)});

  auto &second = container.template resolve<element_interface &>(
      indexed_value<key_type>(2));
  auto &third = container.template resolve<element_interface &>(
      indexed_value<key_type>(3));
  ASSERT_NE(std::addressof(second), std::addressof(third));
  ASSERT_EQ(std::addressof(second),
            container.template resolve<element_interface *>(
                indexed_value<key_type>(2)));
  ASSERT_EQ(std::addressof(third),
            container.template resolve<element_interface *>(
                indexed_value<key_type>(3)));

  ASSERT_THROW((container.template register_type<
                   dingo::scope<dingo::shared>,
                   dingo::storage<std::shared_ptr<element_type<3>>>,
                   dingo::interfaces<element_interface>>(
                   dingo::key_value{indexed_value<key_type>(3)})),
               lookup_already_registered_exception);
  ASSERT_THROW((container.template register_type<
                   dingo::scope<dingo::shared>,
                   dingo::storage<std::shared_ptr<element_type<4>>>,
                   dingo::interfaces<element_interface>>(
                   dingo::key_value{indexed_value<key_type>(3)})),
               lookup_already_registered_exception);

  ASSERT_EQ(second.id(), 2);
  ASSERT_EQ(third.id(), 3);
  ASSERT_EQ(std::addressof(third),
            std::addressof(container.template resolve<element_interface &>(
                indexed_value<key_type>(3))));
  ASSERT_THROW((container.template resolve<element_interface &>(
                   indexed_value<key_type>(-2))),
               type_not_found_exception);
  ASSERT_THROW((container.template resolve<element_interface &>()),
               type_not_found_exception);

  container
      .template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<std::shared_ptr<element_type<4>>>,
                              dingo::interfaces<element_interface>>(
          dingo::key_value{indexed_value<key_type>(4)});
  container
      .template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<std::shared_ptr<element_type<4>>>,
                              dingo::interfaces<element_interface>>(
          dingo::key_value{indexed_value<key_type>(5)});

  auto &fourth = container.template resolve<element_interface &>(
      indexed_value<key_type>(4));
  auto &fifth = container.template resolve<element_interface &>(
      indexed_value<key_type>(5));
  ASSERT_NE(std::addressof(fourth), std::addressof(fifth));
  ASSERT_EQ(std::addressof(fourth),
            container.template resolve<element_interface *>(
                indexed_value<key_type>(4)));
  ASSERT_EQ(std::addressof(fifth),
            container.template resolve<element_interface *>(
                indexed_value<key_type>(5)));

  fixed_indexed_dependency_check<Container, key_type>::run(container);
}

template <typename Container>
void exercise_indexed_registration(Container &container) {
  exercise_indexed_registration_container(container);
}

struct indexed_registration_scenario {
  template <typename Container> static void run(Container &container) {
    exercise_indexed_registration(container);
  }
};

} // namespace dingo::matrix
