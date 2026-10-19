//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Lifetime slots: where a binding keeps its object.

#include <cstddef>
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

// Holds an object constructed on first use and destroyed with the slot.
template <typename Type> class lazy_slot {
public:
  lazy_slot() = default;
  lazy_slot(const lazy_slot &) = delete;
  lazy_slot &operator=(const lazy_slot &) = delete;

  ~lazy_slot() {
    if (constructed_) {
      destroy_object(get());
    }
  }

  bool constructed() const { return constructed_; }

  // Constructs the object in place by `construct(void *)` unless it exists.
  template <typename Construct> Type &get_or_construct(Construct &&construct) {
    if (!constructed_) {
      construct(static_cast<void *>(storage_));
      constructed_ = true;
    }
    return *get();
  }

private:
  Type *get() { return std::launder(reinterpret_cast<Type *>(storage_)); }

  alignas(Type) unsigned char storage_[sizeof(Type)];
  bool constructed_ = false;
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
