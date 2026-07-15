//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

namespace dingo::matrix {

template <typename RootShape, typename ParentShape, typename ChildShape>
void assert_origin_after_three_level_search() {
  using root_type = typename RootShape::template type<nested_container_traits>;
  using parent_type =
      typename ParentShape::template type<nested_container_traits, root_type>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  root_type root;
  parent_type parent(&root);
  child_type child(&parent);
  root.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_inherited_dependency>>>(
      dingo::callable(
          [] { return std::make_shared<nested_inherited_dependency>(1); }));
  child.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_local_dependency>>>(dingo::callable(
      [] { return std::make_shared<nested_local_dependency>(2); }));
  child.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<nested_packed_service>>>();

  auto service =
      child.template resolve<std::shared_ptr<nested_packed_service>>();
  ASSERT_EQ(service->dependencies.local,
            child.template resolve<std::shared_ptr<nested_local_dependency>>());
  ASSERT_EQ(
      service->dependencies.inherited,
      root.template resolve<std::shared_ptr<nested_inherited_dependency>>());
  ASSERT_THROW(
      parent.template resolve<std::shared_ptr<nested_local_dependency>>(),
      dingo::type_not_found_exception);
  ASSERT_THROW(
      root.template resolve<std::shared_ptr<nested_local_dependency>>(),
      dingo::type_not_found_exception);
}

template <typename = void> void exercise_parent_origin_chains() {
  assert_origin_after_three_level_search<nested_runtime_parent_shape,
                                         nested_runtime_child_shape,
                                         nested_runtime_child_shape>();
  assert_origin_after_three_level_search<nested_runtime_parent_shape,
                                         nested_runtime_child_shape,
                                         nested_container_child_shape>();
  assert_origin_after_three_level_search<nested_runtime_parent_shape,
                                         nested_container_child_shape,
                                         nested_runtime_child_shape>();
  assert_origin_after_three_level_search<nested_runtime_parent_shape,
                                         nested_container_child_shape,
                                         nested_container_child_shape>();
  assert_origin_after_three_level_search<nested_container_parent_shape,
                                         nested_runtime_child_shape,
                                         nested_runtime_child_shape>();
  assert_origin_after_three_level_search<nested_container_parent_shape,
                                         nested_runtime_child_shape,
                                         nested_container_child_shape>();
  assert_origin_after_three_level_search<nested_container_parent_shape,
                                         nested_container_child_shape,
                                         nested_runtime_child_shape>();
  assert_origin_after_three_level_search<nested_container_parent_shape,
                                         nested_container_child_shape,
                                         nested_container_child_shape>();
}

} // namespace dingo::matrix
