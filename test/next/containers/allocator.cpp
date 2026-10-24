//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The allocator of a container allocates its bindings, and the container
// frees every one it allocated: when it is destroyed, and when a failed
// request undoes a registration. spec: transaction_rules

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace {

using dingo::scope;
using dingo::storage;

struct allocation_counts {
  static inline std::size_t allocations = 0;
  static inline std::size_t deallocations = 0;
  static inline std::size_t allocated_bytes = 0;
  static inline std::size_t deallocated_bytes = 0;

  static void clear() {
    allocations = deallocations = allocated_bytes = deallocated_bytes = 0;
  }
};

template <typename Type> struct counting_allocator {
  using value_type = Type;

  counting_allocator() = default;
  template <typename Other>
  counting_allocator(const counting_allocator<Other> & /*other*/) noexcept {}

  Type *allocate(std::size_t count) {
    ++allocation_counts::allocations;
    allocation_counts::allocated_bytes += count * sizeof(Type);
    return std::allocator<Type>().allocate(count);
  }

  void deallocate(Type *pointer, std::size_t count) noexcept {
    ++allocation_counts::deallocations;
    allocation_counts::deallocated_bytes += count * sizeof(Type);
    std::allocator<Type>().deallocate(pointer, count);
  }
};

template <typename Left, typename Right>
bool operator==(const counting_allocator<Left> & /*left*/,
                const counting_allocator<Right> & /*right*/) {
  return true;
}

template <typename Left, typename Right>
bool operator!=(const counting_allocator<Left> &left,
                const counting_allocator<Right> &right) {
  return !(left == right);
}

using counted_container =
    dingo::container<dingo::dynamic_container_traits, counting_allocator<char>>;

template <int Id> struct service {
  service() : id(Id) {}
  int id;
};

struct failure {};

TEST(next_allocator, a_container_allocates_its_bindings_from_its_allocator) {
  allocation_counts::clear();
  {
    counted_container container;
    container.register_type<scope<dingo::shared>, storage<service<1>>>();
    container.register_type<scope<dingo::unique>, storage<service<2>>>();
    // The state of the container, and each binding.
    EXPECT_EQ(allocation_counts::allocations, 3u);
    EXPECT_EQ(allocation_counts::deallocations, 0u);
    EXPECT_EQ(container.resolve<service<1> &>().id, 1);
    EXPECT_EQ(container.resolve<service<2>>().id, 2);
  }
  EXPECT_EQ(allocation_counts::deallocations, allocation_counts::allocations);
  EXPECT_EQ(allocation_counts::deallocated_bytes,
            allocation_counts::allocated_bytes);
}

TEST(next_allocator, a_failed_request_frees_the_binding_it_registered) {
  allocation_counts::clear();
  counted_container container;
  container.register_type<scope<dingo::shared>, storage<failure>>(
      dingo::callable([&container]() -> failure {
        container.register_type<scope<dingo::shared>, storage<service<3>>>();
        throw std::runtime_error("failed");
      }));
  // The state of the container, and the binding.
  ASSERT_EQ(allocation_counts::allocations, 2u);
  EXPECT_THROW(container.resolve<failure &>(), std::runtime_error);
  EXPECT_EQ(allocation_counts::allocations, 3u);
  EXPECT_EQ(allocation_counts::deallocations, 1u);
  EXPECT_THROW(container.resolve<service<3> &>(),
               dingo::type_not_found_exception);
}

} // namespace
