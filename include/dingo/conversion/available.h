//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/conversion/paths.h>
#include <dingo/conversion/route.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {

namespace detail {

// The target the conversion of Source to Target is planned for. An rvalue
// reference refers to the source, or to what it converts to without a new
// object, where that is available; otherwise to a new object made of the
// source, such as a handle it is boxed into.
template <typename Target, typename Source, typename Access>
using conversion_target_t = std::conditional_t<
    !std::is_rvalue_reference_v<Target> ||
        is_type_conversion_available_v<Target, Source, Access>,
    Target, std::remove_reference_t<Target>>;

template <typename Target, typename Source, typename Access = consume>
using conversion_resolution = resolution<
    Target,
    type_resolution<Source, type_conversion_path_t<
                                conversion_target_t<Target, Source, Access>,
                                Source, Access>>>;

// Whether what an rvalue reference of Target refers to can be moved out, as
// what takes it may: not an abstract interface, which is moved out of only
// as a value of it, which would slice it.
template <typename Target>
inline constexpr bool movable_referent_v =
    !std::is_abstract_v<remove_cvref_t<Target>> &&
    std::is_move_constructible_v<remove_cvref_t<Target>>;

// Whether a conversion of Source to Target is available. A target whose
// object cannot exist, such as an optional of an abstract interface a request
// is rebound to, has none. An rvalue reference refers to what the request
// owns, which can be moved out: its conversion consumes the source, rather
// than borrowing from it, such as the element a shared handle shares.
template <typename Target, typename Source, typename Access>
inline constexpr bool resolution_available_v = [] {
  if constexpr (is_wrapper_formable_v<
                    std::remove_pointer_t<remove_cvref_t<Target>>>) {
    using target = conversion_target_t<Target, Source, Access>;
    if constexpr (std::is_rvalue_reference_v<Target> &&
                  !movable_referent_v<Target>) {
      return false;
    } else if constexpr (!is_type_conversion_available_v<target, Source,
                                                         Access> ||
                         !std::is_rvalue_reference_v<Target>) {
      return is_type_conversion_available_v<target, Source, Access>;
    } else {
      return !access_satisfies<
          borrow, typename type_conversion_path_t<
                      target, Source, Access>::required_access>::value;
    }
  } else {
    return false;
  }
}();

// The resolution of Target from Source, as a list of one when a conversion
// is available and of none otherwise. The resolution is named only when it is
// available: naming it probes the conversion, which instantiates the target.
template <typename Target, typename Source, typename Access,
          bool = resolution_available_v<Target, Source, Access>>
struct available_resolution {
  using type = type_list<>;
};

template <typename Target, typename Source, typename Access>
struct available_resolution<Target, Source, Access, true> {
  using type = type_list<conversion_resolution<Target, Source, Access>>;
};

// An unchanged wrapper is not probed.
template <bool Rebound, typename Target, typename Source, typename Access>
struct wrapper_resolution_impl {
  using type = type_list<>;
};

template <typename Target, typename Source, typename Access>
struct wrapper_resolution_impl<true, Target, Source, Access>
    : available_resolution<Target, Source, Access> {};
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
