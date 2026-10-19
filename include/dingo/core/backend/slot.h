//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The slot of a binding, selected by the slot kind of its scope descriptor.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/slot.h>
#include <dingo/core/projection/construct.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <utility>

namespace dingo::core {

template <typename Binding, slot Kind = info(Binding::scope).storage_slot>
class binding_slot {
  static_assert(dependent_false<Binding>,
                "the next implementation does not provide this scope yet");
};

// A lazily constructed object, made by the default factory on first use.
template <typename Binding> class binding_slot<Binding, slot::lazy> {
public:
  using stored_type = typename Binding::stored_type;

  stored_type &get() {
    return slot_.get_or_construct(
        [](void *where) { construct_default<stored_type>(where); });
  }

private:
  lazy_slot<stored_type> slot_;
};

// An object supplied by the caller at registration.
template <typename Binding> class binding_slot<Binding, slot::external_ref> {
public:
  using stored_type = typename Binding::stored_type;

  template <typename Instance>
  explicit binding_slot(Instance &&instance)
      : slot_(std::forward<Instance>(instance)) {}

  auto &get() { return slot_.get(); }

private:
  external_slot<stored_type> slot_;
};

} // namespace dingo::core
