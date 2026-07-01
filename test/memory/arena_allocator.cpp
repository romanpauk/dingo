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
#include <memory>

namespace dingo {
namespace {

struct counting_allocator_stats {
  static inline std::size_t allocations = 0;
  static inline std::size_t deallocations = 0;

  static void reset() {
    allocations = 0;
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

} // namespace dingo
