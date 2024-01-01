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
    arena() : current_(buffer_.data()) {}
    static constexpr size_t size() { return N; }

    template <typename T> T* allocate(size_t n) {
        unsigned char* ptr =
            (unsigned char*)(uintptr_t(current_ + alignof(T) - 1) &
                             ~(alignof(T) - 1));
        std::size_t size = n * sizeof(T);
        if (ptr + size < buffer_.data() + buffer_.size()) {
            current_ = ptr + size;
            return reinterpret_cast<T*>(ptr);
        } else {
            return nullptr;
        }
    }

    bool deallocate(unsigned char* ptr) const {
        return ptr >= buffer_.data() && ptr < buffer_.data() + buffer_.size();
    }

    std::array<unsigned char, N> buffer_;
    unsigned char* current_ = 0;
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

    // arena_allocator<T, Allocator>&
    // operator=(const arena_allocator<T, Allocator>& other) {
    //     arena_ = other.arena_;
    //     return *this;
    // }

    value_type* allocate(std::size_t n) {
        if (auto* p = arena_->template allocate<value_type>(n)) {
            return p;
        } else {
            assert(false);
            return Allocator::allocate(n);
        }
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        if (!arena_->deallocate(reinterpret_cast<unsigned char*>(p)))
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
