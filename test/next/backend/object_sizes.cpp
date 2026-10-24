//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The sizes of the objects a runtime container is made of: the container
// itself, which no registration grows, the slot of each scope, which keeps its
// object and only what its scope needs beside it, the transaction log, which
// holds a pointer to the effects recorded in the frame of the outermost
// request, and the lists of retained and kept objects, which hold a pointer to
// their last object. The size of containers as they grow is the contract
// object_size_* of spec/content/contracts.py, which test/contracts/probe.py
// measures. spec: contracts

#include <dingo/backend/slot.h>
#include <dingo/container.h>
#include <dingo/conversion/convert.h>
#include <dingo/memory/retained.h>
#include <dingo/memory/lifetime_slot.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <type_traits>

namespace {

struct leaf {
  int value = 0;
};

struct iface {
  virtual ~iface() = default;
};
struct impl : iface {};

template <typename Scope, typename Stored, typename... Interfaces>
using slot_of = dingo::core::slot_of_t<dingo::core::binding_type<
    Scope, Stored, dingo::core::type_list<Interfaces...>>>;

TEST(next_object_sizes, a_runtime_container_is_a_few_words) {
  if constexpr (sizeof(void *) == 8) {
    // The baseline bound; the contract object_size_runtime_container records
    // the size of each toolchain.
    EXPECT_LE(sizeof(dingo::container<>), 120u);
  }
  // A container is moved between its requests, and holds its effects in the
  // frame of its outermost request.
  static_assert(sizeof(dingo::core::transaction_log) == sizeof(void *));
  static_assert(std::is_empty_v<dingo::core::no_transaction_log>);
}

TEST(next_object_sizes, a_slot_keeps_its_object_and_what_its_scope_needs) {
  // A lazily constructed object and its state.
  static_assert(sizeof(slot_of<dingo::shared, leaf>) ==
                sizeof(dingo::core::lazy_slot<leaf>));
  static_assert(sizeof(dingo::core::lazy_slot<leaf>) <= 2 * sizeof(leaf));
  // An external object is referred to.
  static_assert(sizeof(slot_of<dingo::external, leaf &>) == sizeof(void *));
  static_assert(sizeof(slot_of<dingo::external, leaf *>) == sizeof(void *));
  // A consuming scope keeps no object, only whether it is constructing one.
  static_assert(sizeof(slot_of<dingo::unique, leaf>) == sizeof(bool));
  // A two-phase object and its state.
  static_assert(sizeof(slot_of<dingo::shared_cyclical, leaf>) <=
                sizeof(leaf) + sizeof(void *));
  // A binding that keeps the handles converted from its object adds one
  // pointer to its slot.
  static_assert(sizeof(slot_of<dingo::shared, std::shared_ptr<impl>, iface>) ==
                sizeof(slot_of<dingo::shared, std::shared_ptr<impl>>) +
                    sizeof(void *));
  static_assert(!std::is_polymorphic_v<slot_of<dingo::shared, leaf>>);
}

TEST(next_object_sizes, kept_objects_are_a_list_of_one_pointer) {
  static_assert(sizeof(dingo::core::retained_objects) == sizeof(void *));
  static_assert(sizeof(dingo::core::kept_objects) == sizeof(void *));
  static_assert(std::is_empty_v<dingo::core::no_kept_objects>);
}

} // namespace
