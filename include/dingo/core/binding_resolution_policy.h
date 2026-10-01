//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_selection.h>
#include <dingo/core/exceptions.h>

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

template <typename ErrorRequest, typename ResolveRequest = ErrorRequest,
          typename Context, typename Sources>
ResolveRequest resolve_from_binding_sources(Context &context,
                                            Sources &sources) {
  auto selection = sources.select();
  if (selection.ambiguous()) {
    throw make_type_ambiguous_exception<ErrorRequest>(context);
  }

  return sources.template resolve<ResolveRequest>(context, selection);
}

template <typename SelectedSource, typename MissingSource>
struct selected_binding_sources {
  SelectedSource &selected;
  MissingSource &missing;

  decltype(auto) select() { return selected.select(); }

  template <typename Request, typename Context, typename Selection>
  decltype(auto) resolve(Context &context, Selection selection) {
    if (selection.found()) {
      return selected.template resolve<Request>(context, selection);
    }
    return missing.template resolve<Request>(context);
  }
};

template <typename SelectedSource, typename MissingSource>
selected_binding_sources<SelectedSource, MissingSource>
make_selected_binding_sources(SelectedSource &selected,
                              MissingSource &missing) {
  return {selected, missing};
}

} // namespace dingo::detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif
