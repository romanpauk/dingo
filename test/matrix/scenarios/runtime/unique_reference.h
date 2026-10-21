//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/type_name.h"
#include "matrix/scenarios/runtime/common.h"

#include <string>

namespace dingo::matrix {

struct runtime_unique_reference_regression_scenario {
  template <typename Container> static void run(Container &container) {
    container
        .template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<shared_from_unique_reference>>();
    container.template register_type<
        dingo::scope<dingo::unique>, dingo::storage<unique_reference_value>,
        dingo::factory<dingo::constructor<unique_reference_value()>>>();
#if !defined(DINGO_MATRIX_BASELINE)
    // A consuming scope never serves an lvalue reference to the object it
    // consumed, as the behavior change consumed_objects_are_never_borrowed of
    // the spec declares: the dependency is refused, and the shared object is
    // not constructed, so a second request is refused again. The baseline
    // serves a reference to the unique object.
    try {
      (void)container.template resolve<shared_from_unique_reference &>();
      FAIL() << "expected type_not_convertible_exception";
    } catch (const dingo::type_not_convertible_exception &exception) {
      std::string expected = "type is not convertible to ";
      expected += type_name<const unique_reference_value &>();
      expected += " from ";
      expected += type_name<unique_reference_value>();
      expected += " (required by ";
      expected += type_name<shared_from_unique_reference>();
      expected += ")";
      ASSERT_STREQ(exception.what(), expected.c_str());
    }
    ASSERT_THROW(container.template resolve<shared_from_unique_reference &>(),
                 dingo::type_not_convertible_exception);
#else
    auto &shared = container.template resolve<shared_from_unique_reference &>();
    ASSERT_FALSE(shared.token.expired());
    ASSERT_FALSE(container.template resolve<shared_from_unique_reference &>()
                     .token.expired());
#endif

    int destructor_count = 0;
    container.template register_type<dingo::scope<dingo::external>,
                                     dingo::storage<int &>>(destructor_count);
    container.template register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<unique_reference_exception_value>>();
    container.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<shared_unique_reference_exception_value>>();
#if !defined(DINGO_MATRIX_BASELINE)
    // The same refusal: the unique object is not constructed, so nothing is
    // destroyed, where the baseline destroys it once when the constructor of
    // its consumer throws.
    ASSERT_THROW(
        container.template resolve<shared_unique_reference_exception_value>(),
        dingo::type_not_convertible_exception);
    ASSERT_EQ(destructor_count, 0);
#else
    ASSERT_THROW(
        container.template resolve<shared_unique_reference_exception_value>(),
        std::runtime_error);
    ASSERT_EQ(destructor_count, 1);
#endif
  }
};

} // namespace dingo::matrix
