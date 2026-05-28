//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/runtime_containers.h"

#include <dingo/container.h>

#include <cstddef>

namespace dingo::matrix {

template <typename T> class test_allocator_stats {
  protected:
    static std::size_t allocated_;

  public:
    static std::size_t allocated() { return allocated_; }
};

template <typename T> std::size_t test_allocator_stats<T>::allocated_ = 0;

template <typename T> class test_allocator : public test_allocator_stats<void> {
  public:
    using value_type = T;

    test_allocator() noexcept = default;
    template <typename U> test_allocator(const test_allocator<U>&) noexcept {}

    value_type* allocate(std::size_t n) {
        this->allocated_ += n * sizeof(value_type);
        return static_cast<value_type*>(::operator new(n * sizeof(value_type)));
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        this->allocated_ -= n * sizeof(value_type);
        ::operator delete(p);
    }
};

template <typename T, typename U>
bool operator==(const test_allocator<T>&, const test_allocator<U>&) noexcept {
    return true;
}

template <typename T, typename U>
bool operator!=(const test_allocator<T>& lhs,
                const test_allocator<U>& rhs) noexcept {
    return !(lhs == rhs);
}

} // namespace dingo::matrix
