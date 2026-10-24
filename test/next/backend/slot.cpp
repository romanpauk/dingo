//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The slot of a binding and what it keeps beside its object: the objects the
// cached rules of its routes derive, which live as long as the object of the
// slot and are forgotten when a failed transaction resets it; and the address
// a request type borrows from a settled slot of a stable scope, which the
// container keeps so a repeated request does not reach the slot again.
// spec: transaction_rules

#include <dingo/backend/slot.h>
#include <dingo/container.h>
#include <dingo/conversion/convert.h>
#include <dingo/primitives/retained.h>
#include <dingo/primitives/slot.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {

using dingo::scope;
using dingo::storage;

struct iface {
  virtual ~iface() = default;
};
struct impl : iface {};
struct leaf {};

using handle_binding =
    dingo::core::binding_type<dingo::shared, std::shared_ptr<impl>,
                              dingo::core::type_list<iface>>;
using leaf_binding =
    dingo::core::binding_type<dingo::shared, leaf, dingo::core::type_list<>>;

// A resolver of a slot that is only asked for its transaction.
struct test_resolver {
  dingo::core::transaction_log log;
  dingo::core::transaction_log &transactions() { return log; }
};

// The handle of the interface the binding keeps in the context of its slot,
// made by `make` when it keeps none; `made` counts the handles made.
std::shared_ptr<iface> &
kept_handle(dingo::core::binding_slot<handle_binding> &slot,
            std::shared_ptr<impl> &object, int &made) {
  using target = std::shared_ptr<iface>;
  return slot.context().get_or_emplace<target>(
      dingo::core::type_address_v<target>, [&] {
        ++made;
        return target(object);
      });
}

TEST(next_slot, keeps_a_derived_object_until_its_object_is_reset) {
  dingo::core::binding_slot<handle_binding> slot;
  test_resolver resolver;
  int made = 0;

  // A request that fails resets the slot it constructed the object of, and
  // the slot forgets the handle derived from it.
  EXPECT_THROW(resolver.transactions().run([&] {
    std::shared_ptr<impl> &object = slot.get(resolver);
    std::shared_ptr<iface> &first = kept_handle(slot, object, made);
    EXPECT_EQ(&kept_handle(slot, object, made), &first);
    EXPECT_EQ(object.use_count(), 2);
    throw std::runtime_error("failed");
  }),
               std::runtime_error);
  EXPECT_EQ(made, 1);
  EXPECT_FALSE(slot.settled(resolver));

  // A request that returns keeps the object and the handle.
  std::shared_ptr<iface> *committed = nullptr;
  resolver.transactions().run([&] {
    std::shared_ptr<impl> &object = slot.get(resolver);
    committed = &kept_handle(slot, object, made);
    EXPECT_EQ(object.use_count(), 2);
  });
  EXPECT_EQ(made, 2);
  EXPECT_TRUE(slot.settled(resolver));

  // A later request that fails reset nothing it did not construct.
  EXPECT_THROW(resolver.transactions().run([&] {
    std::shared_ptr<impl> &object = slot.get(resolver);
    EXPECT_EQ(&kept_handle(slot, object, made), committed);
    throw std::runtime_error("failed");
  }),
               std::runtime_error);
  EXPECT_EQ(made, 2);
  EXPECT_TRUE(slot.settled(resolver));
}

TEST(next_slot,
     a_binding_without_cached_rules_keeps_nothing_beside_its_object) {
  using leaf_slot = dingo::core::binding_slot<leaf_binding>;
  static_assert(std::is_empty_v<dingo::core::no_kept_objects>);
  static_assert(std::is_trivially_destructible_v<dingo::core::no_kept_objects>);
  static_assert(std::is_empty_v<dingo::core::binding_context<leaf_binding>>);
  static_assert(sizeof(leaf_slot) == sizeof(dingo::core::lazy_slot<leaf>));
  // A binding with a cached rule keeps the list of its derived objects.
  static_assert(sizeof(dingo::core::binding_context<handle_binding>) ==
                sizeof(dingo::core::kept_objects));

  leaf_slot slot;
  slot.clear_context();
  test_resolver resolver;
  EXPECT_FALSE(slot.settled(resolver));
  (void)slot.get(resolver);
  EXPECT_TRUE(slot.settled(resolver));
}

// A slot that counts how often a request reaches it, and that is settled
// once its object is constructed.
template <typename Binding>
class counting_slot : public dingo::scope_slot<Binding> {
public:
  using stored_type = typename Binding::stored_type;
  using dingo::scope_slot<Binding>::scope_slot;

  ~counting_slot() {
    if (object_ != nullptr) {
      object_->~stored_type();
    }
  }

  template <typename Resolver> stored_type &get(Resolver &resolver) {
    ++reached;
    if (object_ == nullptr) {
      this->construct(storage_, resolver);
      object_ = std::launder(reinterpret_cast<stored_type *>(storage_));
    }
    return *object_;
  }

  template <typename Resolver> bool settled(Resolver & /*resolver*/) const {
    return object_ != nullptr;
  }

  static inline int reached = 0;

private:
  alignas(stored_type) unsigned char storage_[sizeof(stored_type)];
  stored_type *object_ = nullptr;
};

struct stable_scope {};
struct unstable_scope {};

constexpr dingo::scope_descriptor unstable_descriptor() {
  dingo::scope_descriptor descriptor =
      dingo::scope_traits<dingo::shared>::descriptor;
  descriptor.stable = false;
  return descriptor;
}

} // namespace

template <> struct dingo::scope_traits<stable_scope> {
  static constexpr dingo::scope_descriptor descriptor =
      dingo::scope_traits<dingo::shared>::descriptor;
  template <typename Binding> using slot = counting_slot<Binding>;
};

template <> struct dingo::scope_traits<unstable_scope> {
  static constexpr dingo::scope_descriptor descriptor = unstable_descriptor();
  template <typename Binding> using slot = counting_slot<Binding>;
};

namespace {

struct stable_object {
  stable_object() : value(3) {}
  int value;
};
struct unstable_object {
  unstable_object() : value(5) {}
  int value;
};

TEST(next_slot, a_settled_stable_object_is_borrowed_once_per_request_type) {
  using slot =
      counting_slot<dingo::core::binding_type<stable_scope, stable_object,
                                              dingo::core::type_list<>>>;
  slot::reached = 0;
  dingo::container<> container;
  container.register_type<scope<stable_scope>, storage<stable_object>>();

  stable_object &object = container.resolve<stable_object &>();
  EXPECT_EQ(slot::reached, 1);
  EXPECT_EQ(&container.resolve<stable_object &>(), &object);
  EXPECT_EQ(slot::reached, 1);

  // Every request type keeps the address it borrowed on its own: a const
  // reference and a pointer reach the slot once each.
  EXPECT_EQ(&container.resolve<const stable_object &>(), &object);
  EXPECT_EQ(&container.resolve<const stable_object &>(), &object);
  EXPECT_EQ(slot::reached, 2);
  EXPECT_EQ(container.resolve<stable_object *>(), &object);
  EXPECT_EQ(container.resolve<stable_object *>(), &object);
  EXPECT_EQ(slot::reached, 3);
}

TEST(next_slot, an_unstable_scope_is_reached_by_every_request) {
  static_assert(dingo::scope_traits<dingo::shared>::descriptor.stable);
  static_assert(dingo::scope_traits<dingo::external>::descriptor.stable);
  static_assert(!dingo::scope_traits<dingo::unique>::descriptor.stable);
  static_assert(
      !dingo::scope_traits<dingo::shared_cyclical>::descriptor.stable);
  static_assert(!dingo::scope_traits<unstable_scope>::descriptor.stable);

  using slot =
      counting_slot<dingo::core::binding_type<unstable_scope, unstable_object,
                                              dingo::core::type_list<>>>;
  slot::reached = 0;
  dingo::container<> container;
  container.register_type<scope<unstable_scope>, storage<unstable_object>>();

  unstable_object &object = container.resolve<unstable_object &>();
  EXPECT_EQ(&container.resolve<unstable_object &>(), &object);
  EXPECT_EQ(&container.resolve<unstable_object &>(), &object);
  EXPECT_EQ(slot::reached, 3);
}

// The ports of the rollback checks of the conversion cache of the baseline,
// through the public API.
struct rollback_implementation : iface {};

struct failing_consumer {
  explicit failing_consumer(std::shared_ptr<iface> &handle) {
    if (first_handle == nullptr) {
      first_handle = &handle;
    } else {
      second_handle = &handle;
    }
    if (should_throw) {
      throw std::runtime_error("resolution failed");
    }
  }

  static inline bool should_throw = true;
  static inline std::shared_ptr<iface> *first_handle = nullptr;
  static inline std::shared_ptr<iface> *second_handle = nullptr;

  static void clear() {
    should_throw = true;
    first_handle = nullptr;
    second_handle = nullptr;
  }
};

TEST(next_slot, a_failed_request_forgets_the_converted_handle) {
  failing_consumer::clear();
  dingo::container<> container;
  container.register_type<scope<dingo::shared>,
                          storage<std::shared_ptr<rollback_implementation>>,
                          dingo::interfaces<iface, rollback_implementation>>();
  container.register_type<scope<dingo::shared>, storage<failing_consumer>>();

  EXPECT_THROW((container.resolve<failing_consumer &>()), std::runtime_error);
  ASSERT_NE(failing_consumer::first_handle, nullptr);
  // The handle constructed during the failure is destroyed with the handle
  // converted from it: the new object is shared by no kept handle.
  auto &implementation =
      container.resolve<std::shared_ptr<rollback_implementation> &>();
  EXPECT_EQ(implementation.use_count(), 1);

  failing_consumer::should_throw = false;
  (void)container.resolve<failing_consumer &>();
  ASSERT_NE(failing_consumer::second_handle, nullptr);
  EXPECT_EQ(implementation.use_count(), 2);

  auto &first = container.resolve<std::shared_ptr<iface> &>();
  auto &second = container.resolve<std::shared_ptr<iface> &>();
  EXPECT_EQ(&first, &second);
  EXPECT_EQ(&first, failing_consumer::second_handle);
}

TEST(next_slot, a_committed_converted_handle_survives_a_later_failure) {
  failing_consumer::clear();
  dingo::container<> container;
  container.register_type<scope<dingo::shared>,
                          storage<std::shared_ptr<rollback_implementation>>,
                          dingo::interfaces<iface, rollback_implementation>>();

  auto &committed = container.resolve<std::shared_ptr<iface> &>();
  container.register_type<scope<dingo::shared>, storage<failing_consumer>>();
  EXPECT_THROW((container.resolve<failing_consumer &>()), std::runtime_error);

  auto &after_failure = container.resolve<std::shared_ptr<iface> &>();
  EXPECT_EQ(&after_failure, &committed);
  EXPECT_EQ(after_failure.get(), committed.get());
  EXPECT_EQ(failing_consumer::first_handle, &committed);
}

struct pointer_consumer {
  explicit pointer_consumer(int *) {
    throw std::runtime_error("resolution failed");
  }
};

TEST(next_slot, a_failed_request_keeps_the_address_of_another_request_type) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<int>>();
  container.register_type<scope<dingo::shared>, storage<pointer_consumer>>();

  int &committed = container.resolve<int &>();
  EXPECT_THROW((container.resolve<pointer_consumer &>()), std::runtime_error);
  EXPECT_EQ(&container.resolve<int &>(), &committed);
  EXPECT_EQ(container.resolve<int *>(), &committed);
}

struct ordered_element {
  ordered_element() : value(next++) {}
  ~ordered_element() { order->push_back(value); }

  int value;
  static inline int next = 0;
  static inline std::vector<int> *order = nullptr;
};

// A shared array destroys its elements in the reverse order of their
// construction, as the language does.
TEST(next_slot, a_shared_array_destroys_its_elements_in_reverse_order) {
  std::vector<int> order;
  ordered_element::next = 0;
  ordered_element::order = &order;
  {
    dingo::container<> container;
    container
        .register_type<scope<dingo::shared>, storage<ordered_element[3]>>();
    auto &elements = container.resolve<ordered_element(&)[3]>();
    EXPECT_EQ(elements[2].value, 2);
  }
  EXPECT_EQ(order, (std::vector<int>{2, 1, 0}));
}

} // namespace
