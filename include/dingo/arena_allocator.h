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
#if 1
template <typename T, typename Allocator> class arena_allocator;

class arena_base {
    template <typename T, typename Allocator> friend class arena_allocator;

  public:
    template <std::size_t Size>
    arena_base(std::array<unsigned char, Size>& arena)
        : base_(arena.data()), current_(arena.data()),
          end_(arena.data() + arena.size()), allocated_() {}

    bool operator==(const arena_base& arena) { return base_ == arena.base_; }

    arena_base& operator=(const arena_base& other) {
        base_ = other.base_;
        current_ = other.current_;
        end_ = other.end_;
        allocated_ = other.allocated_;
        return *this;
    }

    std::size_t get_allocated() const { return allocated_; }

  private:
    unsigned char* base_;
    unsigned char* current_;
    unsigned char* end_;
    std::size_t allocated_;
};

template <std::size_t N> class arena : public arena_base {
  public:
    arena() : arena_base(buffer_) {}

    std::array<unsigned char, N> buffer_;
};

template <typename T, typename Allocator = std::allocator<T>>
class arena_allocator : public Allocator {
  public:
    using value_type = T;

    template <typename U> struct rebind {
        using other =
            arena_allocator<U, typename std::allocator_traits<
                                   Allocator>::template rebind_alloc<U>>;
    };

    template <typename... Args>
    arena_allocator(arena_base& arena, Args&&... args) noexcept
        : Allocator(std::forward<Args>(args)...), arena_(arena) {}

    template <typename U, typename AllocatorU> friend class arena_allocator;

    template <typename U, typename AllocatorU>
    arena_allocator(const arena_allocator<U, AllocatorU>& alloc) noexcept
        : Allocator(alloc), arena_(alloc.arena_) {}

    // arena_allocator<T, Allocator>&
    // operator=(const arena_allocator<T, Allocator>& other) {
    //     arena_ = other.arena_;
    //     return *this;
    // }

    value_type* allocate(std::size_t n) {
        unsigned char* ptr =
            (unsigned char*)(uintptr_t(arena_.current_ + alignof(value_type) -
                                       1) &
                             ~(alignof(value_type) - 1));

        std::size_t bytes = n * sizeof(value_type);
        if (ptr + bytes < arena_.end_) {
            value_type* p = reinterpret_cast<value_type*>(ptr);
            ptr += bytes;
            arena_.allocated_ += bytes;
            arena_.current_ = ptr;
            return p;
        } else {
            assert(false);
            return Allocator::allocate(n);
        }
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        auto ptr = reinterpret_cast<unsigned char*>(p);
        if (ptr >= arena_.base_ && ptr < arena_.end_) {
            // This is from arena
            arena_.allocated_ -= n * sizeof(value_type);
            if (arena_.allocated_ == 0) {
                arena_.current_ = arena_.base_;
            }
        } else {
            Allocator::deallocate(p, n);
        }
    }

  private:
    arena_base& arena_;
};

template <typename T, typename U, typename Allocator>
bool operator==(arena_allocator<T, Allocator> const& lhs,
                arena_allocator<U, Allocator> const& rhs) noexcept {
    return lhs.arena_ == rhs.arena_;
}

template <typename T, typename U, typename Allocator>
bool operator!=(arena_allocator<T, Allocator> const& x,
                arena_allocator<U, Allocator> const& y) noexcept {
    return !(x == y);
}
#else
struct arena {
    arena(char* buffer, size_t size)
        : buffer_(buffer), end_(buffer_ + size), current_(buffer_) {}

    template <typename T> T* allocate(size_t n) {
        auto ptr = (char*)((uintptr_t)(current_ + alignof(T)) & ~alignof(T));
        if (ptr + n * sizeof(T) < end_) {
            current_ = ptr + n * sizeof(T);
            return reinterpret_cast<T*>(ptr);
        }

        throw std::bad_alloc();
    }

    template <typename T> void deallocate(T*, size_t) {}

  private:
    char* buffer_;
    char* end_;
    char* current_;
};

template <typename T> class arena_allocator {
    template <typename U> friend class arena_allocator;

  public:
    using value_type = T;

    arena_allocator(arena* arena) noexcept : arena_(arena) {}

    template <typename U>
    arena_allocator(const arena_allocator<U>& other) noexcept
        : arena_(other.arena_) {}

    value_type* allocate(std::size_t n) {
        return reinterpret_cast<value_type*>(arena_->allocate<value_type>(n));
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        arena_->deallocate(p, n);
    }

  private:
    arena* arena_;
};

template <typename T, typename U>
bool operator==(const arena_allocator<T>& lhs,
                const arena_allocator<U>& rhs) noexcept {
    return lhs.arena_ == rhs.arena_;
}
template <typename T, typename U>
bool operator!=(const arena_allocator<T>& lhs,
                const arena_allocator<U>& rhs) noexcept {
    return !(lhs == rhs);
}
#endif
} // namespace dingo
