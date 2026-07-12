//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/memory/arena_allocator.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace dingo {
namespace {

struct counting_allocator_stats {
  static inline std::size_t allocations = 0;
  static inline std::size_t allocated_bytes = 0;
  static inline std::size_t last_allocation_bytes = 0;
  static inline std::size_t deallocations = 0;

  static void reset() {
    allocations = 0;
    allocated_bytes = 0;
    last_allocation_bytes = 0;
    deallocations = 0;
  }
};

template <typename T> struct counting_allocator {
  using value_type = T;

  counting_allocator() = default;

  template <typename U>
  counting_allocator(const counting_allocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    ++counting_allocator_stats::allocations;
    counting_allocator_stats::last_allocation_bytes = n * sizeof(T);
    counting_allocator_stats::allocated_bytes +=
        counting_allocator_stats::last_allocation_bytes;
    return std::allocator<T>{}.allocate(n);
  }

  void deallocate(T *ptr, std::size_t n) noexcept {
    ++counting_allocator_stats::deallocations;
    std::allocator<T>{}.deallocate(ptr, n);
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

bool contains(const std::array<std::uint8_t, 96> &buffer, const void *ptr) {
  auto address = reinterpret_cast<std::uintptr_t>(ptr);
  auto begin = reinterpret_cast<std::uintptr_t>(buffer.data());
  auto end = begin + buffer.size();
  return begin <= address && address < end;
}

} // namespace

TEST(arena_allocator_test, buffer_backed_arena_allocates_overflow_blocks) {
  counting_allocator_stats::reset();
  std::array<std::uint8_t, 96> buffer{};
  arena<counting_allocator<std::uint8_t>> arena(buffer, 64);

  auto *initial = arena.allocate(16, alignof(std::max_align_t));
  ASSERT_TRUE(contains(buffer, initial));

  auto *overflow = arena.allocate(256, alignof(std::max_align_t));
  ASSERT_FALSE(contains(buffer, overflow));
  ASSERT_EQ(counting_allocator_stats::allocations, 1u);
}

TEST(arena_allocator_test, buffer_backed_reset_releases_overflow_and_rewinds) {
  counting_allocator_stats::reset();
  std::array<std::uint8_t, 96> buffer{};
  arena<counting_allocator<std::uint8_t>> arena(buffer, 64);

  auto *initial = arena.allocate(16, alignof(std::max_align_t));
  static_cast<void>(arena.allocate(256, alignof(std::max_align_t)));
  ASSERT_EQ(counting_allocator_stats::allocations, 1u);

  arena.reset();

  ASSERT_EQ(counting_allocator_stats::deallocations, 1u);
  EXPECT_EQ(arena.allocate(16, alignof(std::max_align_t)), initial);
  EXPECT_EQ(counting_allocator_stats::allocations, 1u);
}

TEST(arena_allocator_test, checkpoint_rewind_reuses_storage_after_mark) {
  std::array<std::uint8_t, 128> buffer{};
  arena<counting_allocator<std::uint8_t>> arena(buffer, 128);

  static_cast<void>(arena.allocate(16, alignof(std::max_align_t)));
  auto checkpoint = arena.mark();
  auto *second = arena.allocate(16, alignof(std::max_align_t));

  arena.rewind(checkpoint);

  EXPECT_EQ(arena.allocate(16, alignof(std::max_align_t)), second);
}

TEST(arena_allocator_test, checkpoint_rewind_releases_later_overflow_blocks) {
  counting_allocator_stats::reset();
  std::array<std::uint8_t, 96> buffer{};
  arena<counting_allocator<std::uint8_t>> arena(buffer, 64);

  static_cast<void>(arena.allocate(16, alignof(std::max_align_t)));
  auto checkpoint = arena.mark();
  static_cast<void>(arena.allocate(256, alignof(std::max_align_t)));
  ASSERT_EQ(counting_allocator_stats::allocations, 1u);

  arena.rewind(checkpoint);

  EXPECT_EQ(counting_allocator_stats::deallocations, 1u);
  EXPECT_EQ(counting_allocator_stats::allocations, 1u);
}

TEST(arena_allocator_test, inline_arena_uses_its_embedded_buffer) {
  inline_arena<128> arena;

  const auto *value = arena.allocate(16, alignof(std::max_align_t));
  const auto address = reinterpret_cast<std::uintptr_t>(value);
  const auto begin = reinterpret_cast<std::uintptr_t>(std::addressof(arena));

  EXPECT_GE(address, begin);
  EXPECT_LT(address, begin + sizeof(arena));
}

TEST(arena_allocator_test, heap_arena_reset_releases_all_blocks) {
  counting_allocator_stats::reset();
  arena<counting_allocator<std::uint8_t>> arena(64);

  static_cast<void>(arena.allocate(3000, alignof(std::max_align_t)));
  static_cast<void>(arena.allocate(3000, alignof(std::max_align_t)));
  ASSERT_EQ(counting_allocator_stats::allocations, 2u);

  arena.reset();

  EXPECT_EQ(counting_allocator_stats::deallocations, 2u);
  static_cast<void>(arena.allocate(16, alignof(std::max_align_t)));
  EXPECT_EQ(counting_allocator_stats::allocations, 3u);
}

TEST(arena_allocator_test, small_owned_blocks_are_not_rounded_to_a_page) {
  counting_allocator_stats::reset();
  arena<counting_allocator<std::uint8_t>> arena(128);

  static_cast<void>(arena.allocate(16, alignof(std::max_align_t)));

  EXPECT_EQ(counting_allocator_stats::allocations, 1u);
  EXPECT_EQ(counting_allocator_stats::allocated_bytes, 128u);
}

TEST(arena_allocator_test, small_owned_blocks_do_not_grow) {
  counting_allocator_stats::reset();
  arena<counting_allocator<std::uint8_t>> arena(128);

  static_cast<void>(arena.allocate(88, alignof(std::max_align_t)));
  static_cast<void>(arena.allocate(88, alignof(std::max_align_t)));

  EXPECT_EQ(counting_allocator_stats::allocations, 2u);
  EXPECT_EQ(counting_allocator_stats::allocated_bytes, 256u);
}

TEST(arena_allocator_test, oversized_block_does_not_change_following_size) {
  counting_allocator_stats::reset();
  arena<counting_allocator<std::uint8_t>> arena(128);

  static_cast<void>(arena.allocate(256, alignof(std::max_align_t)));
  static_cast<void>(arena.allocate(88, alignof(std::max_align_t)));

  EXPECT_EQ(counting_allocator_stats::allocations, 2u);
  EXPECT_EQ(counting_allocator_stats::last_allocation_bytes, 128u);
}

TEST(arena_allocator_test, allocation_rejects_block_size_overflow) {
  counting_allocator_stats::reset();
  arena<counting_allocator<std::uint8_t>> arena(128);

  EXPECT_EQ(arena.allocate(std::numeric_limits<intptr_t>::max(), 1), nullptr);
  EXPECT_EQ(counting_allocator_stats::allocations, 0u);
}

TEST(arena_allocator_test, large_owned_blocks_are_rounded_to_a_page) {
  counting_allocator_stats::reset();
  arena<counting_allocator<std::uint8_t>> arena(128);

  static_cast<void>(arena.allocate(2048, alignof(std::max_align_t)));

  EXPECT_EQ(counting_allocator_stats::allocations, 1u);
  EXPECT_EQ(counting_allocator_stats::allocated_bytes, 4080u);
}

} // namespace dingo
