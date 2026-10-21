//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The layer kinds and wrapper traits that make the user wrappers of the route
// probes layers of the next implementation. Each wrapper is of a kind of the
// user, declared as the library declares its own, with the properties and
// forms of the kind it stands in for; see user_kinds.py.

#include "probe_types.h"

#include <dingo/type/type_traits.h>

#include <cstdint>
#include <memory>
#include <utility>

namespace dingo {

struct probe_user_family {};

struct user_unique_kind {};
struct user_shared_kind {};
struct user_optional_kind {};

// A sole-owner handle, which shares its element with the user's sharing
// handle and adopts an address.
template <> struct layer_kind_traits<user_unique_kind> {
  static constexpr std::uint16_t properties =
      layer_property::owning | layer_property::nullable |
      layer_property::borrowable | layer_property::constructible |
      layer_property::wrapper;
  using forms = type_list<kind_form<route_form::borrow>,
                          kind_form<route_form::share, user_shared_kind>,
                          kind_form<route_form::adopt, pointer_kind>,
                          kind_form<route_form::upcast_consumed>>;
};

// A sharing handle.
template <> struct layer_kind_traits<user_shared_kind> {
  static constexpr std::uint16_t properties =
      layer_property::owning | layer_property::sharing |
      layer_property::nullable | layer_property::borrowable |
      layer_property::constructible | layer_property::copyable |
      layer_property::wrapper;
  using forms = type_list<
      kind_form<route_form::borrow>, kind_form<route_form::adopt, pointer_kind>,
      kind_form<route_form::upcast_consumed>,
      kind_form<route_form::upcast_copied>, kind_form<route_form::retain>>;
};

// An optional element, held in place.
template <> struct layer_kind_traits<user_optional_kind> {
  static constexpr std::uint16_t properties =
      layer_property::owning | layer_property::nullable |
      layer_property::borrowable | layer_property::in_place |
      layer_property::constructible | layer_property::wrapper;
  using forms =
      type_list<kind_form<route_form::borrow>, kind_form<route_form::take>,
                kind_form<route_form::wrap>>;
};

template <typename T> struct wrapper_traits<user_shared<T>> {
  using kind = user_shared_kind;
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

  template <typename Deleter>
  static user_shared<T> adopt(T *element, Deleter deleter) {
    return user_shared<T>(std::shared_ptr<T>(element, std::move(deleter)));
  }
};

template <typename T> struct wrapper_traits<user_unique<T>> {
  using kind = user_unique_kind;
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
  using kind = user_optional_kind;
  using family = probe_user_family;
  using arguments = type_list<T>;
  template <typename U> using rebind = user_optional<U>;

  static T &element(user_optional<T> &wrapper) { return *wrapper.get(); }

  template <typename... Args> static user_optional<T> make(Args &&...args) {
    return user_optional<T>(std::in_place, std::forward<Args>(args)...);
  }
};

} // namespace dingo
