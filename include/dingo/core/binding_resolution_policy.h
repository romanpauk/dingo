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

struct binding_source_selection {
  binding_result result;

  constexpr bool found() const {
    return result == binding_result::primary ||
           result == binding_result::secondary;
  }

  constexpr bool ambiguous() const {
    return result == binding_result::ambiguous;
  }

  constexpr bool primary() const { return result == binding_result::primary; }

  constexpr bool secondary() const {
    return result == binding_result::secondary;
  }
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

  if (selection.found()) {
    return sources.template resolve_selected<ResolveRequest>(context,
                                                             selection);
  }

  return sources.template resolve_missing<ResolveRequest>(context);
}

template <typename PrimarySource, typename SecondarySource,
          typename MissingSource>
struct two_binding_sources {
  PrimarySource &primary;
  SecondarySource &secondary;
  MissingSource &missing;
  binding_resolution_policy policy;

  binding_source_selection select() {
    return {resolve_binding(primary.status(), secondary.status(), policy)};
  }

  template <typename Request, typename Context>
  decltype(auto) resolve_selected(Context &context,
                                  binding_source_selection selection) {
    if constexpr (PrimarySource::can_resolve) {
      if (selection.primary()) {
        return primary.template resolve<Request>(context);
      }
    }

    if constexpr (SecondarySource::can_resolve) {
      if (selection.secondary()) {
        return secondary.template resolve<Request>(context);
      }
    }

    return missing.template resolve<Request>(context);
  }

  template <typename Request, typename Context>
  decltype(auto) resolve_missing(Context &context) {
    return missing.template resolve<Request>(context);
  }
};

template <typename PrimarySource, typename SecondarySource,
          typename MissingSource>
two_binding_sources<PrimarySource, SecondarySource, MissingSource>
make_two_binding_sources(PrimarySource &primary, SecondarySource &secondary,
                         MissingSource &missing,
                         binding_resolution_policy policy) {
  return {primary, secondary, missing, policy};
}

template <typename SelectedSource, typename MissingSource>
struct selected_binding_sources {
  SelectedSource &selected;
  MissingSource &missing;

  decltype(auto) select() { return selected.select(); }

  template <typename Request, typename Context, typename Selection>
  decltype(auto) resolve_selected(Context &context, Selection selection) {
    return selected.template resolve<Request>(context, selection);
  }

  template <typename Request, typename Context>
  decltype(auto) resolve_missing(Context &context) {
    return missing.template resolve<Request>(context);
  }
};

template <typename SelectedSource, typename MissingSource>
selected_binding_sources<SelectedSource, MissingSource>
make_selected_binding_sources(SelectedSource &selected,
                              MissingSource &missing) {
  return {selected, missing};
}

template <typename LookupRequest> struct missing_binding_source {
  template <typename ResolveRequest, typename Context>
  ResolveRequest resolve(Context &context) {
    (void)context;
    throw make_type_not_found_exception<LookupRequest>();
  }

  template <typename ResolveRequest, typename Context>
  ResolveRequest resolve_missing(Context &context) {
    (void)context;
    throw make_type_not_found_exception<LookupRequest>();
  }
};

} // namespace dingo::detail

#ifdef _MSC_VER
#pragma warning(pop)
#endif
