//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/registration/constructor.h>

#include "matrix/support/values.h"

namespace dingo::matrix {

struct factory_function_value_type {
private:
  explicit factory_function_value_type(int init) : value(init) {}

public:
  static factory_function_value_type create() {
    return factory_function_value_type(9);
  }

  int value;
};

struct factory_function_dependency_value_type {
private:
  explicit factory_function_dependency_value_type(int init) : value(init) {}

public:
  static factory_function_dependency_value_type create(value_type &dependency) {
    return factory_function_dependency_value_type(dependency.marker() + 36);
  }

  int value;
};

struct factory_constructor_value_type {
  explicit factory_constructor_value_type(value_type &dependency)
      : value(dependency.marker() + 6) {}

  int value;
};

struct factory_detected_constructor_value_type {
  explicit factory_detected_constructor_value_type(value_type &dependency)
      : value(dependency.marker() + 24) {}

  int value;
};

struct factory_typedef_constructor_value_type {
  DINGO_CONSTRUCTOR(
      factory_typedef_constructor_value_type(value_type &dependency))
      : value(dependency.marker() + 30) {}

  int value;
};

struct factory_callable_value_type {
  explicit factory_callable_value_type(int init) : value(init) {}

  int value;
};

struct factory_overloaded_callable {
  factory_callable_value_type operator()(value_type &dependency) const {
    return factory_callable_value_type(dependency.marker() + 18);
  }

  factory_callable_value_type operator()(int value) const {
    return factory_callable_value_type(value + 100);
  }
};

} // namespace dingo::matrix
