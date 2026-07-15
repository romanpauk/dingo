//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/assertions.h"

#include <type_traits>

namespace dingo::matrix::resolution {

template <typename Scenario> struct scenario {
  template <typename Container> static void check(Container &container) {
    Scenario::run(container);
  }
};

template <auto Function> struct no_container_function {
  static void run_without_container() { Function(); }
};

template <typename Type, int Expected> struct member_value_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Type &>();
    ASSERT_EQ(instance.value, Expected);
  }
};

template <typename Type, int Expected> struct member_value {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<Type>();
    ASSERT_EQ(instance.value, Expected);
  }
};

template <typename Type, int Expected> struct member_value_ptr {
  template <typename Container> static void check(Container &container) {
    auto *instance = container.template resolve<Type *>();
    ASSERT_EQ(instance->value, Expected);
  }
};

template <typename Policy, typename = void> struct policy_before {
  static void run() {}
};

template <typename Policy>
struct policy_before<Policy, std::void_t<decltype(Policy::before())>> {
  static void run() { Policy::before(); }
};

template <typename Policy, typename = void> struct policy_after {
  static void run() {}
};

template <typename Policy>
struct policy_after<Policy, std::void_t<decltype(Policy::after())>> {
  static void run() { Policy::after(); }
};

template <typename Policy, typename Case, typename = void>
struct policy_invocation {
  static void run() {
    Case::with_container([](auto &container) { Policy::check(container); });
  }
};

template <typename Policy, typename Case>
struct policy_invocation<
    Policy, Case, std::void_t<decltype(Policy::run_without_container())>> {
  static void run() { Policy::run_without_container(); }
};

template <typename Case> void run() {
  policy_before<typename Case::policy>::run();
  policy_invocation<typename Case::policy, Case>::run();
  policy_after<typename Case::policy>::run();
}

} // namespace dingo::matrix::resolution

namespace dingo::matrix {

template <typename Case> void run_resolution() { resolution::run<Case>(); }

} // namespace dingo::matrix
