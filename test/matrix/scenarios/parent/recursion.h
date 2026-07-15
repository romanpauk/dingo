//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

namespace dingo::matrix {

struct nested_recursive_b;

struct nested_recursive_a {
  explicit nested_recursive_a(std::shared_ptr<nested_recursive_b> dependency)
      : value(std::move(dependency)) {}

  std::shared_ptr<nested_recursive_b> value;
};

struct nested_recursive_b {
  explicit nested_recursive_b(std::shared_ptr<nested_recursive_a> dependency)
      : value(std::move(dependency)) {}

  std::shared_ptr<nested_recursive_a> value;
};

struct nested_recursive_value {
  explicit nested_recursive_value(int initial_value) : value(initial_value) {}

  int value;
};

template <typename ContainerShape> void assert_recursion_within_container() {
  using container_type =
      typename ContainerShape::template type<nested_container_traits>;

  container_type container;
  container.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_recursive_a>>>();
  container.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_recursive_b>>>();

  ASSERT_THROW(
      container.template resolve<std::shared_ptr<nested_recursive_a>>(),
      dingo::type_recursion_exception);
}

template <typename FirstShape, typename SecondShape>
void assert_recursion_across_containers() {
  using first_type =
      typename FirstShape::template type<nested_container_traits>;
  using second_type =
      typename SecondShape::template type<nested_container_traits>;

  first_type first;
  second_type second;
  first.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_recursive_value>>>(
      dingo::callable([&second] {
        return second
            .template resolve<std::shared_ptr<nested_recursive_value>>();
      }));
  second.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_recursive_value>>>(
      dingo::callable([&first] {
        return first
            .template resolve<std::shared_ptr<nested_recursive_value>>();
      }));

  ASSERT_THROW(
      first.template resolve<std::shared_ptr<nested_recursive_value>>(),
      dingo::type_recursion_exception);
}

template <typename = void> void exercise_parent_recursion_pairs() {
  assert_recursion_within_container<nested_runtime_parent_shape>();
  assert_recursion_within_container<nested_container_parent_shape>();

  assert_recursion_across_containers<nested_runtime_parent_shape,
                                     nested_runtime_parent_shape>();
  assert_recursion_across_containers<nested_runtime_parent_shape,
                                     nested_container_parent_shape>();
  assert_recursion_across_containers<nested_container_parent_shape,
                                     nested_runtime_parent_shape>();
  assert_recursion_across_containers<nested_container_parent_shape,
                                     nested_container_parent_shape>();
}

} // namespace dingo::matrix
