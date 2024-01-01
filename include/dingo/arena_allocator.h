//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <array>
#include <cassert>
#include <memory>

namespace dingo {

template <std::size_t N> class arena {
  public:
    arena() : current_(begin()) {}
    static constexpr size_t size() { return N; }

    template <size_t Alignment> void* allocate(size_t bytes) {
        uintptr_t ptr = (current_ + Alignment - 1) & ~(Alignment - 1);
        if (ptr + bytes < end()) {
            current_ = ptr + bytes;
            return reinterpret_cast<void*>(ptr);
        } else {
            return nullptr;
        }
    }

    bool deallocate(void* ptr) const {
        return reinterpret_cast<uintptr_t>(ptr) >= begin() &&
               reinterpret_cast<uintptr_t>(ptr) < end();
    }

  private:
    uintptr_t begin() const { return reinterpret_cast<uintptr_t>(&buffer_); }
    uintptr_t end() const {
        return reinterpret_cast<uintptr_t>(reinterpret_cast<uint8_t*>(begin()) +
                                           size());
    }

    std::aligned_storage_t<N> buffer_;
    uintptr_t current_ = 0;
};

template <typename T, size_t N, typename Allocator = std::allocator<T>>
class arena_allocator : public Allocator {
  public:
    using value_type = T;

    template <typename U> struct rebind {
        using other = arena_allocator<U, N,
                                      typename std::allocator_traits<
                                          Allocator>::template rebind_alloc<U>>;
    };

    template <typename... Args>
    arena_allocator(arena<N>& arena, Args&&... args) noexcept
        : Allocator(std::forward<Args>(args)...), arena_(&arena) {}

    template <typename U, size_t NU, typename AllocatorU>
    friend class arena_allocator;

    template <typename U, typename AllocatorU>
    arena_allocator(const arena_allocator<U, N, AllocatorU>& alloc) noexcept
        : Allocator(alloc), arena_(alloc.arena_) {}

    value_type* allocate(std::size_t n) {
        if (auto* p = arena_->template allocate<alignof(value_type)>(
                n * sizeof(value_type))) {
            return reinterpret_cast<value_type*>(p);
        } else {
            assert(false);
            return Allocator::allocate(n);
        }
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        if (!arena_->deallocate(p))
            Allocator::deallocate(p, n);
    }

  private:
    arena<N>* arena_;
};

template <typename T, typename U, size_t N, typename Allocator>
bool operator==(arena_allocator<T, N, Allocator> const& lhs,
                arena_allocator<U, N, Allocator> const& rhs) noexcept {
    return lhs.arena_ == rhs.arena_;
}

template <typename T, typename U, size_t N, typename Allocator>
bool operator!=(arena_allocator<T, N, Allocator> const& x,
                arena_allocator<U, N, Allocator> const& y) noexcept {
    return !(x == y);
}

} // namespace dingo
