//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

namespace dingo::matrix {

struct native_parent_container_scenario {
  template <typename Container> static void run(Container &) {
    typename Container::allocator_type allocator;
    Container parent(allocator);
    parent.template register_type<dingo::scope<dingo::external>,
                                  dingo::storage<int>>(1);
    parent.template register_type<dingo::scope<dingo::unique>,
                                  dingo::storage<nested_value_type>>();
    parent.template register_type<dingo::scope<dingo::unique>,
                                  dingo::storage<nested_parent_value_type>>();
    Container child(&parent, allocator);
    ASSERT_EQ(child.template resolve<nested_parent_value_type>().value, 1);
    child.template register_type<dingo::scope<dingo::external>,
                                 dingo::storage<int>>(2);
    child.template register_type<dingo::scope<dingo::unique>,
                                 dingo::storage<nested_value_type>>();
    ASSERT_EQ(parent.template resolve<nested_value_type>().value, 1);
    ASSERT_EQ(child.template resolve<nested_value_type>().value, 2);
  }
};

} // namespace dingo::matrix
