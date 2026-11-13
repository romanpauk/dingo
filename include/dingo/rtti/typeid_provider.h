//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/rtti/rtti.h>

#include <typeindex>

namespace dingo {

template <> class rtti<typeid_provider> {
  // typeid of the wrapper keeps the cv and reference qualifiers of T.
  template <typename T> struct wrapper {};

public:
  using type_index = std::type_index;

  template <typename T> static type_index get_type_index() {
    return typeid(wrapper<T>);
  }
};

} // namespace dingo
