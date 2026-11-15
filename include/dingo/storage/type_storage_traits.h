//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
namespace detail {
struct no_scope {};
} // namespace detail

namespace detail {
// What a storage resolves, borrowed and consumed. A pointer to what the
// storage keeps is borrowed as a reference to it: it is never null, and a
// conversion of it then starts at the object. Any other pointer is borrowed
// as itself.
template <typename Storage> struct storage_source_traits {
private:
  using stored_value_type = remove_cvref_t<typename Storage::type>;
  using resolved_type = typename Storage::resolved_type;
  using resolved_leaf_type =
      std::conditional_t<std::is_pointer_v<resolved_type>,
                         std::remove_pointer_t<resolved_type>, resolved_type>;
  static constexpr bool addresses_kept =
      std::is_pointer_v<resolved_type> &&
      std::is_same_v<std::remove_cv_t<resolved_leaf_type>, stored_value_type>;

public:
  using borrowed_type =
      std::conditional_t<std::is_pointer_v<resolved_type> && !addresses_kept,
                         resolved_type, resolved_leaf_type &>;
  using consumed_type = resolved_type &&;
};
} // namespace detail

} // namespace dingo
