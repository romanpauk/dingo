//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include "support/containers.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>

// Invoke accepts any kind of callable: it reads the parameters from the
// callable's signature, or from the signature it is given, and resolves each
// one. The requests themselves are checked by the model probe in spec/.

using namespace dingo;

namespace {
struct value {
  int marker() const { return 3; }
};

using value_binding = dingo::bind<scope<shared>, storage<value>>;

int free_function(value &dependency) { return dependency.marker() + 54; }

int noexcept_function(value &dependency) noexcept {
  return dependency.marker() + 60;
}

struct static_member {
  static int invoke(value &dependency) { return dependency.marker() + 66; }
};

struct overloaded {
  int operator()(value &dependency) const { return dependency.marker() + 42; }
  int operator()(int other) const { return other + 100; }
};

struct lvalue_qualified {
  int operator()(value &dependency) & { return dependency.marker() + 72; }
};

struct rvalue_qualified {
  int operator()(value &dependency) && { return dependency.marker() + 78; }
};

struct noexcept_functor {
  int operator()(value &dependency) const noexcept {
    return dependency.marker() + 84;
  }
};

struct move_only_functor {
  move_only_functor() : offset(std::make_unique<int>(90)) {}
  move_only_functor(const move_only_functor &) = delete;
  move_only_functor(move_only_functor &&) noexcept = default;

  int operator()(value &dependency) const {
    return dependency.marker() + *offset;
  }

  std::unique_ptr<int> offset;
};
} // namespace

template <typename T> struct invoke_callables_test : testing::Test {};

TYPED_TEST_SUITE(invoke_callables_test, container_kinds, );

TYPED_TEST(invoke_callables_test, free_function_pointer) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(free_function), 57);
    EXPECT_EQ(container.invoke(&free_function), 57);
  });
}

TYPED_TEST(invoke_callables_test, noexcept_function_pointer) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(noexcept_function), 63);
  });
}

TYPED_TEST(invoke_callables_test, static_member_function_pointer) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(static_member::invoke), 69);
  });
}

// The object a member function is called on is resolved like a parameter.
TYPED_TEST(invoke_callables_test, member_function_pointer) {
  TypeParam::template with<value_binding>(
      [](auto &container) { EXPECT_EQ(container.invoke(&value::marker), 3); });
}

TYPED_TEST(invoke_callables_test, std_function) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(std::function<int(value &)>(
                  [](value &dependency) { return dependency.marker() + 48; })),
              51);
  });
}

TYPED_TEST(invoke_callables_test, move_only_function) {
#if defined(__cpp_lib_move_only_function)
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(std::move_only_function<int(value &)>(
                  [](value &dependency) { return dependency.marker() + 114; })),
              117);
  });
#else
  GTEST_SKIP() << "std::move_only_function is not available";
#endif
}

TYPED_TEST(invoke_callables_test, move_only_functor) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(move_only_functor{}), 93);
  });
}

TYPED_TEST(invoke_callables_test, mutable_lambda) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke([offset = 96](value &dependency) mutable {
      return dependency.marker() + offset++;
    }),
              99);
  });
}

TYPED_TEST(invoke_callables_test, noexcept_functor) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(noexcept_functor{}), 87);
  });
}

TYPED_TEST(invoke_callables_test, lvalue_ref_qualified_functor) {
  TypeParam::template with<value_binding>([](auto &container) {
    lvalue_qualified invoker;
    EXPECT_EQ(container.invoke(invoker), 75);
  });
}

TYPED_TEST(invoke_callables_test, rvalue_ref_qualified_functor) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.invoke(rvalue_qualified{}), 81);
  });
}

// A signature given to invoke picks one overload of the call operator.
TYPED_TEST(invoke_callables_test, explicit_signature_selects_overload) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.template invoke<int(value &)>(overloaded{}), 45);
  });
}

// A signature given to invoke instantiates a generic lambda.
TYPED_TEST(invoke_callables_test, explicit_signature_of_generic_lambda) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.template invoke<int(value &)>(
                  [](auto &dependency) { return dependency.marker() + 102; }),
              105);
  });
}

// Every parameter is resolved, each from its own request.
TYPED_TEST(invoke_callables_test, multi_argument_callable) {
  TypeParam::template with<value_binding>([](auto &container) {
    EXPECT_EQ(container.template invoke<int(value &, const value &)>(
                  [](value &first, const value &second) {
                    EXPECT_EQ(&first, &second);
                    return first.marker() + second.marker() + 108;
                  }),
              114);
  });
}
