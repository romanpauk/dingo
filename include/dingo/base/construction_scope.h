//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

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

inline constexpr construction_scope ephemeral_scope{
    construction_lifetime::ephemeral};
inline constexpr construction_scope caller_scope{construction_lifetime::caller};
inline constexpr construction_scope persistent_scope{
    construction_lifetime::persistent};

} // namespace dingo
