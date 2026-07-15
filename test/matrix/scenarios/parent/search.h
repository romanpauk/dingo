//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

#include <dingo/storage/shared.h>

namespace dingo::matrix {

template <typename RootShape, typename ParentShape, typename ChildShape>
void assert_intermediate_parent_ambiguity() {
  using root_type = typename RootShape::template type<nested_container_traits>;
  using parent_type =
      typename ParentShape::template type<nested_container_traits, root_type>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  root_type root;
  parent_type parent(&root);
  child_type child(&parent);
  root.template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<nested_processor_impl<1>>,
                              dingo::interfaces<nested_processor>>();
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<2>>,
                                dingo::interfaces<nested_processor>>();
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<3>>,
                                dingo::interfaces<nested_processor>>();

  ASSERT_THROW(child.template resolve<nested_processor &>(),
               dingo::type_ambiguous_exception);
}

template <typename = void> void exercise_intermediate_ambiguity_chains() {
  assert_intermediate_parent_ambiguity<nested_runtime_parent_shape,
                                       nested_runtime_child_shape,
                                       nested_runtime_child_shape>();
  assert_intermediate_parent_ambiguity<nested_runtime_parent_shape,
                                       nested_runtime_child_shape,
                                       nested_container_child_shape>();
  assert_intermediate_parent_ambiguity<nested_runtime_parent_shape,
                                       nested_container_child_shape,
                                       nested_runtime_child_shape>();
  assert_intermediate_parent_ambiguity<nested_runtime_parent_shape,
                                       nested_container_child_shape,
                                       nested_container_child_shape>();
  assert_intermediate_parent_ambiguity<nested_container_parent_shape,
                                       nested_runtime_child_shape,
                                       nested_runtime_child_shape>();
  assert_intermediate_parent_ambiguity<nested_container_parent_shape,
                                       nested_runtime_child_shape,
                                       nested_container_child_shape>();
  assert_intermediate_parent_ambiguity<nested_container_parent_shape,
                                       nested_container_child_shape,
                                       nested_runtime_child_shape>();
  assert_intermediate_parent_ambiguity<nested_container_parent_shape,
                                       nested_container_child_shape,
                                       nested_container_child_shape>();
}

} // namespace dingo::matrix
