//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>

namespace dingo {

template <typename T> struct arena_allocator_traits : std::allocator_traits<T> {
  static constexpr intptr_t page_size() { return 1 << 12; }
  static constexpr intptr_t header_size() { return sizeof(uintptr_t) * 2; }
};

// An arena of blocks of the size it is made with, or, one that GrowsBlocks,
// of blocks twice the size of the block it requested before, up to a limit:
// it requests its blocks less and less often as it grows. A block made for
// an allocation larger than that size does not change it.
template <typename Allocator = std::allocator<uint8_t>,
          bool GrowsBlocks = false>
class arena
    : arena_allocator_traits<Allocator>::template rebind_alloc<uint8_t> {
  using allocator_type = typename arena_allocator_traits<
      Allocator>::template rebind_alloc<uint8_t>;
  using allocator_traits_type = arena_allocator_traits<allocator_type>;

  struct block {
    block *next;
    uintptr_t size : 63;
    uintptr_t owned : 1;
    intptr_t ptr;
  };

  block *block_head_ = nullptr;
  // The size of the next block.
  intptr_t block_size_ = 0;

  static constexpr intptr_t max_grown_block_size = intptr_t{1} << 16;

  static intptr_t block_begin(block *head) {
    return reinterpret_cast<intptr_t>(head) + sizeof(block);
  }

  static intptr_t block_end(block *head) {
    return reinterpret_cast<intptr_t>(head) + head->size;
  }

  static intptr_t checked_block_size(std::size_t block_size) {
    assert(block_size <=
           static_cast<std::size_t>(std::numeric_limits<intptr_t>::max()));
    return static_cast<intptr_t>(block_size);
  }

  bool request_block(intptr_t bytes) {
    assert(block_size_ > 0);
    if (bytes > std::numeric_limits<intptr_t>::max() -
                    static_cast<intptr_t>(sizeof(block))) {
      return false;
    }

    intptr_t size = std::max<intptr_t>(block_size_, sizeof(block) + bytes);
    [[maybe_unused]] const bool regular = size == block_size_;
    const auto header_size = allocator_traits_type::header_size();
    const auto page_size = allocator_traits_type::page_size();
    assert((page_size & (page_size - 1)) == 0);
    if (size > page_size / 2) {
      if (size > std::numeric_limits<intptr_t>::max() - header_size -
                     (page_size - 1)) {
        return false;
      }
      // Account for allocator metadata when rounding large blocks to pages.
      size = ((header_size + size + page_size - 1) & ~(page_size - 1)) -
             header_size;
    }
    assert(size - (intptr_t)sizeof(block) >= bytes);
    auto head = allocate_block(size);
    head->size = size;
    head->owned = true;
    push_block(head);
    if constexpr (GrowsBlocks) {
      if (regular && block_size_ < max_grown_block_size) {
        block_size_ *= 2;
      }
    }
    return true;
  }

  block *allocate_block(intptr_t size) {
    block *ptr =
        reinterpret_cast<block *>(allocator_traits_type::allocate(*this, size));
    assert((reinterpret_cast<intptr_t>(ptr) & (alignof(block) - 1)) == 0);
    return ptr;
  }

  void push_block(block *head) {
    head->next = block_head_;
    head->ptr = block_begin(head);
    block_head_ = head;
  }

  // Allocates in a new block, which holds the size at the alignment.
  DINGO_NOINLINE void *allocate_new_block(intptr_t size, intptr_t alignment) {
    if (std::numeric_limits<intptr_t>::max() - (alignment - 1) < size ||
        !request_block(size + (alignment - 1))) {
      return nullptr;
    }
    const intptr_t ptr = (block_head_->ptr + (alignment - 1)) & -alignment;
    assert(size <= block_end(block_head_) - ptr);
    block_head_->ptr = ptr + size;
    return reinterpret_cast<void *>(ptr);
  }

  void deallocate_block(block *ptr) {
    assert(ptr->owned);
    allocator_traits_type::deallocate(*this, reinterpret_cast<uint8_t *>(ptr),
                                      ptr->size);
  }

  void deallocate_blocks(block *end) {
    auto head = block_head_;
    while (head != end) {
      assert(head != nullptr);
      assert(head->owned || !head->next);
      auto next = head->next;
      if (head->owned) {
        deallocate_block(head);
      } else {
        assert(head->next == nullptr);
        head->ptr = block_begin(head);
        block_head_ = head;
        break;
      }
      head = next;
    }
    if (head == end) {
      block_head_ = end;
    }
  }

public:
  struct checkpoint {
    block *head;
    intptr_t ptr;
  };

  arena(std::size_t block_size) : block_size_(checked_block_size(block_size)) {}

  template <typename AllocatorT>
  arena(std::size_t block_size, const AllocatorT &alloc)
      : allocator_type(alloc), block_size_(checked_block_size(block_size)) {}

  template <typename T, std::size_t N>
  arena(T (&buffer)[N], std::size_t block_size = N * sizeof(T))
      : arena(reinterpret_cast<uint8_t *>(buffer), N * sizeof(T), block_size) {
    static_assert(std::is_trivially_default_constructible_v<T> &&
                  std::is_trivially_copyable_v<T>);
  }

  template <typename T>
  arena(T &buffer, std::size_t block_size = sizeof(T))
      : arena(reinterpret_cast<uint8_t *>(&buffer), sizeof(T), block_size) {
    static_assert(std::is_trivially_destructible_v<T>);
  }

  arena(void *buffer, std::size_t size) : arena(buffer, size, size) {}

  arena(void *buffer, std::size_t size, std::size_t block_size)
      : arena(block_size) {
    assert(size > sizeof(block));
    auto head = reinterpret_cast<block *>(buffer);
    head->owned = false;
    head->size = size;
    push_block(head);
  }

  ~arena() { deallocate_blocks(nullptr); }

  // Allocates in the block at the head when it has room, else in a new block.
  void *allocate(intptr_t size, intptr_t alignment) {
    assert((alignment & (alignment - 1)) == 0);
    if (block_head_ != nullptr) {
      const intptr_t ptr = (block_head_->ptr + (alignment - 1)) & -alignment;
      if (ptr <= block_end(block_head_) - size) {
        block_head_->ptr = ptr + size;
        return reinterpret_cast<void *>(ptr);
      }
    }
    return allocate_new_block(size, alignment);
  }

  void deallocate(void *, std::size_t) {}

  checkpoint mark() const noexcept {
    return checkpoint{block_head_,
                      block_head_ != nullptr ? block_head_->ptr : intptr_t{0}};
  }

  void rewind(checkpoint point) noexcept {
    deallocate_blocks(point.head);
    if (point.head != nullptr) {
      point.head->ptr = point.ptr;
    }
  }

  void reset() { deallocate_blocks(nullptr); }
};

namespace detail {
template <std::size_t Size> struct inline_arena_storage {
  alignas(std::max_align_t) uint8_t buffer[Size];
};
} // namespace detail

template <std::size_t Size>
class inline_arena : private detail::inline_arena_storage<Size>,
                     public arena<> {
  using storage_type = detail::inline_arena_storage<Size>;

public:
  inline_arena() : arena<>(static_cast<storage_type &>(*this).buffer, Size) {}

  inline_arena(const inline_arena &) = delete;
  inline_arena &operator=(const inline_arena &) = delete;
};

template <typename T, typename Arena = arena<>,
          std::size_t Alignment = alignof(std::max_align_t)>
class arena_allocator {
  static_assert((Alignment & (Alignment - 1)) == 0);

  template <typename U, typename ArenaU, std::size_t AlignmentU>
  friend class arena_allocator;
  Arena *arena_ = nullptr;

public:
  using value_type = T;
  static constexpr std::size_t alignment = Alignment > alignof(T) ? Alignment
                                                                  : alignof(T);

  template <typename U> struct rebind {
    using other = arena_allocator<U, Arena, Alignment>;
  };

  arena_allocator(Arena &arena) noexcept : arena_(&arena) {}

  template <typename U, std::size_t AlignmentU>
  arena_allocator(const arena_allocator<U, Arena, AlignmentU> &other) noexcept
      : arena_(other.arena_) {}

  value_type *allocate(std::size_t n) {
    static_assert(alignment >= alignof(T));
    void *memory = std::numeric_limits<intptr_t>::max() / sizeof(T) < n
                       ? nullptr
                       : arena_->allocate(static_cast<intptr_t>(sizeof(T) * n),
                                          static_cast<intptr_t>(alignment));
    if (memory == nullptr) {
      throw std::bad_alloc();
    }
    return static_cast<value_type *>(memory);
  }

  void deallocate(value_type *ptr, std::size_t n) noexcept {
    arena_->deallocate(ptr, sizeof(T) * n);
  }
};

template <typename T, std::size_t AlignmentT, typename U,
          std::size_t AlignmentU, typename Arena>
bool operator==(const arena_allocator<T, Arena, AlignmentT> &lhs,
                const arena_allocator<U, Arena, AlignmentU> &rhs) noexcept {
  return lhs.arena_ == rhs.arena_;
}

template <typename T, std::size_t AlignmentT, typename U,
          std::size_t AlignmentU, typename Arena>
bool operator!=(const arena_allocator<T, Arena, AlignmentT> &x,
                const arena_allocator<U, Arena, AlignmentU> &y) noexcept {
  return !(x == y);
}

} // namespace dingo
