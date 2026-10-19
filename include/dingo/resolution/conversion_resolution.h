//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/resolution/resolution.h>
#include <dingo/type/access.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {

namespace detail {

template <typename Target>
using conversion_target_t =
    std::conditional_t<std::is_rvalue_reference_v<Target>,
                       std::remove_reference_t<Target>, Target>;

template <typename Target, typename Source, typename Access = consume>
using conversion_resolution = resolution<
    Target, type_resolution<conversion_target_t<Target>, Source,
                            type_conversion_path_t<conversion_target_t<Target>,
                                                   Source, Access>>>;

// A conditional expression still names the conversion probe for unchanged
// wrappers, so keep it behind a specialization that is never instantiated.
template <bool Rebound, typename Target, typename Source, typename Access>
struct wrapper_resolution_impl {
  using type = type_list<>;
};

template <typename Target, typename Source, typename Access>
struct wrapper_resolution_impl<true, Target, Source, Access> {
  using type = std::conditional_t<
      is_type_conversion_available_v<Target, Source, Access>,
      type_list<conversion_resolution<Target, Source, Access>>, type_list<>>;
};
} // namespace detail

template <typename Source, typename Interface, typename Access, typename = void>
struct wrapper_resolution_traits {
  using type = type_list<>;
};

template <typename Source, typename Interface, typename Access>
struct wrapper_resolution_traits<
    Source, Interface, Access,
    std::enable_if_t<
        type_traits<std::remove_pointer_t<
            std::remove_cv_t<std::remove_reference_t<Source>>>>::enabled &&
        !type_traits<std::remove_cv_t<Interface>>::enabled &&
        (std::is_same_v<Access, borrow> || std::is_same_v<Access, consume>)>> {
private:
  using source_type =
      std::remove_pointer_t<std::remove_cv_t<std::remove_reference_t<Source>>>;
  using target_type = detail::wrapper_rebind_leaf_t<source_type, Interface>;

public:
  using type = typename detail::wrapper_resolution_impl<
      !std::is_same_v<target_type, source_type>, target_type, Source,
      Access>::type;
};
} // namespace dingo
