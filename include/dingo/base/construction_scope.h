//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

namespace dingo {

enum class construction_lifetime {
  ephemeral,
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

private:
  construction_lifetime lifetime_;
};

inline constexpr construction_scope ephemeral_scope{
    construction_lifetime::ephemeral};
inline constexpr construction_scope persistent_scope{
    construction_lifetime::persistent};

} // namespace dingo
