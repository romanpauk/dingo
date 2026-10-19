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

// A lazily constructed object, made by the factory on first use, whose
// dependencies the resolver resolves.
template <typename Binding> class binding_slot<Binding, slot::lazy> {
  static_assert(info(Binding::scope).guard == recursion_guard::while_unresolved,
                "a lazy slot guards the recursion while it is unresolved");

public:
  using stored_type = typename Binding::stored_type;

  template <typename Resolver> stored_type &get(Resolver &resolver) {
    return slot_.get_or_construct([&resolver](void *where) {
      construct_stored<stored_type>(where, resolver);
    });
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

  template <typename Resolver> auto &get(Resolver & /*resolver*/) {
    return slot_.get();
  }

private:
  external_slot<stored_type> slot_;
};

} // namespace dingo::core
