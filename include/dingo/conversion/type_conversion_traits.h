//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/conversion/candidates.h>
#include <dingo/conversion/conversions.h>
#include <dingo/conversion/direct_paths.h>
#include <dingo/conversion/indirect_candidates.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <std::size_t Index, typename Target, typename Source, typename Access,
          typename Direct>
struct conversion_fallback_candidate;

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<0, Target, Source, Access, Direct> {
  using type =
      typename target_alternative_type_conversion_candidate<Target, Source,
                                                            Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<1, Target, Source, Access, Direct> {
  using type =
      typename source_alternative_type_conversion<Target, Source, Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<2, Target, Source, Access, Direct> {
  using type = typename target_wrapper_type_conversion_candidate<Target, Source,
                                                                 Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<3, Target, Source, Access, Direct> {
  using type = typename array_type_conversion_candidate<Target, Source>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<4, Target, Source, Access, Direct> {
  using type = std::conditional_t<Direct::can_take_address,
                                  address_type_conversion<Target, Source>,
                                  unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<5, Target, Source, Access, Direct> {
  using type =
      typename pointer_access_type_conversion_candidate<Target, Source>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<6, Target, Source, Access, Direct> {
  using type = typename dereference_type_conversion_candidate<Target, Source,
                                                              Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<7, Target, Source, Access, Direct> {
  using type =
      typename borrowed_type_conversion_candidate<Target, Source, Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<8, Target, Source, Access, Direct> {
  using type =
      typename retained_type_conversion_candidate<Target, Source, Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<9, Target, Source, Access, Direct> {
  using type = unavailable_type_conversion;
};

// Returns the first candidate at or after Index whose necessary conditions
// hold. These are necessary conditions only: the candidate still performs its
// complete validity check before it can win. Skipped candidates cannot be
// available, so the walk never has to instantiate them.
template <std::size_t Index, typename Target, typename Source, typename Direct>
constexpr std::size_t first_conversion_fallback() {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using target_array = std::conditional_t<std::is_pointer_v<Target>,
                                          std::remove_pointer_t<Target>,
                                          std::remove_reference_t<Target>>;

  if constexpr (Index <= 0 && !std::is_reference_v<Target> &&
                !std::is_pointer_v<Target> &&
                is_alternative_type_v<target_type>) {
    return 0;
  } else if constexpr (Index <= 1 && is_alternative_type_v<source_type> &&
                       !std::is_volatile_v<std::remove_reference_t<Source>>) {
    return 1;
  } else if constexpr (Index <= 2 && !std::is_reference_v<Target> &&
                       !std::is_pointer_v<Target> &&
                       is_value_wrapper_type_v<target_type>) {
    return 2;
  } else if constexpr (Index <= 3 &&
                       std::is_pointer_v<std::remove_reference_t<Source>> &&
                       ((std::is_lvalue_reference_v<Target> &&
                         std::is_array_v<std::remove_reference_t<Target>>) ||
                        (std::is_pointer_v<Target> &&
                         std::is_array_v<target_array>))) {
    return 3;
  } else if constexpr (Index <= 4 && Direct::can_take_address) {
    return 4;
  } else if constexpr (Index <= 5 && std::is_pointer_v<Target> &&
                       std::is_lvalue_reference_v<Source>) {
    return 5;
  } else if constexpr (Index <= 6 &&
                       std::is_pointer_v<std::remove_reference_t<Source>> &&
                       !std::is_void_v<std::remove_pointer_t<
                           std::remove_reference_t<Source>>> &&
                       !std::is_pointer_v<Target>) {
    return 6;
  } else if constexpr (Index <= 7 &&
                       type_traits<source_type>::is_value_borrowable) {
    return 7;
  } else if constexpr (Index <= 8 &&
                       (std::is_lvalue_reference_v<Target> ||
                        std::is_pointer_v<Target>) &&
                       type_traits<
                           retained_conversion_type_t<Target>>::enabled &&
                       !std::is_array_v<retained_conversion_type_t<Target>>) {
    return 8;
  } else {
    return 9;
  }
}

// Keep later recursive candidates incomplete until every earlier conversion
// has failed; merely naming all candidates instantiates all of their paths.
template <std::size_t Index, typename Target, typename Source, typename Access,
          typename Direct,
          typename Candidate = conversion_for_access_t<
              typename conversion_fallback_candidate<Index, Target, Source,
                                                     Access, Direct>::type,
              Access>,
          bool Available = Candidate::available>
struct select_conversion_fallback {
  using type = Candidate;
};

template <std::size_t Index, typename Target, typename Source, typename Access,
          typename Direct, typename Candidate>
struct select_conversion_fallback<Index, Target, Source, Access, Direct,
                                  Candidate, false> {
  using type = typename select_conversion_fallback<
      first_conversion_fallback<Index + 1, Target, Source, Direct>(), Target,
      Source, Access, Direct>::type;
};

template <typename Target, typename Source, typename Access, typename Direct,
          typename Candidate>
struct select_conversion_fallback<9, Target, Source, Access, Direct, Candidate,
                                  false> {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct type_conversion_path_impl<Target, Source, Access, Direct, true> {
public:
  using type = typename select_conversion_fallback<
      first_conversion_fallback<0, Target, Source, Direct>(), Target, Source,
      Access, Direct>::type;
  static constexpr bool available = type::available;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct type_conversion_path_impl<Target, Source, Access, Direct, false>
    : Direct {};

template <typename Target, typename Source, typename Access, typename Basic>
struct type_conversion_path<Target, Source, Access, Basic, true> : Basic {};

template <typename Target, typename Source, typename Access, typename Basic>
struct type_conversion_path<Target, Source, Access, Basic, false>
    : type_conversion_path_impl<
          Target, Source, Access,
          conversion_direct_path_t<Target, Source, Access>> {};

template <typename Target, typename Source, typename Access = consume>
using type_conversion_path_t =
    typename type_conversion_path<Target, Source, Access>::type;

template <typename Target, typename Source, typename Access = consume>
inline constexpr bool is_type_conversion_available_v =
    type_conversion_path<Target, Source, Access>::available;

template <typename Target, typename Source, typename Access = consume>
struct is_type_conversion_available
    : std::bool_constant<
          is_type_conversion_available_v<Target, Source, Access>> {};
} // namespace detail
} // namespace dingo
