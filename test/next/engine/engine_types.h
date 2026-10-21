//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The types of the generated engine cases: leaves with the properties the
// route rules depend on, an implementation of two interfaces, and a user
// wrapper family of each wrapper layer kind.

#include <dingo/core/projection/layer_traits.h>

namespace dingo::next_test {

struct leaf {};

struct move_only_leaf {
  move_only_leaf() = default;
  move_only_leaf(move_only_leaf &&) = default;
  move_only_leaf &operator=(move_only_leaf &&) = default;
};

struct other_leaf {};

struct iface {
  virtual ~iface() = default;
  virtual void run() = 0;
};

struct other_iface {
  virtual ~other_iface() = default;
  virtual void walk() = 0;
};

struct impl : iface, other_iface {
  void run() override {}
  void walk() override {}
};

struct user_family {};

template <typename Type> struct user_unique {
  user_unique() = default;
  user_unique(const user_unique &) = delete;
  user_unique(user_unique &&) = default;
  Type *value = nullptr;
};

template <typename Type> struct user_shared {
  Type *value = nullptr;
};

template <typename Type> struct user_optional {
  Type value;
};

} // namespace dingo::next_test

namespace dingo {

template <typename Type> struct wrapper_traits<next_test::user_unique<Type>> {
  using kind = unique_kind;
  using family = next_test::user_family;
  using arguments = type_list<Type>;
};

template <typename Type> struct wrapper_traits<next_test::user_shared<Type>> {
  using kind = shared_kind;
  using family = next_test::user_family;
  using arguments = type_list<Type>;
};

template <typename Type> struct wrapper_traits<next_test::user_optional<Type>> {
  using kind = optional_kind;
  using family = next_test::user_family;
  using arguments = type_list<Type>;
};

} // namespace dingo
