//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/exposure/object_resolutions.h>
#include <dingo/exposure/resolutions.h>

namespace dingo {
namespace detail {
template <typename Interface, typename Shape> struct shape_resolutions {
  using value_resolutions =
      typename binding_published_value_resolutions<Interface, Shape>::type;
  using object_resolutions =
      typename binding_object_resolutions<Interface, Shape>::type;
  using rvalue_reference_resolutions =
      typename binding_rvalue_reference_resolutions<Interface, Shape>::type;
  // Target forms are disjoint across the value, object, and rvalue-reference
  // categories, so concatenating their already-unique lists cannot duplicate
  // a resolution.
  using type = type_list_cat_t<value_resolutions, object_resolutions,
                               rvalue_reference_resolutions>;
};

template <typename Interface, typename Storage>
using binding_resolutions =
    shape_resolutions<Interface, storage_shape_t<Storage>>;
} // namespace detail
} // namespace dingo
