//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What the conversion layer of a binding answers a request, without resolving
// it: whether the request lists of the static container hold it, whether the
// descriptors the runtime container dispatches on select it, and whether the
// conversion of the resolution is available. Nothing of a container is
// instantiated, so many registrations are answered in one program.

#pragma once

#include "model/probe.h"

#include <dingo/runtime/binding.h>
#include <dingo/static/binding.h>

#include <cstddef>
#include <string>
#include <type_traits>

namespace dingo::model {

template <typename Operation> struct conversion_available : std::true_type {};

template <typename Source, typename Conversion>
struct conversion_available<detail::type_resolution<Source, Conversion>>
    : std::bool_constant<Conversion::available> {};

// The index of the resolution the runtime container dispatches a request to,
// or the number of resolutions when none matches.
template <typename Request, typename... Resolutions>
constexpr std::size_t runtime_index(type_list<Resolutions...>) {
  constexpr bool matches[] = {detail::matches_resolution_request<Resolutions>(
                                  detail::described_type_v<Request>)...,
                              true};
  std::size_t index = 0;
  while (!matches[index]) {
    ++index;
  }
  return index;
}

// The derivation the runtime container selects for a request its binding
// does not publish, from the facts the binding tells of itself.
template <typename Request, typename Storage, typename Resolutions>
constexpr detail::derivation runtime_derivation() {
  if constexpr (!detail::derivable_v<Request>) {
    return detail::derivation::none;
  } else {
    return detail::derive_request<Request>(
        detail::runtime_derivation_facts<Storage, Resolutions>(
            detail::described_type_v<Request>));
  }
}

// Whether the two containers resolve a request alike, down the chain of the
// requests it is derived from: both publish it, or both derive it the same
// way from a request they resolve alike, or neither resolves it.
template <typename Request, typename Binding, typename Storage,
          typename Resolutions>
constexpr bool derives_alike() {
  constexpr bool by_static_list =
      detail::binding_supports_request_v<Request, Binding>;
  constexpr bool by_runtime_list =
      runtime_index<Request>(Resolutions{}) < type_list_size_v<Resolutions>;
  if constexpr (by_static_list != by_runtime_list) {
    return false;
  } else if constexpr (by_static_list) {
    return true;
  } else {
    constexpr detail::derivation by_static =
        detail::static_derivation_v<Request, Binding>;
    constexpr detail::derivation by_runtime =
        runtime_derivation<Request, Storage, Resolutions>();
    if constexpr (by_static != by_runtime) {
      return false;
    } else if constexpr (by_static == detail::derivation::none) {
      return true;
    } else {
      return derives_alike<detail::derived_request_t<by_static, Request>,
                           Binding, Storage, Resolutions>();
    }
  }
}

template <std::size_t Index, typename Resolutions>
struct resolution_at_available : std::false_type {};

template <std::size_t Index, typename Head, typename... Tail>
struct resolution_at_available<Index, type_list<Head, Tail...>>
    : resolution_at_available<Index - 1, type_list<Tail...>> {};

template <typename Head, typename... Tail>
struct resolution_at_available<0, type_list<Head, Tail...>>
    : conversion_available<typename Head::operation> {};

// Records a request of Interface the binding of a registration answers:
// published when its resolution is selected and its conversion available,
// unconvertible when the conversion is not, composed when it is a wrapper the
// binding makes of a value it resolves, unpublished when it is none of
// these, and diverged when the two containers select differently. A request
// the lookup sends to the binding of another interface is answered there.
template <typename Model, typename Interface, typename Request>
void publication(const std::string &prefix, const char *name) {
  using Storage = typename Model::storage_type;
  using type = unannotated_t<Interface>;
  using resolutions = typename detail::binding_resolutions<type, Storage>::type;
  using selected = typename detail::matching_binding_resolution<
      Request, typename detail::request_binding_resolutions<
                   Request, type, Storage>::type>::type;
  constexpr std::size_t index = runtime_index<Request>(resolutions{});
  constexpr bool listed = !std::is_void_v<selected>;
  constexpr bool matched = index < type_list_size_v<resolutions>;
  const std::string cell = prefix + name;
  if constexpr (!selects_v<Request, type>) {
    record(cell.c_str(), "selects_another", nullptr);
  } else if constexpr (listed != matched) {
    record(cell.c_str(), "diverged",
           listed ? "the static container selects a resolution, the runtime "
                    "container none"
                  : "the runtime container selects a resolution, the static "
                    "container none");
  } else if constexpr (!listed) {
    // How the static container derives a request it does not publish: kept
    // with the instance or composed, taken out of a pointer, or into the
    // context of the resolution.
    using binding = detail::binding<type, Model>;
    constexpr detail::derivation how =
        detail::static_request_derivation_v<Request, binding>;
    if constexpr (!derives_alike<Request, binding, Storage, resolutions>()) {
      record(cell.c_str(), "diverged",
             "the static and the runtime container derive the request "
             "differently");
    } else if constexpr (how == detail::derivation::keep ||
                         how == detail::derivation::compose) {
      record(cell.c_str(), "composed", nullptr);
    } else if constexpr (how == detail::derivation::take) {
      record(cell.c_str(), "adopted", nullptr);
    } else if constexpr (how == detail::derivation::adopt ||
                         how == detail::derivation::place) {
      record(cell.c_str(), "placed", nullptr);
    } else {
      record(cell.c_str(), "unpublished", nullptr);
    }
  } else if constexpr (!conversion_available<
                           typename selected::operation>::value ||
                       !resolution_at_available<index, resolutions>::value) {
    record(cell.c_str(), "unconvertible", nullptr);
  } else {
    record(cell.c_str(), "published", nullptr);
  }
}

} // namespace dingo::model
