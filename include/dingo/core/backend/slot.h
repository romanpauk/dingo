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
#include <dingo/core/projection/layer_traits.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstdint>
#include <new>
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

// An object published before it is constructed: a request during its
// construction refers to the published object, so objects can depend on each
// other in a cycle. A plain leaf is published in the slot, and a sharing
// handle as a handle of an element it constructs later. An alternative is not
// published, since reaching its element reads it. A failed construction
// resets every slot of the outermost publication.
template <typename Binding>
class binding_slot<Binding, slot::two_phase> : public binding_context<Binding> {
  static_assert(info(Binding::scope).guard == recursion_guard::none,
                "a two-phase slot refers to the published object instead");

public:
  using stored_type = typename Binding::stored_type;

  binding_slot() = default;
  binding_slot(const binding_slot &) = delete;
  binding_slot &operator=(const binding_slot &) = delete;
  ~binding_slot() { reset(); }

  template <typename Resolver> stored_type &get(Resolver &resolver) {
    if (state_ == state::empty) {
      resolver.publications().run(this, &reset_slot,
                                  [&] { publish(resolver); });
    } else if (state_ == state::publishing && !published_while_constructed) {
      throw type_recursion_exception("type recursion");
    }
    return *object();
  }

private:
  using traits = layer_traits<stored_type>;
  static constexpr bool handle = traits::kind == layer_kind::share;
  static constexpr bool published_while_constructed =
      traits::kind != layer_kind::alt;

  enum class state : std::uint8_t { empty, publishing, constructed };

  stored_type *object() { return reinterpret_cast<stored_type *>(storage_); }

  template <typename Resolver> void publish(Resolver &resolver) {
    state_ = state::publishing;
    struct failure {
      binding_slot &slot;
      bool active = true;
      ~failure() {
        if (active) {
          slot.reset();
        }
      }
    } guard{*this};
    if constexpr (handle) {
      using element = type_list_at_t<0, typename traits::arguments>;
      auto *block = new publication_block<element>;
      ::new (static_cast<void *>(storage_)) stored_type(
          traits::adopt(block->object(), publication_deleter<element>()));
      holds_handle_ = true;
      ::new (static_cast<void *>(block->object()))
          element(make_stored<element>(resolver));
      block->constructed = true;
    } else {
      construct_stored<stored_type>(storage_, resolver);
    }
    state_ = state::constructed;
    guard.active = false;
  }

  void reset() noexcept {
    if (handle ? holds_handle_ : state_ == state::constructed) {
      destroy_object(std::launder(object()));
    }
    holds_handle_ = false;
    state_ = state::empty;
  }

  static void reset_slot(void *slot) noexcept {
    static_cast<binding_slot *>(slot)->reset();
  }

  alignas(stored_type) unsigned char storage_[sizeof(stored_type)];
  state state_ = state::empty;
  bool holds_handle_ = false;
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
