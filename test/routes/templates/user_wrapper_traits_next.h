//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The wrapper traits that make the user wrappers of the route probes layers of
// the next implementation.

#include "probe_types.h"

#include <dingo/type/type_traits.h>

#include <memory>
#include <utility>

namespace dingo {

struct probe_user_family {};

template <typename T> struct wrapper_traits<user_shared<T>> {
  static constexpr layer_kind kind = layer_kind::share;
  using family = probe_user_family;
  using arguments = type_list<T>;
  template <typename U> using rebind = user_shared<U>;

  static T &element(const user_shared<T> &wrapper) { return *wrapper.get(); }

  template <typename... Args> static user_shared<T> make(Args &&...args) {
    return user_shared<T>(std::make_shared<T>(std::forward<Args>(args)...));
  }

  static user_shared<T> adopt(T *element) {
    return user_shared<T>(std::shared_ptr<T>(element));
  }
};

template <typename T> struct wrapper_traits<user_unique<T>> {
  static constexpr layer_kind kind = layer_kind::own;
  using family = probe_user_family;
  using arguments = type_list<T>;
  template <typename U> using rebind = user_unique<U>;

  static T &element(const user_unique<T> &wrapper) { return *wrapper.get(); }

  template <typename... Args> static user_unique<T> make(Args &&...args) {
    return user_unique<T>(std::make_unique<T>(std::forward<Args>(args)...));
  }

  static user_unique<T> adopt(T *element) {
    return user_unique<T>(std::unique_ptr<T>(element));
  }

  static user_shared<T> share(user_unique<T> &&value) {
    return user_shared<T>(std::move(value));
  }
};

template <typename T> struct wrapper_traits<user_optional<T>> {
  static constexpr layer_kind kind = layer_kind::opt;
  using family = probe_user_family;
  using arguments = type_list<T>;
  template <typename U> using rebind = user_optional<U>;

  static T &element(user_optional<T> &wrapper) { return *wrapper.get(); }

  template <typename... Args> static user_optional<T> make(Args &&...args) {
    return user_optional<T>(std::in_place, std::forward<Args>(args)...);
  }
};

} // namespace dingo
