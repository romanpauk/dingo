//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/type/rebind_type.h of the next implementation: the leaf of a type and
// the type with its leaf rebound, through its wrappers, and the marker of a
// request that names its wrapper exactly.

#include <dingo/type/wrapper_traits.h>

namespace dingo {

template <typename Type> struct leaf_type {
  using type = leaf_of_t<Type>;
};

template <typename Type> using leaf_type_t = typename leaf_type<Type>::type;

template <typename Type, typename Leaf> struct rebind_type {
  using type = replace_leaf_t<Type, Leaf>;
};

template <typename Type, typename Leaf>
using rebind_type_t = typename rebind_type<Type, Leaf>::type;

// A request of `Type` that names the wrapper exactly; the routes of the next
// implementation always match the requested type tree exactly.
template <typename Type> struct exact_lookup {
  using type = Type;
};

} // namespace dingo
