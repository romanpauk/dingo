//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/assertions.h"
#include "matrix/fixtures/values.h"

#include <functional>

namespace dingo::matrix {

struct invoke_inferred_lambda {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(
        [](value_type &dependency) { return dependency.marker(); });
    ASSERT_EQ(invoked, 3);
  }
};

struct invoke_explicit_signature_overload {
  template <typename Container> static void run(Container &container) {
    auto invoked =
        container.template invoke<int(value_type &)>(overloaded_invoker{});
    ASSERT_EQ(invoked, 45);
  }
};

struct invoke_std_function {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(std::function<int(value_type &)>(
        [](value_type &dependency) { return dependency.marker() + 48; }));
    ASSERT_EQ(invoked, 51);
  }
};

struct invoke_free_function_pointer {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(free_function_invoke);
    ASSERT_EQ(invoked, 57);
  }
};

struct invoke_noexcept_function_pointer {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(noexcept_function_invoke);
    ASSERT_EQ(invoked, 63);
  }
};

struct invoke_static_member_function_pointer {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(member_function_invoker::invoke);
    ASSERT_EQ(invoked, 69);
  }
};

struct invoke_member_function_pointer {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(&value_type::marker);
    ASSERT_EQ(invoked, 3);
  }
};

struct invoke_mutable_lambda {
  template <typename Container> static void run(Container &container) {
    auto invoked =
        container.invoke([offset = 96](value_type &dependency) mutable {
          return dependency.marker() + offset;
        });
    ASSERT_EQ(invoked, 99);
  }
};

struct invoke_generic_lambda_explicit_signature {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.template invoke<int(value_type &)>(
        [](auto &dependency) { return dependency.marker() + 102; });
    ASSERT_EQ(invoked, 105);
  }
};

struct invoke_lvalue_ref_qualified_functor {
  template <typename Container> static void run(Container &container) {
    lvalue_ref_qualified_invoker invoker;
    auto invoked = container.invoke(invoker);
    ASSERT_EQ(invoked, 75);
  }
};

struct invoke_rvalue_ref_qualified_functor {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(rvalue_ref_qualified_invoker{});
    ASSERT_EQ(invoked, 81);
  }
};

struct invoke_noexcept_functor {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(noexcept_invoker{});
    ASSERT_EQ(invoked, 87);
  }
};

struct invoke_move_only_functor {
  template <typename Container> static void run(Container &container) {
    auto invoked = container.invoke(move_only_invoker{});
    ASSERT_EQ(invoked, 93);
  }
};

struct invoke_multi_argument_callable {
  template <typename Container> static void run(Container &container) {
    auto invoked =
        container.template invoke<int(value_type &, const value_type &)>(
            [](value_type &first, const value_type &second) {
              return first.marker() + second.marker() + 108;
            });
    ASSERT_EQ(invoked, 114);
  }
};

struct invoke_move_only_function {
  template <typename Container> static void run(Container &container) {
#if defined(__cpp_lib_move_only_function)
    auto invoked = container.invoke(std::move_only_function<int(value_type &)>(
        [](value_type &dependency) { return dependency.marker() + 114; }));
    ASSERT_EQ(invoked, 117);
#else
    (void)container;
#endif
  }
};

} // namespace dingo::matrix
