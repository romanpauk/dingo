//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The requests a binding serves from the object it stores: every reference and
// pointer of the object refers to the one stored object, a value request
// copies it once, a move-only or immovable object is referred to and never
// copied, and an object behind layers is reached through them. spec: routes

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/type/type_conversion_traits.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace {

using dingo::scope;
using dingo::storage;

struct copy_counted {
  copy_counted() = default;
  copy_counted(const copy_counted &other) : copies(other.copies + 1) {}

  int copies = 0;
};

struct other_leaf {};

struct move_only_object {
  move_only_object() = default;
  move_only_object(move_only_object &&) = default;
  move_only_object &operator=(move_only_object &&) = default;
};

struct base_interface {
  virtual ~base_interface() = default;
};
struct other_interface {
  virtual ~other_interface() = default;
};
struct leaf : base_interface, other_interface {};

struct custom_copy {
  custom_copy() : value(1) {}

  int value;
};

} // namespace

template <> struct dingo::type_conversion_traits<custom_copy, custom_copy> {
  template <typename> using required_access = dingo::borrow;

  static custom_copy convert(const custom_copy &source) {
    custom_copy result;
    result.value = source.value + 10;
    return result;
  }
};

namespace {

TEST(next_object_routes, a_shared_leaf_serves_its_references_and_copies) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<copy_counted>>();

  copy_counted &reference = container.resolve<copy_counted &>();
  EXPECT_EQ(container.resolve<copy_counted *>(), &reference);
  EXPECT_EQ(&container.resolve<const copy_counted &>(), &reference);
  EXPECT_EQ(container.resolve<const copy_counted *>(), &reference);

  // The copy is taken from the stored object once.
  const copy_counted copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, &reference);
  EXPECT_EQ(container.resolve<const copy_counted>().copies, 1);
  EXPECT_EQ(reference.copies, 0);
}

TEST(next_object_routes, a_shared_leaf_serves_no_move_and_no_other_leaf) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<copy_counted>>();

  EXPECT_THROW(container.resolve<copy_counted &&>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<other_leaf &>(),
               dingo::type_not_found_exception);
  EXPECT_THROW(container.resolve<other_leaf *>(),
               dingo::type_not_found_exception);
}

TEST(next_object_routes, a_move_only_object_serves_references_and_no_copy) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<move_only_object>>();

  move_only_object &reference = container.resolve<move_only_object &>();
  EXPECT_EQ(container.resolve<move_only_object *>(), &reference);
  EXPECT_THROW(container.resolve<move_only_object>(),
               dingo::type_not_convertible_exception);
}

TEST(next_object_routes, a_const_object_serves_const_references_and_copies) {
  const copy_counted object;
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>, storage<const copy_counted &>>(
          object);

  EXPECT_EQ(&container.resolve<const copy_counted &>(), &object);
  EXPECT_EQ(container.resolve<const copy_counted *>(), &object);
  EXPECT_EQ(container.resolve<copy_counted>().copies, 1);
}

TEST(next_object_routes, a_const_object_serves_no_mutable_reference) {
  const copy_counted object;
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>, storage<const copy_counted &>>(
          object);

  EXPECT_THROW(container.resolve<copy_counted &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<copy_counted *>(),
               dingo::type_not_convertible_exception);
}

TEST(next_object_routes,
     an_external_unique_handle_serves_a_copy_of_its_object) {
  auto handle = std::make_unique<copy_counted>();
  copy_counted *object = handle.get();
  dingo::container<> container;
  container.register_type<scope<dingo::external>,
                          storage<std::unique_ptr<copy_counted>>>(
      std::move(handle));

  EXPECT_EQ(&container.resolve<copy_counted &>(), object);
  EXPECT_EQ(container.resolve<copy_counted *>(), object);
  const copy_counted copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, object);
}

TEST(next_object_routes,
     an_external_unique_handle_serves_no_copy_of_a_move_only_object) {
  auto handle = std::make_unique<move_only_object>();
  move_only_object *object = handle.get();
  dingo::container<> container;
  container.register_type<scope<dingo::external>,
                          storage<std::unique_ptr<move_only_object>>>(
      std::move(handle));

  EXPECT_EQ(&container.resolve<move_only_object &>(), object);
  EXPECT_THROW(container.resolve<move_only_object>(),
               dingo::type_not_convertible_exception);
}

TEST(next_object_routes, a_shared_pointer_serves_a_copy_of_its_pointee) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<copy_counted *>>(
      dingo::callable([]() { return new copy_counted; }));

  copy_counted &object = container.resolve<copy_counted &>();
  EXPECT_EQ(container.resolve<copy_counted *>(), &object);
  const copy_counted copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, &object);
  // The binding does not delete the pointee, unlike the baseline; see the
  // notes of the coverage ledger.
  delete &object;
}

TEST(next_object_routes,
     a_shared_pointer_serves_no_copy_of_a_move_only_pointee) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<move_only_object *>>(
      dingo::callable([]() { return new move_only_object; }));

  move_only_object &object = container.resolve<move_only_object &>();
  EXPECT_EQ(container.resolve<move_only_object *>(), &object);
  EXPECT_THROW(container.resolve<move_only_object>(),
               dingo::type_not_convertible_exception);
  delete &object;
}

struct destroy_counted {
  ~destroy_counted() { ++destructions; }
  static inline int destructions = 0;
};

// The baseline destroys the object the factory of a shared pointer returns
// with the container, and the next implementation leaves it alive, a
// difference no behavior change declares; see the notes of the coverage
// ledger (build/ledger/fragments/routes_resolution.md).
TEST(next_object_routes,
     DISABLED_a_shared_pointer_owns_the_object_its_factory_returns) {
  destroy_counted::destructions = 0;
  {
    dingo::container<> container;
    container.register_type<scope<dingo::shared>, storage<destroy_counted *>>(
        dingo::callable([]() { return new destroy_counted; }));
    (void)container.resolve<destroy_counted &>();
    EXPECT_EQ(destroy_counted::destructions, 0);
  }
  EXPECT_EQ(destroy_counted::destructions, 1);
}

// The baseline copies a leaf by the type_conversion_traits it declares to
// itself, and the next implementation copies it by its copy constructor, a
// difference no behavior change declares; see the notes of the coverage
// ledger (build/ledger/fragments/routes_resolution.md).
TEST(next_object_routes, DISABLED_a_custom_copy_conversion_copies_the_leaf) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<custom_copy>>();

  custom_copy &reference = container.resolve<custom_copy &>();
  EXPECT_EQ(container.resolve<custom_copy *>(), &reference);
  EXPECT_EQ(container.resolve<custom_copy>().value, reference.value + 10);
}

TEST(next_object_routes, interfaces_are_served_in_place_of_the_leaf) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<leaf>,
                          dingo::interfaces<base_interface, other_interface>>();

  base_interface &base = container.resolve<base_interface &>();
  EXPECT_EQ(container.resolve<base_interface *>(), &base);
  other_interface &other = container.resolve<other_interface &>();
  EXPECT_EQ(container.resolve<other_interface *>(), &other);
  EXPECT_EQ(dynamic_cast<leaf *>(&base), dynamic_cast<leaf *>(&other));
  EXPECT_THROW(container.resolve<leaf &>(), dingo::type_not_found_exception);
}

TEST(next_object_routes, a_shared_handle_serves_itself_and_its_leaf) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>,
                          storage<std::shared_ptr<copy_counted>>>();

  copy_counted &leaf_reference = container.resolve<copy_counted &>();
  EXPECT_EQ(container.resolve<copy_counted *>(), &leaf_reference);
  auto &handle = container.resolve<std::shared_ptr<copy_counted> &>();
  EXPECT_EQ(container.resolve<std::shared_ptr<copy_counted> *>(), &handle);
  EXPECT_EQ(&container.resolve<const std::shared_ptr<copy_counted> &>(),
            &handle);
  EXPECT_EQ(handle.get(), &leaf_reference);
  EXPECT_EQ(container.resolve<std::shared_ptr<copy_counted>>().get(),
            &leaf_reference);
  EXPECT_EQ(container.resolve<copy_counted>().copies, 1);
}

TEST(next_object_routes, a_pointer_storage_serves_its_pointee) {
  copy_counted object;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<copy_counted *>>(
      &object);

  EXPECT_EQ(&container.resolve<copy_counted &>(), &object);
  EXPECT_EQ(container.resolve<copy_counted *>(), &object);
  EXPECT_EQ(container.resolve<copy_counted>().copies, 1);
}

TEST(next_object_routes, an_external_shared_handle_serves_a_copy_of_its_leaf) {
  auto handle = std::make_shared<copy_counted>();
  dingo::container<> container;
  container.register_type<scope<dingo::external>,
                          storage<std::shared_ptr<copy_counted>>>(handle);

  EXPECT_EQ(&container.resolve<copy_counted &>(), handle.get());
  EXPECT_EQ(container.resolve<copy_counted *>(), handle.get());
  EXPECT_EQ(container.resolve<std::shared_ptr<copy_counted>>(), handle);
  const copy_counted copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, handle.get());
}

TEST(next_object_routes, an_external_nested_handle_serves_a_copy_of_its_leaf) {
  auto handle = std::make_shared<std::optional<copy_counted>>(std::in_place);
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>,
                     storage<std::shared_ptr<std::optional<copy_counted>>>>(
          handle);

  EXPECT_EQ(&container.resolve<copy_counted &>(), &**handle);
  const copy_counted copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, &**handle);
}

TEST(next_object_routes,
     an_external_nested_handle_serves_no_copy_of_a_move_only_leaf) {
  auto handle =
      std::make_shared<std::optional<move_only_object>>(std::in_place);
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>,
                     storage<std::shared_ptr<std::optional<move_only_object>>>>(
          handle);

  EXPECT_EQ(&container.resolve<move_only_object &>(), &**handle);
  EXPECT_THROW(container.resolve<move_only_object>(),
               dingo::type_not_convertible_exception);
}

} // namespace
