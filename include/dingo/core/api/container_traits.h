//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The traits of the containers of the next implementation: the lookup
// declarations they list, none by default.

#include <dingo/core/api/rtti/typeid_provider.h>
#include <dingo/core/primitives/allocation.h>

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo {

// The lookup backend that orders the key values, see dingo/lookup/lookup.h.
struct ordered;

struct dynamic_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

struct static_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using lookup_definition_type = std::tuple<>;
};

namespace detail {
// The member types a container takes from its traits: their own, or the
// ones of dynamic_container_traits.
template <typename Traits, typename = void> struct traits_rtti {
  using type = dynamic_container_traits::rtti_type;
};

template <typename Traits>
struct traits_rtti<Traits, std::void_t<typename Traits::rtti_type>> {
  using type = typename Traits::rtti_type;
};

template <typename Traits, typename = void> struct traits_allocator {
  using type = dynamic_container_traits::allocator_type;
};

template <typename Traits>
struct traits_allocator<Traits, std::void_t<typename Traits::allocator_type>> {
  using type = typename Traits::allocator_type;
};

template <typename Type, typename = void>
struct is_container_type : std::false_type {};

template <typename Type>
struct is_container_type<Type, std::void_t<decltype(Type::has_static_source)>>
    : std::true_type {};

// Whether a type is container traits: it declares lookups and is not a
// container, which exposes the lookups of its traits.
template <typename Type, typename = void>
struct is_traits_type : std::false_type {};

template <typename Type>
struct is_traits_type<Type, std::void_t<typename Type::lookup_definition_type>>
    : std::bool_constant<!is_container_type<Type>::value> {};

// The state of a container, its bindings and what they keep, as a base of
// the container: apart from it, allocated by `Allocator`, when it `Moves`, so
// the container takes it along, and its children, which refer to the state,
// follow it; or in place, in a container that does not move. The state
// refers back to its holder, `owner`.
template <typename State, typename Allocator, bool Moves>
class container_state {
public:
  using state_type = State;

  template <typename... Args>
  explicit container_state(const Allocator &allocator, Args &&...args)
      : state_(make(allocator, std::forward<Args>(args)...)) {
    state_->owner = this;
  }

  // A moved-from container can only be destroyed or assigned to.
  container_state(container_state &&other) noexcept
      : state_(std::move(other.state_)) {
    adopt();
  }

  container_state &operator=(container_state &&other) noexcept {
    state_ = std::move(other.state_);
    adopt();
    return *this;
  }

  State *state() const { return state_.get(); }

private:
  using bytes_type = core::byte_allocator_t<Allocator>;
  using pointer =
      std::unique_ptr<State, core::allocated_deleter<State, bytes_type>>;

  template <typename... Args>
  static pointer make(const Allocator &allocator, Args &&...args) {
    bytes_type bytes(allocator);
    return core::allocate_object<State, State>(bytes,
                                               std::forward<Args>(args)...);
  }

  void adopt() noexcept {
    if (state_ != nullptr) {
      state_->owner = this;
    }
  }

  pointer state_;
};

template <typename State, typename Allocator>
class container_state<State, Allocator, false> {
public:
  using state_type = State;

  template <typename... Args>
  explicit container_state(const Allocator & /*allocator*/, Args &&...args)
      : state_(std::forward<Args>(args)...) {
    state_.owner = this;
  }

  container_state(const container_state &) = delete;
  container_state &operator=(const container_state &) = delete;

  State *state() { return &state_; }

private:
  State state_;
};

// Whether the arguments of a constructor of instances are one container of
// the type, which it does not take as an instance.
template <typename Container, typename... Instances>
struct is_self : std::false_type {};

template <typename Container, typename Instance>
struct is_self<Container, Instance>
    : std::is_same<Container, std::decay_t<Instance>> {};

template <typename Type, typename = void>
struct has_state_holder : std::false_type {};

template <typename Type>
struct has_state_holder<Type, std::void_t<typename Type::state_holder>>
    : std::true_type {};

// How a child refers to its parent: by the state of a parent that holds its
// state as a container_state, so the child follows the parent when it moves,
// or by the address of the parent. The parent is complete only when the link
// is made, so the choice is made then.
template <typename Parent> class parent_link {
public:
  parent_link() = default;

  explicit parent_link(Parent *parent) {
    if constexpr (has_state_holder<Parent>::value) {
      link_ = parent != nullptr ? parent->state() : nullptr;
    } else {
      link_ = parent;
    }
  }

  Parent *get() const {
    if constexpr (has_state_holder<Parent>::value) {
      using holder = typename Parent::state_holder;
      using state = typename holder::state_type;
      return link_ != nullptr ? static_cast<Parent *>(static_cast<holder *>(
                                    static_cast<state *>(link_)->owner))
                              : nullptr;
    } else {
      return static_cast<Parent *>(link_);
    }
  }

private:
  void *link_ = nullptr;
};

// The member types of a container of `Traits`.
template <typename Traits> struct container_member_types {
  // The backend of the lookups of a domain no lookup declaration names.
  using default_indexing_type = ::dingo::ordered;
  using traits_type = Traits;
  using lookup_definition_type = typename Traits::lookup_definition_type;
  using rtti_type = typename traits_rtti<Traits>::type;
  using allocator_type = typename traits_allocator<Traits>::type;
};
} // namespace detail

} // namespace dingo
