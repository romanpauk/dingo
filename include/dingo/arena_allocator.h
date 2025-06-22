//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <memory>

namespace dingo {

template< typename T > struct arena_allocator_traits: std::allocator_traits< T > {
    static constexpr intptr_t page_size() { return 1<<12; }
    static constexpr intptr_t header_size() { return sizeof(uintptr_t) * 2; }
};

template< typename Allocator = std::allocator<uint8_t> > class arena
    : arena_allocator_traits< Allocator >::template rebind_alloc<uint8_t>
{
    using allocator_type = typename arena_allocator_traits< Allocator >::template rebind_alloc<uint8_t>;
    using allocator_traits_type = arena_allocator_traits< allocator_type >;

    static constexpr std::size_t MaxBlockSize = 1<<21;

    struct block {
        block* next;
        uintptr_t size:63;
        uintptr_t owned:1;
    };

    block* block_initial_ = nullptr;
    std::size_t block_size_ = 0;

    struct state {
        block* block_head_ = nullptr;
        intptr_t block_ptr_ = 0;
        intptr_t block_end_ = 0;
        intptr_t allocated_ = 0;
        intptr_t block_size_ = 0;
    };

    state state_;

    bool request_block(intptr_t bytes) {
        assert(state_.block_size_ > 0);

        // For large blocks, glibc's malloc is aligning large allocations
        // to the multiples of page size, also keeping space for chunk size.
        auto header_size = allocator_traits_type::header_size();
        auto page_size = allocator_traits_type::page_size();
        intptr_t size = ((
            header_size +
            std::max<intptr_t>(state_.block_size_, sizeof(block) + bytes) +
            page_size - 1
        ) & ~(page_size - 1)) - header_size;
        if (size < 0)
            return false;
        assert(size - (intptr_t)sizeof(block) >= bytes);
        auto head = allocate_block(size);
        head->size = size;
        head->owned = true;
        push_block(head);
        state_.block_size_ = std::min<intptr_t>(state_.block_size_ * 2, MaxBlockSize);
        return true;
    }

    block* allocate_block(intptr_t size) {
        block *ptr = reinterpret_cast<block*>(allocator_traits_type::allocate(*this, size));
        assert((reinterpret_cast<intptr_t>(ptr) & (alignof(block) - 1)) == 0);
        return ptr;
    }

    void push_block(block* head) {
        head->next = state_.block_head_;
        state_.block_head_ = head;
        state_.block_ptr_ = reinterpret_cast<intptr_t>(head) + sizeof(block);
        state_.block_end_ = reinterpret_cast<intptr_t>(head) + head->size;
    }

    void deallocate_block(block* ptr) {
        assert(ptr->owned);
        allocator_traits_type::deallocate(*this, reinterpret_cast<uint8_t*>(ptr), ptr->size);
    }

    void deallocate_blocks(block* end) {
        auto head = state_.block_head_;
        while(head != end) {
            assert(head->owned || !head->next);
            auto next = head->next;
            if (head->owned)
                deallocate_block(head);
            head = next;
        }
    }

public:
    arena(std::size_t block_size)
        : block_size_(block_size)
    {
        state_.block_size_ = block_size;
    }

    template< typename T, std::size_t N > arena(T(&buffer)[N], std::size_t block_size = N * sizeof(T))
        : arena(reinterpret_cast<uint8_t*>(buffer), N * sizeof(T), block_size) {
        static_assert(std::is_trivial_v<T>);
    }

    arena(void* buffer, std::size_t size)
        : arena(buffer, size, size)
    {}

    arena(void* buffer, std::size_t size, std::size_t block_size)
        : arena(block_size)
    {
        assert(size > sizeof(block));
        auto head = reinterpret_cast<block*>(buffer);
        block_initial_ = head;
        head->owned = false;
        head->size = size;
        push_block(head);
    }

    ~arena() {
        deallocate_blocks(nullptr);
    }

    void* allocate(intptr_t size, intptr_t alignment) {
        assert((alignment & (alignment - 1)) == 0);
        intptr_t padding = -state_.block_ptr_ & (alignment - 1);
        intptr_t capacity = state_.block_end_ - state_.block_ptr_ - padding;
        if (capacity < size) {
            if (std::numeric_limits<intptr_t>::max() - (alignment - 1) < size)
                return nullptr;
            if (!request_block(size + (alignment - 1)))
                return nullptr;
            padding = -state_.block_ptr_ & (alignment - 1);
            assert(size <= state_.block_end_ - state_.block_ptr_ - padding);
        }
        intptr_t ptr = state_.block_ptr_ + padding;
        assert((ptr & (alignment - 1)) == 0);
        state_.block_ptr_ = ptr + size;
        state_.allocated_ += size;
        return reinterpret_cast<void*>(ptr);
    }

    void deallocate(void*, std::size_t size) {
        state_.allocated_ -= size;
        assert(state_.allocated_ >= 0);
        if (state_.allocated_ == 0) {
            deallocate_blocks(nullptr);
            state_.block_head_ = nullptr;
            state_.block_size_ = block_size_;
            push_block(block_initial_);
        }
    }
};

template <typename T> struct arena_allocator_alignment {
    static constexpr std::size_t value = alignof(
        std::conditional_t< std::is_same_v<T, void>, std::max_align_t, T >
    );
};

template <typename T> static constexpr std::size_t arena_allocator_alignment_v = arena_allocator_alignment<T>::value;

template <
    typename T,
    typename Arena = arena<>,
    std::size_t Alignment = alignof(std::max_align_t)
> class arena_allocator {
    static_assert((Alignment & (Alignment - 1)) == 0);

    template <typename U, typename ArenaU, std::size_t AlignmentU> friend class arena_allocator;
    Arena* arena_ = nullptr;

public:
    using value_type    = T;
    static constexpr std::size_t alignment = std::max(Alignment, arena_allocator_alignment_v<T>);

    template< typename U > struct rebind { using other = arena_allocator< U, Arena, Alignment >; };

    arena_allocator(Arena& arena) noexcept
        : arena_(&arena) {}

    template <typename U, std::size_t AlignmentU> arena_allocator(const arena_allocator<U, Arena, AlignmentU>& other) noexcept
        : arena_(other.arena_) {}

    value_type* allocate(std::size_t n) {
        if (std::numeric_limits<intptr_t>::max() / sizeof(T) < n)
            return nullptr;

        static_assert(alignment >= alignof(T));
        return reinterpret_cast<value_type*>(arena_->allocate(sizeof(T) * n, alignment));
    }

    void deallocate(value_type* ptr, std::size_t n) noexcept {
        arena_->deallocate(ptr, sizeof(T) * n);
    }
};

template < typename Arena, std::size_t Alignment > class arena_allocator<void, Arena, Alignment> {
    template <typename U, typename ArenaU, std::size_t AlignmentU> friend class arena_allocator;
    Arena* arena_ = nullptr;

public:
    using value_type = void;
    template< typename U > struct rebind { using other = arena_allocator< U, Arena, Alignment >; };

    arena_allocator(Arena& arena) noexcept
        : arena_(&arena) {}

    template <typename U, std::size_t AlignmentU> arena_allocator(const arena_allocator<U, Arena, AlignmentU>& other) noexcept
        : arena_(other.arena_) {}
};

template <typename T, std::size_t AlignmentT, typename U, std::size_t AlignmentU, typename Arena>
bool operator == (const arena_allocator<T, Arena, AlignmentT>& lhs, const arena_allocator<U, Arena, AlignmentU>& rhs) noexcept {
    return lhs.arena_ == rhs.arena_;
}

template <typename T, std::size_t AlignmentT, typename U, std::size_t AlignmentU, typename Arena>
bool operator != (const arena_allocator<T, Arena, AlignmentT>& x, const arena_allocator<U, Arena, AlignmentU>& y) noexcept {
    return !(x == y);
}

}

