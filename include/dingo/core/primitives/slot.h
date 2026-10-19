//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Lifetime slots: where a binding keeps its object.

#include <dingo/core/primitives/exceptions.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo::core {

// Destroys an object, element by element for an array, which std::destroy_at
// handles only since C++20.
template <typename Type> void destroy_object(Type *object) {
  if constexpr (std::is_array_v<Type>) {
    for (auto &element : *object) {
      destroy_object(std::addressof(element));
    }
  } else {
    object->~Type();
  }
}

// Holds an object constructed on first use and destroyed with the slot. A
// request of the object while it is being constructed is a recursion.
template <typename Type> class lazy_slot {
public:
  lazy_slot() = default;
  lazy_slot(const lazy_slot &) = delete;
  lazy_slot &operator=(const lazy_slot &) = delete;

  ~lazy_slot() {
    if (state_ == state::constructed) {
      destroy_object(get());
    }
  }

  bool constructed() const { return state_ == state::constructed; }

  // Constructs the object in place by `construct(void *)` unless it exists.
  template <typename Construct> Type &get_or_construct(Construct &&construct) {
    if (state_ != state::constructed) {
      if (state_ == state::constructing) {
        throw type_recursion_exception("type recursion");
      }
      state_ = state::constructing;
      struct reset {
        state &value;
        ~reset() {
          if (value == state::constructing) {
            value = state::empty;
          }
        }
      } guard{state_};
      construct(static_cast<void *>(storage_));
      state_ = state::constructed;
    }
    return *get();
  }

private:
  enum class state : std::uint8_t { empty, constructing, constructed };

  Type *get() { return std::launder(reinterpret_cast<Type *>(storage_)); }

  alignas(Type) unsigned char storage_[sizeof(Type)];
  state state_ = state::empty;
};

// Holds an object supplied by the caller: a reference and an array are kept
// as the address of the caller's object, any other object by value.
template <typename Type> class external_slot {
public:
  template <typename Instance>
  explicit external_slot(Instance &&instance)
      : instance_(std::forward<Instance>(instance)) {}

  Type &get() { return instance_; }

private:
  Type instance_;
};

template <typename Type> class external_slot<Type &> {
public:
  explicit external_slot(Type &instance) : instance_(&instance) {}

  Type &get() { return *instance_; }

private:
  Type *instance_;
};

template <typename Type, std::size_t Extent>
class external_slot<Type[Extent]> : public external_slot<Type (&)[Extent]> {
public:
  using external_slot<Type (&)[Extent]>::external_slot;
};

} // namespace dingo::core
