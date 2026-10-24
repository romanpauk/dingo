//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Objects of any alignment allocated from an allocator of bytes, which need
// not align them, and freed with the bytes they took.

#include <dingo/memory/allocation.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using dingo::core::allocate_aligned;
using dingo::core::allocate_object;
using dingo::core::byte_allocator_t;
using dingo::core::deallocate_aligned;

struct allocation_stats {
  std::size_t allocations = 0;
  std::size_t deallocations = 0;
  std::size_t allocated_bytes = 0;
  std::size_t deallocated_bytes = 0;
  std::vector<unsigned char *> freed;
  std::vector<unsigned char *> given;
  std::size_t last_size = 0;
};

// An allocator of bytes that gives addresses one byte past an aligned one,
// so no allocation it makes is aligned for more than a byte.
template <typename Type> struct misaligning_allocator {
  using value_type = Type;

  explicit misaligning_allocator(allocation_stats *init_stats)
      : stats(init_stats) {}
  template <typename Other>
  misaligning_allocator(const misaligning_allocator<Other> &other) noexcept
      : stats(other.stats) {}

  Type *allocate(std::size_t count) {
    static_assert(sizeof(Type) == 1);
    ++stats->allocations;
    stats->allocated_bytes += count;
    stats->last_size = count;
    auto *bytes = static_cast<unsigned char *>(
        ::operator new(count + 1, std::align_val_t(64)));
    stats->given.push_back(bytes + 1);
    return reinterpret_cast<Type *>(bytes + 1);
  }

  void deallocate(Type *pointer, std::size_t count) noexcept {
    ++stats->deallocations;
    stats->deallocated_bytes += count;
    auto *bytes = reinterpret_cast<unsigned char *>(pointer);
    stats->freed.push_back(bytes);
    ::operator delete(bytes - 1, std::align_val_t(64));
  }

  allocation_stats *stats;
};

template <typename Left, typename Right>
bool operator==(const misaligning_allocator<Left> &left,
                const misaligning_allocator<Right> &right) {
  return left.stats == right.stats;
}

template <typename Left, typename Right>
bool operator!=(const misaligning_allocator<Left> &left,
                const misaligning_allocator<Right> &right) {
  return !(left == right);
}

using bytes = misaligning_allocator<unsigned char>;

static_assert(std::is_same_v<byte_allocator_t<misaligning_allocator<int>>,
                             misaligning_allocator<unsigned char>>);
static_assert(std::is_same_v<byte_allocator_t<std::allocator<char>>,
                             std::allocator<unsigned char>>);

TEST(next_allocation, aligns_an_allocation_its_allocator_does_not_align) {
  allocation_stats stats;
  bytes allocator(&stats);
  for (std::size_t alignment = 1; alignment <= 256; alignment *= 2) {
    void *object = allocate_aligned(allocator, 24, alignment);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object) % alignment, 0u)
        << "alignment " << alignment;
    // The object lies within the bytes allocated for it.
    const auto *start = stats.given.back();
    EXPECT_GE(static_cast<unsigned char *>(object), start);
    EXPECT_LE(static_cast<unsigned char *>(object) + 24,
              start + stats.last_size);
    deallocate_aligned(allocator, object);
  }
}

TEST(next_allocation, frees_the_bytes_it_allocated) {
  allocation_stats stats;
  bytes allocator(&stats);
  void *first = allocate_aligned(allocator, 16, alignof(std::max_align_t));
  void *second = allocate_aligned(allocator, 3000, 64);
  ASSERT_EQ(stats.allocations, 2u);
  deallocate_aligned(allocator, second);
  deallocate_aligned(allocator, first);
  EXPECT_EQ(stats.deallocations, 2u);
  EXPECT_EQ(stats.deallocated_bytes, stats.allocated_bytes);
  EXPECT_EQ(stats.freed,
            (std::vector<unsigned char *>{stats.given[1], stats.given[0]}));
}

struct base {
  virtual ~base() = default;
  virtual int value() const = 0;
};

struct alignas(64) derived : base {
  derived(std::vector<int> *init_events, int init_value)
      : events(init_events), stored(init_value) {}
  ~derived() override { events->push_back(stored); }
  int value() const override { return stored; }

  std::vector<int> *events;
  int stored;
};

struct throwing : base {
  throwing() { throw std::runtime_error("failed"); }
  int value() const override { return 0; }
};

TEST(next_allocation, an_object_is_destroyed_and_freed_through_its_base) {
  allocation_stats stats;
  bytes allocator(&stats);
  std::vector<int> events;
  {
    auto object = allocate_object<derived, base>(allocator, &events, 7);
    EXPECT_EQ(object->value(), 7);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % alignof(derived),
              0u);
    EXPECT_EQ(stats.allocations, 1u);
    EXPECT_EQ(stats.deallocations, 0u);
  }
  EXPECT_EQ(events, (std::vector<int>{7}));
  EXPECT_EQ(stats.deallocations, 1u);
  EXPECT_EQ(stats.deallocated_bytes, stats.allocated_bytes);
}

TEST(next_allocation, a_failed_construction_frees_its_bytes) {
  allocation_stats stats;
  bytes allocator(&stats);
  EXPECT_THROW((allocate_object<throwing, base>(allocator)),
               std::runtime_error);
  EXPECT_EQ(stats.allocations, 1u);
  EXPECT_EQ(stats.deallocations, 1u);
  EXPECT_EQ(stats.deallocated_bytes, stats.allocated_bytes);
}

} // namespace
