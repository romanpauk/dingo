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
void assert_parent_resolution() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  parent_type parent;
  parent.template register_type<dingo::scope<dingo::external>,
                                dingo::storage<int>>(1);
  parent.template register_type<dingo::scope<dingo::unique>,
                                dingo::storage<nested_value_type>>();
  parent.template register_type<dingo::scope<dingo::unique>,
                                dingo::storage<nested_parent_value_type>>();
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<1>>,
                                dingo::interfaces<nested_processor>>();

  child_type child(&parent);
  ASSERT_EQ(child.template resolve<nested_parent_value_type>().value, 1);
  auto inherited = child.template resolve<std::vector<nested_processor *>>();
  ASSERT_EQ(inherited.size(), 1u);
  ASSERT_EQ(inherited[0]->id(), 1);

  child.template register_type<dingo::scope<dingo::external>,
                               dingo::storage<int>>(2);
  child.template register_type<dingo::scope<dingo::unique>,
                               dingo::storage<nested_value_type>>();
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<nested_processor_impl<2>>,
                               dingo::interfaces<nested_processor>>();
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<nested_processor_impl<3>>,
                               dingo::interfaces<nested_processor>>();

  ASSERT_EQ(parent.template resolve<nested_value_type>().value, 1);
  ASSERT_EQ(child.template resolve<nested_value_type>().value, 2);
  auto local = child.template resolve<std::vector<nested_processor *>>();
  ASSERT_EQ(local.size(), 2u);
  ASSERT_THROW(child.template resolve<nested_processor &>(),
               dingo::type_ambiguous_exception);
}

template <typename = void> void exercise_parent_resolution_pairs() {
  assert_parent_resolution<nested_runtime_parent_shape,
                           nested_runtime_child_shape>();
  assert_parent_resolution<nested_runtime_parent_shape,
                           nested_container_child_shape>();
  assert_parent_resolution<nested_container_parent_shape,
                           nested_runtime_child_shape>();
  assert_parent_resolution<nested_container_parent_shape,
                           nested_container_child_shape>();
}

} // namespace dingo::matrix
