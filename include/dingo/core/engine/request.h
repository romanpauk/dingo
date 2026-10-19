//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The descriptor of a request type.

#include <dingo/core/engine/derive.h>
#include <dingo/core/projection/layer_traits.h>

#include <type_traits>

namespace dingo::core {

template <typename Request> struct request_traits {
  static constexpr request_kind kind =
      std::is_lvalue_reference_v<Request>   ? request_kind::lvalue_reference
      : std::is_rvalue_reference_v<Request> ? request_kind::rvalue_reference
                                            : request_kind::value;
  // The type the request asks for, which a term of its shape provides.
  using value_type = canonical_t<std::remove_reference_t<Request>>;
  static constexpr request_descriptor descriptor{
      kind, shape_v<std::remove_reference_t<Request>>};
};

} // namespace dingo::core
