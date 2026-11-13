//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// What the emptiness check probes: a registration whose stored object holds
// no leaf, and a request the specification says is defined when a layer of
// it is empty, which has to give what holds no leaf, without failing.

#include "model/probe.h"

#include <exception>
#include <utility>
#include <variant>

namespace dingo::model {

// A stored object that holds no leaf: its outermost layer empty, as a null
// handle, an empty optional or an alternative type that holds the other
// alternative, or, with Inner, its outermost layer holding an empty one.
template <typename S, bool Inner> struct empty_stored {
  static S make() {
    if constexpr (Inner) {
      using element = typename type_traits<S>::value_type;
      return type_traits<S>::emplace(empty_stored<element, false>::make());
    } else if constexpr (is_alternative_type_v<S>) {
      return S(std::in_place_type<other>);
    } else {
      return S{};
    }
  }
};

// Whether a result holds a leaf: refers to one, points to one, or has a
// layer that holds one.
template <typename T> bool holds_leaf(const T &value) {
  using type = std::remove_cv_t<T>;
  if constexpr (std::is_pointer_v<type>) {
    return value != nullptr && holds_leaf(*value);
  } else if constexpr (std::is_base_of_v<leaf_interface, type>) {
    return true;
  } else if constexpr (is_alternative_type_v<type>) {
    return std::visit([](const auto &held) { return holds_leaf(held); }, value);
  } else if constexpr (type_traits<type>::enabled) {
    auto *element = type_traits<type>::get(value);
    return element != nullptr && holds_leaf(*element);
  } else {
    return false;
  }
}

// Resolves a request through invoke and records whether it gives what holds
// no leaf, or what it throws.
template <typename Request, typename Container>
void empty_probe(const char *cell, Container &container) {
  try {
    const bool leaf =
        container.invoke([](Request request) { return holds_leaf(request); });
    if (leaf) {
      record(cell, "violated", "the result holds a leaf");
    } else {
      record(cell, "defined");
    }
  } catch (const dingo::type_not_convertible_exception &) {
    record(cell, "type_not_convertible");
  } catch (const std::exception &error) {
    record(cell, "exception", error.what());
  }
}

} // namespace dingo::model
