//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The slot of a binding, selected by the slot kind of its scope descriptor.

#include <dingo/core/base/config.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/exceptions.h>
#include <dingo/core/primitives/slot.h>
#include <dingo/core/projection/construct.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <type_traits>
#include <utility>

namespace dingo::core {

// The context of the walks of a binding, kept by its slot: the objects its
// cached rules derive, or nothing.
template <typename Binding>
class DINGO_EMPTY_BASES binding_context : private Binding::context_type {
public:
  typename Binding::context_type &context() { return *this; }
};

template <typename Binding, slot Kind = info(Binding::scope).storage_slot>
class binding_slot {
  static_assert(dependent_false<Binding>,
                "the next implementation does not provide this scope yet");
};

// A lazily constructed object, made by the factory on first use, whose
// dependencies the resolver resolves.
template <typename Binding>
class binding_slot<Binding, slot::lazy> : public binding_context<Binding> {
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

// No object: the factory constructs one for every request, which consumes it.
// A request of the object while it is being constructed is a recursion.
template <typename Binding>
class binding_slot<Binding, slot::none> : public binding_context<Binding> {
  static_assert(info(Binding::scope).guard == recursion_guard::always,
                "a scope without a slot guards every recursion");

public:
  using stored_type = typename Binding::stored_type;

  // A new object, or a new array, allocated so that its ownership passes to
  // the request.
  template <typename Resolver> decltype(auto) get(Resolver &resolver) {
    if (constructing_) {
      throw type_recursion_exception("type recursion");
    }
    constructing_ = true;
    struct reset {
      bool &value;
      ~reset() { value = false; }
    } guard{constructing_};
    if constexpr (std::is_array_v<stored_type>) {
      using element = std::remove_extent_t<stored_type>;
      element *first = new element[std::extent_v<stored_type>]();
      return *reinterpret_cast<stored_type *>(first);
    } else {
      return make_stored<stored_type>(resolver);
    }
  }

private:
  bool constructing_ = false;
};

// An object supplied by the caller at registration.
template <typename Binding>
class binding_slot<Binding, slot::external_ref>
    : public binding_context<Binding> {
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
