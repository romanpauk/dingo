//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>

namespace dingo {

// How long what a construction makes is used: during the construction that
// requested it (ephemeral), by the caller of the container after the
// resolution has ended (caller), or for the lifetime of the container
// (persistent).
enum class construction_lifetime {
  ephemeral,
  caller,
  persistent,
};

class construction_scope {
public:
  explicit constexpr construction_scope(construction_lifetime lifetime)
      : lifetime_(lifetime) {}

  constexpr construction_lifetime lifetime() const noexcept {
    return lifetime_;
  }

  constexpr bool is_persistent() const noexcept {
    return lifetime_ == construction_lifetime::persistent;
  }

  constexpr bool is_caller() const noexcept {
    return lifetime_ == construction_lifetime::caller;
  }

private:
  construction_lifetime lifetime_;
};

// A scope known as the code compiles: what depends on it is decided at compile
// time. It is a construction_scope where a scope is only known at run time.
template <construction_lifetime Lifetime> struct fixed_construction_scope {
  static constexpr construction_lifetime lifetime_value = Lifetime;

  constexpr construction_lifetime lifetime() const noexcept { return Lifetime; }

  constexpr bool is_persistent() const noexcept {
    return Lifetime == construction_lifetime::persistent;
  }

  constexpr bool is_caller() const noexcept {
    return Lifetime == construction_lifetime::caller;
  }

  constexpr operator construction_scope() const noexcept {
    return construction_scope(Lifetime);
  }
};

template <typename Scope>
struct is_fixed_construction_scope : std::false_type {};

template <construction_lifetime Lifetime>
struct is_fixed_construction_scope<fixed_construction_scope<Lifetime>>
    : std::true_type {};

// Whether a scope is known, as the code compiles, to be the caller's.
template <typename Scope>
inline constexpr bool is_fixed_caller_scope_v = [] {
  if constexpr (is_fixed_construction_scope<Scope>::value) {
    return Scope::lifetime_value == construction_lifetime::caller;
  } else {
    return false;
  }
}();

inline constexpr fixed_construction_scope<construction_lifetime::ephemeral>
    ephemeral_scope{};
inline constexpr fixed_construction_scope<construction_lifetime::caller>
    caller_scope{};
inline constexpr fixed_construction_scope<construction_lifetime::persistent>
    persistent_scope{};

} // namespace dingo
