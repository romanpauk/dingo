//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

#include <dingo/factory/callable.h>
#include <dingo/storage/shared.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

namespace dingo::matrix {

struct parent_allocation_record {
  std::uintptr_t begin;
  std::uintptr_t end;
  bool active;
};

struct parent_allocator_state {
  std::vector<parent_allocation_record> records;
  std::size_t allocations = 0;
  std::size_t deallocations = 0;

  void record_allocation(void *ptr, std::size_t size) {
    const auto begin = reinterpret_cast<std::uintptr_t>(ptr);
    records.push_back({begin, begin + size, true});
    ++allocations;
  }

  void record_deallocation(void *ptr) noexcept {
    const auto begin = reinterpret_cast<std::uintptr_t>(ptr);
    for (auto &record : records) {
      if (record.active && record.begin == begin) {
        record.active = false;
        ++deallocations;
        return;
      }
    }
    std::terminate();
  }

  bool owns(const void *ptr) const {
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    for (const auto &record : records) {
      if (record.active && record.begin <= address && address < record.end) {
        return true;
      }
    }
    return false;
  }

  std::size_t outstanding() const { return allocations - deallocations; }
};

template <typename T> class parent_tracking_allocator {
public:
  using value_type = T;

  explicit parent_tracking_allocator(parent_allocator_state *state = nullptr)
      : state_(state) {}

  template <typename U>
  parent_tracking_allocator(const parent_tracking_allocator<U> &other) noexcept
      : state_(other.state()) {}

  value_type *allocate(std::size_t count) {
    const auto size = count * sizeof(value_type);
    auto *ptr = static_cast<value_type *>(::operator new(size));
    if (state_ != nullptr) {
      try {
        state_->record_allocation(ptr, size);
      } catch (...) {
        ::operator delete(ptr);
        throw;
      }
    }
    return ptr;
  }

  void deallocate(value_type *ptr, std::size_t) noexcept {
    if (state_ != nullptr) {
      state_->record_deallocation(ptr);
    }
    ::operator delete(ptr);
  }

  parent_allocator_state *state() const { return state_; }

private:
  template <typename> friend class parent_tracking_allocator;

  parent_allocator_state *state_;
};

template <typename T, typename U>
bool operator==(const parent_tracking_allocator<T> &lhs,
                const parent_tracking_allocator<U> &rhs) noexcept {
  return lhs.state() == rhs.state();
}

template <typename T, typename U>
bool operator!=(const parent_tracking_allocator<T> &lhs,
                const parent_tracking_allocator<U> &rhs) noexcept {
  return !(lhs == rhs);
}

struct parent_allocator_traits : dingo::dynamic_container_traits {
  using allocator_type = parent_tracking_allocator<char>;
};

struct alignas(64) parent_allocated_object {
  parent_allocated_object() : bytes{} {}

  std::array<std::byte, 1024> bytes;
};

struct alignas(64) child_allocated_object {
  child_allocated_object() : bytes{} {}

  std::array<std::byte, 1024> bytes;
};

struct parent_allocator_trigger {};

struct alignas(64) rollback_child_allocated_object {
  rollback_child_allocated_object() : bytes{} { ++constructions; }
  ~rollback_child_allocated_object() { ++destructions; }

  inline static int constructions = 0;
  inline static int destructions = 0;
  std::array<std::byte, 1024> bytes;
};

struct alignas(64) rollback_parent_allocated_object {
  rollback_parent_allocated_object() : bytes{} { ++constructions; }
  ~rollback_parent_allocated_object() { ++destructions; }

  inline static int constructions = 0;
  inline static int destructions = 0;
  std::array<std::byte, 1024> bytes;
};

template <typename ParentShape, typename ChildShape>
void assert_parent_allocator_ownership() {
  using allocator_type = typename parent_allocator_traits::allocator_type;
  using parent_type =
      typename ParentShape::template type<parent_allocator_traits>;
  using child_type =
      typename ChildShape::template type<parent_allocator_traits, parent_type>;

  parent_allocator_state parent_state;
  parent_allocator_state child_state;
  parent_allocated_object *parent_object = nullptr;

  {
    parent_type parent{allocator_type(&parent_state)};
    parent.template register_type<dingo::scope<dingo::shared>,
                                  dingo::storage<parent_allocated_object>>();

    {
      child_type child(&parent, allocator_type(&child_state));
      child.template register_type<dingo::scope<dingo::shared>,
                                   dingo::storage<child_allocated_object>>();

      parent_object =
          std::addressof(child.template resolve<parent_allocated_object &>());
      auto *child_object =
          std::addressof(child.template resolve<child_allocated_object &>());

      ASSERT_TRUE(parent_state.owns(parent_object));
      ASSERT_FALSE(child_state.owns(parent_object));
      ASSERT_TRUE(child_state.owns(child_object));
      ASSERT_FALSE(parent_state.owns(child_object));
      ASSERT_EQ(reinterpret_cast<std::uintptr_t>(parent_object) % 64u, 0u);
      ASSERT_EQ(reinterpret_cast<std::uintptr_t>(child_object) % 64u, 0u);
    }

    ASSERT_TRUE(parent_state.owns(parent_object));
    ASSERT_EQ(child_state.outstanding(), 0u);
  }

  ASSERT_EQ(parent_state.outstanding(), 0u);
  ASSERT_EQ(parent_state.allocations, parent_state.deallocations);
  ASSERT_EQ(child_state.allocations, child_state.deallocations);
}

template <typename ParentShape, typename ChildShape>
void assert_failed_child_uses_child_allocator() {
  using allocator_type = typename parent_allocator_traits::allocator_type;
  using parent_type =
      typename ParentShape::template type<parent_allocator_traits>;
  using child_type =
      typename ChildShape::template type<parent_allocator_traits, parent_type>;

  rollback_child_allocated_object::constructions = 0;
  rollback_child_allocated_object::destructions = 0;
  parent_allocator_state parent_state;
  parent_allocator_state child_state;
  parent_allocated_object *parent_object = nullptr;
  rollback_child_allocated_object *failed_child_object = nullptr;

  {
    parent_type parent{allocator_type(&parent_state)};
    child_type child(&parent, allocator_type(&child_state));
    parent.template register_type<dingo::scope<dingo::shared>,
                                  dingo::storage<parent_allocated_object>>();
    child.template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<parent_allocator_trigger>>(
        dingo::callable([&]() -> parent_allocator_trigger {
          parent_object = std::addressof(
              child.template resolve<parent_allocated_object &>());
          child.template register_type<
              dingo::scope<dingo::shared>,
              dingo::storage<rollback_child_allocated_object>>();
          failed_child_object = std::addressof(
              child.template resolve<rollback_child_allocated_object &>());
          throw std::runtime_error("child construction failed");
        }));
    const auto child_allocations = child_state.allocations;
    const auto child_deallocations = child_state.deallocations;

    ASSERT_THROW(child.template resolve<parent_allocator_trigger &>(),
                 std::runtime_error);

    ASSERT_NE(parent_object, nullptr);
    ASSERT_NE(failed_child_object, nullptr);
    ASSERT_TRUE(parent_state.owns(parent_object));
    ASSERT_FALSE(child_state.owns(failed_child_object));
    ASSERT_GT(child_state.allocations, child_allocations);
    ASSERT_GT(child_state.deallocations, child_deallocations);
    ASSERT_EQ(rollback_child_allocated_object::constructions, 1);
    ASSERT_EQ(rollback_child_allocated_object::destructions, 1);
    ASSERT_THROW(child.template resolve<rollback_child_allocated_object &>(),
                 dingo::type_not_found_exception);

    child.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<rollback_child_allocated_object>>();
    auto *retry = std::addressof(
        child.template resolve<rollback_child_allocated_object &>());
    ASSERT_TRUE(child_state.owns(retry));
    ASSERT_EQ(&child.template resolve<parent_allocated_object &>(),
              parent_object);
    ASSERT_EQ(rollback_child_allocated_object::constructions, 2);
    ASSERT_EQ(rollback_child_allocated_object::destructions, 1);
  }

  ASSERT_EQ(parent_state.outstanding(), 0u);
  ASSERT_EQ(child_state.outstanding(), 0u);
  ASSERT_EQ(rollback_child_allocated_object::destructions, 2);
}

template <typename ParentShape, typename ChildShape>
void assert_failed_parent_uses_parent_allocator() {
  using allocator_type = typename parent_allocator_traits::allocator_type;
  using parent_type =
      typename ParentShape::template type<parent_allocator_traits>;
  using child_type =
      typename ChildShape::template type<parent_allocator_traits, parent_type>;

  rollback_parent_allocated_object::constructions = 0;
  rollback_parent_allocated_object::destructions = 0;
  parent_allocator_state parent_state;
  parent_allocator_state child_state;
  rollback_parent_allocated_object *failed_parent_object = nullptr;

  {
    parent_type parent{allocator_type(&parent_state)};
    child_type child(&parent, allocator_type(&child_state));
    parent.template register_type<dingo::scope<dingo::shared>,
                                  dingo::storage<parent_allocator_trigger>>(
        dingo::callable([&]() -> parent_allocator_trigger {
          parent.template register_type<
              dingo::scope<dingo::shared>,
              dingo::storage<rollback_parent_allocated_object>>();
          failed_parent_object = std::addressof(
              parent.template resolve<rollback_parent_allocated_object &>());
          throw std::runtime_error("parent construction failed");
        }));
    const auto parent_allocations = parent_state.allocations;
    const auto parent_deallocations = parent_state.deallocations;
    const auto child_allocations = child_state.allocations;
    const auto child_deallocations = child_state.deallocations;

    ASSERT_THROW(child.template resolve<parent_allocator_trigger &>(),
                 std::runtime_error);

    ASSERT_NE(failed_parent_object, nullptr);
    ASSERT_FALSE(parent_state.owns(failed_parent_object));
    ASSERT_GT(parent_state.allocations, parent_allocations);
    ASSERT_GT(parent_state.deallocations, parent_deallocations);
    ASSERT_EQ(child_state.allocations, child_allocations);
    ASSERT_EQ(child_state.deallocations, child_deallocations);
    ASSERT_EQ(rollback_parent_allocated_object::constructions, 1);
    ASSERT_EQ(rollback_parent_allocated_object::destructions, 1);
    ASSERT_THROW(parent.template resolve<rollback_parent_allocated_object &>(),
                 dingo::type_not_found_exception);
  }

  ASSERT_EQ(parent_state.outstanding(), 0u);
  ASSERT_EQ(child_state.outstanding(), 0u);
}

template <typename ParentShape, typename ChildShape>
void assert_parent_allocator_pair() {
  assert_parent_allocator_ownership<ParentShape, ChildShape>();
  assert_failed_child_uses_child_allocator<ParentShape, ChildShape>();
  assert_failed_parent_uses_parent_allocator<ParentShape, ChildShape>();
}

template <typename = void> void exercise_parent_allocator_pairs() {
  assert_parent_allocator_pair<nested_runtime_parent_shape,
                               nested_runtime_child_shape>();
  assert_parent_allocator_pair<nested_runtime_parent_shape,
                               nested_container_child_shape>();
  assert_parent_allocator_pair<nested_container_parent_shape,
                               nested_runtime_child_shape>();
  assert_parent_allocator_pair<nested_container_parent_shape,
                               nested_container_child_shape>();
}

} // namespace dingo::matrix
