//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/assertions.h"

#include <memory>

namespace dingo::matrix {

struct cycle_lvalue_reference {};
struct cycle_pointer {};
struct cycle_shared_pointer_value {};
struct cycle_shared_pointer_reference {};
struct cycle_shared_pointer_pointer {};

template <typename Edge, typename Target> struct cycle_edge_traits;

template <typename Target>
struct cycle_edge_traits<cycle_lvalue_reference, Target> {
  using type = Target &;

  static Target *address(Target &dependency) { return &dependency; }
};

template <typename Target> struct cycle_edge_traits<cycle_pointer, Target> {
  using type = Target *;

  static Target *address(Target *dependency) { return dependency; }
};

template <typename Target>
struct cycle_edge_traits<cycle_shared_pointer_value, Target> {
  using type = std::shared_ptr<Target>;

  static Target *address(const type &dependency) { return dependency.get(); }
};

template <typename Target>
struct cycle_edge_traits<cycle_shared_pointer_reference, Target> {
  using type = std::shared_ptr<Target> &;

  static Target *address(type dependency) { return dependency.get(); }
};

template <typename Target>
struct cycle_edge_traits<cycle_shared_pointer_pointer, Target> {
  using type = std::shared_ptr<Target> *;

  static Target *address(type dependency) { return dependency->get(); }
};

template <typename Edge, typename Target>
using cycle_dependency_t = typename cycle_edge_traits<Edge, Target>::type;

template <typename AToBEdge, typename BToAEdge> struct shared_cyclical_b;

template <typename AToBEdge, typename BToAEdge> struct shared_cyclical_a {
  using dependency_type = shared_cyclical_b<AToBEdge, BToAEdge>;
  using edge_traits = cycle_edge_traits<AToBEdge, dependency_type>;

  explicit shared_cyclical_a(typename edge_traits::type init_dependency)
      : dependency(edge_traits::address(init_dependency)) {}

  dependency_type *dependency;
};

template <typename AToBEdge, typename BToAEdge> struct shared_cyclical_b {
  using dependency_type = shared_cyclical_a<AToBEdge, BToAEdge>;
  using edge_traits = cycle_edge_traits<BToAEdge, dependency_type>;

  explicit shared_cyclical_b(typename edge_traits::type init_dependency)
      : dependency(edge_traits::address(init_dependency)) {}

  dependency_type *dependency;
};

struct shared_cyclical_mixed_anchor {};

template <typename Type, bool SharedOwnership, typename Container>
void check_shared_cyclical_storage(Container &container, Type &instance) {
  ASSERT_EQ(&container.template resolve<Type &>(), &instance);
  ASSERT_EQ(container.template resolve<Type *>(), &instance);
  if constexpr (SharedOwnership) {
    auto handle = container.template resolve<std::shared_ptr<Type>>();
    ASSERT_EQ(handle.get(), &instance);
    ASSERT_EQ(container.template resolve<std::shared_ptr<Type> &>().get(),
              &instance);
    ASSERT_EQ(container.template resolve<std::shared_ptr<Type> *>()->get(),
              &instance);
  }
}

template <typename A, typename B, bool ASharedOwnership, bool BSharedOwnership,
          typename Container>
void exercise_shared_cyclical_cycle(Container &container) {
  auto &a = container.template resolve<A &>();
  auto &b = container.template resolve<B &>();

  ASSERT_EQ(a.dependency, &b);
  ASSERT_EQ(b.dependency, &a);
  check_shared_cyclical_storage<A, ASharedOwnership>(container, a);
  check_shared_cyclical_storage<B, BSharedOwnership>(container, b);
}

} // namespace dingo::matrix
