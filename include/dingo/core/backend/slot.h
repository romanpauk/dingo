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
#include <dingo/core/primitives/slot.h>
#include <dingo/core/primitives/type_name.h>
#include <dingo/core/primitives/type_path.h>
#include <dingo/core/projection/construct.h>
#include <dingo/core/projection/layer_traits.h>
#include <dingo/core/vocabulary/diagnostics.gen.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstdint>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

namespace dingo::core {

// Reports a request of an object while it is being constructed: the path of
// the types being constructed, ending with the repeated one.
template <typename Type> [[noreturn]] void raise_recursion() {
  std::string path;
  if (type_frame::active()) {
    type_frame::append_path(path);
    path += " -> ";
  }
  append_type_name<Type>(path);
  raise_type_recursion(path);
}

// The factory of a binding, kept by its slot, empty unless it holds a callable
// object.
template <typename Factory> class factory_holder : private Factory {
public:
  factory_holder() = default;

  template <typename Argument>
  explicit factory_holder(Argument &&factory)
      : Factory(std::forward<Argument>(factory)) {}

  Factory &factory() { return *this; }
};

// What a slot keeps besides its object: the factory of the binding, and the
// context of its walks, the objects its cached rules derive, or nothing.
template <typename Binding>
class DINGO_EMPTY_BASES binding_context
    : private Binding::context_type,
      public factory_holder<typename Binding::factory_type> {
public:
  binding_context() = default;

  template <typename Factory>
  explicit binding_context(Factory &&factory)
      : factory_holder<typename Binding::factory_type>(
            std::forward<Factory>(factory)) {}

  typename Binding::context_type &context() { return *this; }

  // Forgets the objects the cached rules derived from the object of the slot,
  // when the object is destroyed.
  void clear_context() noexcept { context().clear(); }
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
  using binding_context<Binding>::binding_context;

  // Whether the object is constructed and kept: no transaction that a
  // failure would destroy it with is in progress.
  template <typename Resolver> bool settled(Resolver &resolver) const {
    return slot_.constructed() && !resolver.transactions().active();
  }

  // The object, constructed on first use, which the transaction in progress
  // destroys when it fails; the constructed object is checked first, so a
  // request of it compares once.
  template <typename Resolver> stored_type &get(Resolver &resolver) {
    if (slot_.constructed()) {
      return slot_.object();
    }
    if (slot_.constructing()) {
      raise_recursion<leaf_of_t<stored_type>>();
    }
    // The construction is a transaction of the container, which destroys
    // the object when a construction it is part of fails.
    return resolver.transactions().run([&]() -> stored_type & {
      stored_type &object =
          slot_.get_or_construct([this, &resolver](void *where) {
            const auto frame = type_frame::of<leaf_of_t<stored_type>>();
            construct_stored<stored_type>(where, this->factory(), resolver);
          });
      resolver.transactions().record(this, &reset_slot);
      return object;
    });
  }

private:
  static void reset_slot(void *slot) noexcept {
    auto &self = *static_cast<binding_slot *>(slot);
    self.clear_context();
    self.slot_.reset();
  }

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
  using binding_context<Binding>::binding_context;

  // A request consumes a new object, so none is settled.
  template <typename Resolver> bool settled(Resolver & /*resolver*/) const {
    return false;
  }

  // A new object, or a new array, allocated so that its ownership passes to
  // the request.
  template <typename Resolver> decltype(auto) get(Resolver &resolver) {
    if (constructing_) {
      raise_recursion<leaf_of_t<stored_type>>();
    }
    const auto frame = type_frame::of<leaf_of_t<stored_type>>();
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
      // The construction is a transaction of the container.
      return resolver.transactions().run([&]() -> stored_type {
        return this->factory().template make<stored_type>(resolver);
      });
    }
  }

private:
  bool constructing_ = false;
};

namespace detail {
template <typename Base, typename Derived, typename = void>
struct converts_down : std::false_type {};

template <typename Base, typename Derived>
struct converts_down<
    Base, Derived,
    std::void_t<decltype(static_cast<Derived *>(std::declval<Base *>()))>>
    : std::true_type {};

// Whether `Base` is a base of `Derived` that a pointer to `Derived` reaches
// only through the object, a virtual base: a static conversion down from it is
// ill-formed.
template <typename Base, typename Derived>
inline constexpr bool is_virtual_base_v =
    std::is_base_of_v<Base, Derived> && !std::is_same_v<Base, Derived> &&
    !converts_down<Base, Derived>::value;

template <typename Leaf, typename List> struct reaches_interfaces_statically;

template <typename Leaf, typename... Interfaces>
struct reaches_interfaces_statically<Leaf, type_list<Interfaces...>>
    : std::bool_constant<(!is_virtual_base_v<Interfaces, Leaf> && ...)> {};
} // namespace detail

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

  using binding_context<Binding>::binding_context;
  binding_slot(const binding_slot &) = delete;
  binding_slot &operator=(const binding_slot &) = delete;
  ~binding_slot() { reset(); }

  // Whether the object is constructed and no publication that a failure
  // would reset it with is in progress.
  template <typename Resolver> bool settled(Resolver &resolver) const {
    return state_ == state::constructed && !resolver.transactions().active();
  }

  template <typename Resolver> stored_type &get(Resolver &resolver) {
    if (state_ == state::empty) {
      // The transaction resets the slot when it fails: an object constructed
      // during it may refer to the published object.
      resolver.transactions().run([&] {
        resolver.transactions().record(this, &reset_slot);
        publish(resolver);
      });
    } else if (state_ == state::publishing && !published_while_constructed) {
      raise_recursion<leaf_of_t<stored_type>>();
    }
    return *object();
  }

private:
  using traits = layer_traits<stored_type>;
  static constexpr bool handle = traits::kind == layer_kind::share;
  static constexpr bool published_while_constructed =
      traits::kind != layer_kind::alt;

  template <typename Type, bool Handle> struct published_leaf {
    using type = Type;
  };

  template <typename Type> struct published_leaf<Type, true> {
    using type = type_list_at_t<0, typename layer_traits<Type>::arguments>;
  };

  static_assert(detail::reaches_interfaces_statically<
                    typename published_leaf<stored_type, handle>::type,
                    typename Binding::interfaces>::value,
                "shared_cyclical cannot publish a leaf as an interface it "
                "inherits virtually: the conversion reads the object, which "
                "is not constructed yet");

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
          element(this->factory().template make<element>(resolver));
      block->constructed = true;
    } else {
      construct_stored<stored_type>(storage_, this->factory(), resolver);
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
    this->clear_context();
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

  template <typename Resolver> bool settled(Resolver & /*resolver*/) const {
    return true;
  }

  template <typename Resolver> auto &get(Resolver & /*resolver*/) {
    return slot_.get();
  }

private:
  external_slot<stored_type> slot_;
};

} // namespace dingo::core
