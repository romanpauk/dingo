//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

namespace dingo::matrix {

template <typename ParentShape, typename ChildShape>
void assert_parent_cache_shadowing() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  parent_type parent;
  child_type child(&parent);
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<1>>,
                                dingo::interfaces<nested_processor>>();

  auto &inherited = child.template resolve<nested_processor &>();
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<nested_processor_impl<2>>,
                               dingo::interfaces<nested_processor>>();
  auto &local = child.template resolve<nested_processor &>();

  ASSERT_EQ(inherited.id(), 1);
  ASSERT_EQ(local.id(), 2);
  ASSERT_NE(&local, &inherited);
  ASSERT_EQ(&parent.template resolve<nested_processor &>(), &inherited);
}

template <typename ParentShape, typename ChildShape>
void assert_parent_decorator_regression() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  parent_type decorated_parent;
  child_type decorated_child(&decorated_parent);
  decorated_parent.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_decorated_value>>>(dingo::callable(
      [] { return std::make_shared<nested_decorated_value>(10); }));
  decorated_child.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_decorated_value>>>(
      dingo::callable([&decorated_parent] {
        auto inner =
            decorated_parent
                .template resolve<std::shared_ptr<nested_decorated_value>>();
        return std::make_shared<nested_decorated_value>(inner->value + 1);
      }));

  ASSERT_EQ(decorated_child
                .template resolve<std::shared_ptr<nested_decorated_value>>()
                ->value,
            11);
  ASSERT_EQ(decorated_parent
                .template resolve<std::shared_ptr<nested_decorated_value>>()
                ->value,
            10);
}

template <typename = void> void exercise_parent_cache_pairs() {
  assert_parent_cache_shadowing<nested_runtime_parent_shape,
                                nested_runtime_child_shape>();
  assert_parent_cache_shadowing<nested_runtime_parent_shape,
                                nested_container_child_shape>();
  assert_parent_cache_shadowing<nested_container_parent_shape,
                                nested_runtime_child_shape>();
  assert_parent_cache_shadowing<nested_container_parent_shape,
                                nested_container_child_shape>();
  assert_parent_decorator_regression<nested_runtime_parent_shape,
                                     nested_runtime_child_shape>();
  assert_parent_decorator_regression<nested_container_parent_shape,
                                     nested_container_child_shape>();
}

} // namespace dingo::matrix
