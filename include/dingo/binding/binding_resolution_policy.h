//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/binding/binding_selection.h>

#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo::detail {

enum class binding_resolution_policy {
  prefer_primary,
  ambiguous_on_conflict,
};

enum class binding_result {
  primary,
  secondary,
  missing,
  ambiguous,
};

constexpr binding_result resolve_binding(binding_status primary,
                                         binding_status secondary,
                                         binding_resolution_policy policy) {
  const bool primary_ambiguous = primary == binding_status::ambiguous;
  const bool secondary_ambiguous = secondary == binding_status::ambiguous;
  const bool primary_found = primary == binding_status::found;
  const bool secondary_found = secondary == binding_status::found;

  if (policy == binding_resolution_policy::prefer_primary) {
    if (primary_ambiguous) {
      return binding_result::ambiguous;
    }

    if (primary_found) {
      return binding_result::primary;
    }

    if (secondary_ambiguous) {
      return binding_result::ambiguous;
    }

    if (secondary_found) {
      return binding_result::secondary;
    }

    return binding_result::missing;
  }

  if (primary_ambiguous || secondary_ambiguous ||
      (primary_found && secondary_found)) {
    return binding_result::ambiguous;
  }

  if (primary_found) {
    return binding_result::primary;
  }

  if (secondary_found) {
    return binding_result::secondary;
  }

  return binding_result::missing;
}

constexpr binding_status binding_status_from_result(binding_result resolution) {
  switch (resolution) {
  case binding_result::primary:
  case binding_result::secondary:
    return binding_status::found;
  case binding_result::ambiguous:
    return binding_status::ambiguous;
  case binding_result::missing:
  default:
    return binding_status::not_found;
  }
}

template <binding_status SecondaryStatus>
constexpr binding_status
resolve_binding_status(binding_status primary,
                       binding_resolution_policy policy) {
  return binding_status_from_result(
      resolve_binding(primary, SecondaryStatus, policy));
}

// Resolves a request that two sources may answer. The primary source selected
// at run time; the status of the secondary is known at compile time. The policy
// decides which side wins, or whether both finding a binding is an error.
template <binding_resolution_policy Policy, binding_status SecondaryStatus,
          typename R, typename Request, typename PrimarySelection,
          typename Context, typename PrimaryResolve, typename SecondaryResolve,
          typename Missing>
R resolve_combined_selection(const PrimarySelection &primary, Context &context,
                             PrimaryResolve &&resolve_primary,
                             SecondaryResolve &&resolve_secondary,
                             Missing &&missing) {
  const auto result = resolve_binding(primary.status, SecondaryStatus, Policy);
  if (result == binding_result::primary) {
    return std::forward<PrimaryResolve>(resolve_primary)();
  }
  if constexpr (SecondaryStatus == binding_status::found) {
    if (result == binding_result::secondary) {
      return std::forward<SecondaryResolve>(resolve_secondary)();
    }
  }
  if (result == binding_result::missing) {
    return std::forward<Missing>(missing)();
  }
  throw make_type_ambiguous_exception<Request>(context);
}

} // namespace dingo::detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif
