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

template <std::size_t Index, typename Resolutions>
struct resolution_at_available : std::false_type {};

template <std::size_t Index, typename Head, typename... Tail>
struct resolution_at_available<Index, type_list<Head, Tail...>>
    : resolution_at_available<Index - 1, type_list<Tail...>> {};

template <typename Head, typename... Tail>
struct resolution_at_available<0, type_list<Head, Tail...>>
    : conversion_available<typename Head::operation> {};

// Records a request of Interface the binding of a storage answers: published
// when its resolution is selected and its conversion available, unconvertible
// when the conversion is not, unpublished when no resolution is selected, and
// diverged when the two containers select differently. A request the lookup
// sends to the binding of another interface is answered there.
template <typename Storage, typename Interface, typename Request>
void publication(const std::string &prefix, const char *name) {
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
    record(cell.c_str(), "unpublished", nullptr);
  } else if constexpr (!conversion_available<
                           typename selected::operation>::value ||
                       !resolution_at_available<index, resolutions>::value) {
    record(cell.c_str(), "unconvertible", nullptr);
  } else {
    record(cell.c_str(), "published", nullptr);
  }
}

} // namespace dingo::model
