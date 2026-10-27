//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/rtti/static_provider.h>

#include <cstddef>
#include <memory>
#include <tuple>

namespace dingo::model {

// A container configuration is a trait bundle swapped under the same rows. The
// configuration probes only need the trait, one registration, and one request.

template <typename T> struct allocation_state {
  static std::size_t bytes;
};

template <typename T> std::size_t allocation_state<T>::bytes = 0;

template <typename T> struct counting_allocator {
  using value_type = T;

  counting_allocator() noexcept = default;
  template <typename U>
  counting_allocator(const counting_allocator<U> &) noexcept {}

  T *allocate(std::size_t count) {
    T *memory = static_cast<T *>(::operator new(count * sizeof(T)));
    allocation_state<void>::bytes += count * sizeof(T);
    return memory;
  }

  void deallocate(T *memory, std::size_t count) noexcept {
    allocation_state<void>::bytes -= count * sizeof(T);
    ::operator delete(memory);
  }
};

template <typename T, typename U>
bool operator==(const counting_allocator<T> &,
                const counting_allocator<U> &) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const counting_allocator<T> &lhs,
                const counting_allocator<U> &rhs) noexcept {
  return !(lhs == rhs);
}

struct static_rtti_traits {
  using rtti_type = dingo::rtti<dingo::static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

} // namespace dingo::model
