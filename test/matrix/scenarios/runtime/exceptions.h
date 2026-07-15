//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/runtime/common.h"

namespace dingo::matrix {

struct runtime_exception_regression_scenario {
  template <typename Container> static void run(Container &container) {
    struct missing_dependency {};
    struct missing_type {
      explicit missing_type(missing_dependency &) {}
    };
    try {
      (void)container.template resolve<missing_type>();
      FAIL() << "expected type_not_found_exception";
    } catch (const dingo::type_not_found_exception &exception) {
      std::string expected = "type not found: ";
      expected += type_name<missing_type>();
      ASSERT_STREQ(exception.what(), expected.c_str());
    }

    struct leaf_dependency {};
    struct dependency {
      explicit dependency(leaf_dependency &) {}
    };
    struct missing_with_dependency {
      explicit missing_with_dependency(dependency &) {}
    };
    container.template register_type<dingo::scope<dingo::unique>,
                                     dingo::storage<missing_with_dependency>>();
    try {
      (void)container.template resolve<missing_with_dependency>();
      FAIL() << "expected type_not_found_exception";
    } catch (const dingo::type_not_found_exception &exception) {
      std::string expected = "type not found: ";
      expected += type_name<dependency &>();
      expected += " (required by ";
      expected += type_name<missing_with_dependency>();
      expected += ")";
      ASSERT_STREQ(exception.what(), expected.c_str());
    }

    try {
      (void)container
          .template construct_collection<std::vector<interface_type *>>();
      FAIL() << "expected type_not_found_exception";
    } catch (const dingo::type_not_found_exception &exception) {
      std::string expected = "type not found for collection ";
      expected += type_name<std::vector<interface_type *>>();
      expected += " (element type: ";
      expected += type_name<interface_type *>();
      expected += ")";
      ASSERT_STREQ(exception.what(), expected.c_str());
    }
  }
};

} // namespace dingo::matrix
