//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What a binding plans for a request, without resolving it: whether it plans
// the request for as long as the container, which the caller may take, only
// for as long as the request, or not at all. Both containers plan a request
// alike, so one answer is of both. Nothing of a container is instantiated,
// so many registrations are answered in one program.

#pragma once

#include "model/probe.h"

#include <dingo/static/binding.h>

namespace dingo::model {

// Records the outcome of a request of Interface the binding of a
// registration answers, as the code compiles: published when it plans the
// request for as long as the container, placed when only for as long as the
// request, unpublished when not at all. A request the lookup sends to the
// binding of another interface is answered there.
template <typename Model, typename Interface, typename Request, typename Leaf>
DINGO_ALWAYS_INLINE void publication(const char *prefix, const char *name) {
  using type = unannotated_t<Interface>;
  using binding = detail::binding<type, Model>;
  using planning = detail::static_execution<Request, binding>;
  (void)sizeof(Leaf);
  if constexpr (!selects_v<Request, type>) {
    record(prefix, name, "selects_another", nullptr);
  } else if constexpr (planning::template planned<true>().found) {
    record(prefix, name, "published", nullptr);
  } else if constexpr (planning::template planned<false>().found) {
    record(prefix, name, "placed", nullptr);
  } else {
    record(prefix, name, "unpublished", nullptr);
  }
}

} // namespace dingo::model
