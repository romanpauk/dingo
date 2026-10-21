//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The requests a binding serves of the shapes it stores: pointers to layers,
// const and volatile objects, smart pointers to const objects, alternatives,
// arrays of arrays, and the conversions of its leaf and of its handles to
// the declared interfaces. Ported from the registration traits tests of the
// current implementation, which checked them on its internal binding model.

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/function.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace {

using dingo::scope;
using dingo::storage;

struct service {
  int value;
};

struct move_only_service {
  explicit move_only_service(int init_value) : value(init_value) {}
  move_only_service(const move_only_service &) = delete;
  move_only_service(move_only_service &&) = default;
  int value;
};

// A sharing handle of the user, whose conversion to the handle of a base
// records whether it borrowed its source or consumed it.
template <typename Type> class user_shared {
public:
  user_shared() = default;
  explicit user_shared(std::shared_ptr<Type> pointer)
      : pointer_(std::move(pointer)) {}

  Type *get() const { return pointer_.get(); }
  bool converted_from_const() const { return from_const_; }

private:
  template <typename> friend class user_shared;
  template <typename, typename, typename>
  friend struct dingo::type_conversion_traits;

  std::shared_ptr<Type> pointer_;
  bool from_const_ = false;
};

struct user_family {};

// A user optional, which holds its element in place.
template <typename Type> class user_optional {
public:
  user_optional() = default;
  explicit user_optional(Type value) : value_(std::move(value)) {}

  Type *get() { return value_ ? std::addressof(*value_) : nullptr; }
  const Type *get() const { return value_ ? std::addressof(*value_) : nullptr; }
  bool has_value() const { return value_.has_value(); }

private:
  std::optional<Type> value_;
};

} // namespace

namespace dingo {

template <typename Type> struct wrapper_traits<user_shared<Type>> {
  using kind = shared_kind;
  using family = user_family;
  using arguments = type_list<Type>;
  template <typename Other> using rebind = user_shared<Other>;

  static Type &element(const user_shared<Type> &wrapper) {
    return *wrapper.get();
  }

  template <typename... Args> static user_shared<Type> make(Args &&...args) {
    return user_shared<Type>(
        std::shared_ptr<Type>(new Type{std::forward<Args>(args)...}));
  }

  static user_shared<Type> adopt(Type *element) {
    return user_shared<Type>(std::shared_ptr<Type>(element));
  }
};

template <typename Type> struct wrapper_traits<user_optional<Type>> {
  using kind = optional_kind;
  using family = user_family;
  using arguments = type_list<Type>;
  template <typename Other> using rebind = user_optional<Other>;

  static Type &element(user_optional<Type> &wrapper) { return *wrapper.get(); }
  static const Type &element(const user_optional<Type> &wrapper) {
    return *wrapper.get();
  }
  static Type &&element(user_optional<Type> &&wrapper) {
    return std::move(*wrapper.get());
  }

  template <typename... Args> static user_optional<Type> make(Args &&...args) {
    if constexpr (std::is_constructible_v<Type, Args...>) {
      return user_optional<Type>(Type(std::forward<Args>(args)...));
    } else {
      return user_optional<Type>(Type{std::forward<Args>(args)...});
    }
  }
};

template <typename Target, typename Source>
struct type_conversion_traits<
    user_shared<Target>, user_shared<Source>,
    std::enable_if_t<std::is_convertible_v<Source *, Target *> &&
                     !std::is_same_v<Source, Target>>> {
  template <typename From>
  using required_access =
      std::conditional_t<std::is_rvalue_reference_v<From>, consume, borrow>;

  static user_shared<Target> convert(const user_shared<Source> &source) {
    user_shared<Target> converted(
        std::static_pointer_cast<Target>(source.pointer_));
    converted.from_const_ = true;
    return converted;
  }

  static user_shared<Target> convert(user_shared<Source> &&source) {
    return user_shared<Target>(
        std::static_pointer_cast<Target>(std::move(source.pointer_)));
  }
};

} // namespace dingo

namespace {

// spec: routes, a borrowing scope reaches the objects behind a pointer
TEST(next_stored_shapes, a_pointer_to_a_layer_serves_the_layer_and_a_copy) {
  std::optional<service> optional{service{7}};
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>, storage<std::optional<service> *>>(
          &optional);
  EXPECT_EQ(container.resolve<std::optional<service> *>(), &optional);
  EXPECT_EQ(&container.resolve<std::optional<service> &>(), &optional);
  std::optional<service> copy = container.resolve<std::optional<service>>();
  ASSERT_TRUE(copy.has_value());
  EXPECT_EQ(copy->value, 7);

  const std::array<int, 2> array{3, 5};
  dingo::container<> array_container;
  array_container.register_type<scope<dingo::external>,
                                storage<const std::array<int, 2> *>>(&array);
  EXPECT_EQ((array_container.resolve<std::array<int, 2>>()), array);

  auto handle = std::make_shared<service>(service{9});
  dingo::container<> handle_container;
  handle_container.register_type<scope<dingo::external>,
                                 storage<std::shared_ptr<service> *>>(&handle);
  EXPECT_EQ(handle_container.resolve<std::shared_ptr<service>>(), handle);
}

TEST(next_stored_shapes, a_pointer_to_a_move_only_object_serves_no_copy) {
  move_only_service value{10};
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<move_only_service *>>(
      &value);
  EXPECT_THROW(container.resolve<move_only_service>(),
               dingo::type_not_convertible_exception);
  EXPECT_EQ(value.value, 10);
  EXPECT_EQ(container.resolve<move_only_service *>(), &value);

  std::optional<move_only_service> optional{std::in_place, 12};
  dingo::container<> optional_container;
  optional_container.register_type<scope<dingo::external>,
                                   storage<std::optional<move_only_service> *>>(
      &optional);
  EXPECT_THROW(optional_container.resolve<std::optional<move_only_service>>(),
               dingo::type_not_convertible_exception);
  ASSERT_TRUE(optional.has_value());
  EXPECT_EQ(optional->value, 12);
}

TEST(next_stored_shapes, a_null_external_pointer_is_served_as_null) {
  using alternatives = std::variant<int, float>;
  const alternatives *null = nullptr;
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>, storage<const alternatives *>>(
          null);
  EXPECT_EQ(container.resolve<const alternatives *>(), nullptr);
}

TEST(next_stored_shapes, a_shared_pointer_to_a_const_layer_is_one_object) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>,
                          storage<const std::optional<service> *>>(
      dingo::callable([]() -> const std::optional<service> * {
        return new std::optional<service>{service{11}};
      }));
  const std::optional<service> *pointer =
      container.resolve<const std::optional<service> *>();
  ASSERT_NE(pointer, nullptr);
  ASSERT_TRUE(pointer->has_value());
  EXPECT_EQ(pointer->value().value, 11);
  EXPECT_EQ(container.resolve<const std::optional<service> *>(), pointer);
  EXPECT_EQ(&container.resolve<const std::optional<service> &>(), pointer);
}

TEST(next_stored_shapes, a_unique_pointer_to_a_layer_is_adopted_by_handles) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>,
                          storage<std::optional<service> *>>(
      dingo::callable([] { return new std::optional<service>{service{13}}; }));
  auto owned = container.resolve<std::unique_ptr<std::optional<service>>>();
  ASSERT_TRUE(owned->has_value());
  EXPECT_EQ(owned->value().value, 13);
  auto &&retained =
      container.resolve<std::unique_ptr<std::optional<service>> &&>();
  ASSERT_TRUE(retained->has_value());
  EXPECT_EQ(retained->value().value, 13);
  auto shared = container.resolve<std::shared_ptr<std::optional<service>>>();
  ASSERT_TRUE(shared->has_value());
  EXPECT_EQ(shared->value().value, 13);
}

struct plain_interface {
  int value;
};

struct secondary {
  virtual ~secondary() = default;
};

struct implementation : plain_interface, secondary {
  explicit implementation(int init) { value = init; }
};

TEST(next_stored_shapes, a_pointer_to_an_optional_serves_its_interfaces) {
  std::optional<implementation> value{std::in_place, 17};
  dingo::container<> container;
  container.register_type<scope<dingo::external>,
                          storage<std::optional<implementation> *>,
                          dingo::interfaces<plain_interface, secondary>>(
      &value);
  EXPECT_EQ(&container.resolve<plain_interface &>(),
            static_cast<plain_interface *>(&*value));
  EXPECT_EQ(container.resolve<plain_interface>().value, 17);
  EXPECT_EQ(container.resolve<secondary *>(),
            static_cast<secondary *>(&*value));

  user_optional<implementation> custom{implementation{19}};
  dingo::container<> custom_container;
  custom_container.register_type<scope<dingo::external>,
                                 storage<user_optional<implementation> *>,
                                 dingo::interfaces<plain_interface, secondary>>(
      &custom);
  EXPECT_EQ(&custom_container.resolve<plain_interface &>(),
            static_cast<plain_interface *>(custom.get()));
  EXPECT_EQ(custom_container.resolve<plain_interface>().value, 19);
}

// Finding of the ledger: the current implementation serves an optional of an
// interface converted from a borrowed optional of the implementation, empty
// when the stored optional is empty; the next implementation refuses it with
// type_not_convertible_exception, and no behavior change declares it.
TEST(next_stored_shapes,
     DISABLED_a_pointer_to_an_optional_serves_an_optional_of_its_interface) {
  std::optional<implementation> value{std::in_place, 17};
  dingo::container<> container;
  container.register_type<scope<dingo::external>,
                          storage<std::optional<implementation> *>,
                          dingo::interfaces<plain_interface, secondary>>(
      &value);
  std::optional<plain_interface> converted =
      container.resolve<std::optional<plain_interface>>();
  ASSERT_TRUE(converted.has_value());
  EXPECT_EQ(converted->value, 17);

  std::optional<implementation> empty;
  dingo::container<> empty_container;
  empty_container.register_type<scope<dingo::external>,
                                storage<std::optional<implementation> *>,
                                dingo::interfaces<plain_interface, secondary>>(
      &empty);
  EXPECT_FALSE(
      empty_container.resolve<std::optional<plain_interface>>().has_value());

  user_optional<implementation> custom{implementation{19}};
  dingo::container<> custom_container;
  custom_container.register_type<scope<dingo::external>,
                                 storage<user_optional<implementation> *>,
                                 dingo::interfaces<plain_interface, secondary>>(
      &custom);
  auto custom_converted =
      custom_container.resolve<user_optional<plain_interface>>();
  ASSERT_NE(custom_converted.get(), nullptr);
  EXPECT_EQ(custom_converted.get()->value, 19);
}

std::optional<int> no_element() { return std::nullopt; }

// The element of an empty optional is a null address, and is refused to any
// other request, which has nothing to refer to or copy; once the optional
// holds an element, the requests are served by it.
TEST(next_stored_shapes, the_element_of_an_empty_optional_is_null) {
  std::optional<int> optional;
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>, storage<std::optional<int> &>>(
          optional);
  EXPECT_EQ(container.resolve<const int *>(), nullptr);
  EXPECT_EQ(container.resolve<int *>(), nullptr);
  EXPECT_THROW(container.resolve<const int &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<int>(), dingo::type_not_convertible_exception);
  optional = 5;
  EXPECT_EQ(container.resolve<int *>(), &*optional);
  EXPECT_EQ(container.resolve<int>(), 5);

  dingo::static_container<dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<std::optional<int>>,
                  dingo::factory<dingo::function<&no_element>>>>>
      static_container;
  EXPECT_EQ(static_container.resolve<int *>(), nullptr);
  EXPECT_THROW(static_container.resolve<int &>(),
               dingo::type_not_convertible_exception);
}

TEST(next_stored_shapes, const_storage_serves_only_const_access) {
  const int value = 7;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<const int &>>(value);
  EXPECT_EQ(container.resolve<int>(), 7);
  EXPECT_EQ(&container.resolve<const int &>(), &value);
  EXPECT_EQ(container.resolve<const int *>(), &value);
  EXPECT_THROW(container.resolve<int &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<int *>(),
               dingo::type_not_convertible_exception);

  const std::optional<int> optional{11};
  dingo::container<> optional_container;
  optional_container.register_type<scope<dingo::external>,
                                   storage<const std::optional<int> &>>(
      optional);
  EXPECT_EQ(optional_container.resolve<int>(), 11);
  EXPECT_EQ(&optional_container.resolve<const int &>(), &*optional);
  EXPECT_EQ(optional_container.resolve<const int *>(), &*optional);
  EXPECT_EQ(&optional_container.resolve<const std::optional<int> &>(),
            &optional);
  EXPECT_THROW(optional_container.resolve<int &>(),
               dingo::type_not_convertible_exception);

  const user_optional<int> custom{13};
  dingo::container<> custom_container;
  custom_container.register_type<scope<dingo::external>,
                                 storage<const user_optional<int> &>>(custom);
  EXPECT_EQ(custom_container.resolve<int>(), 13);
  EXPECT_EQ(&custom_container.resolve<const int &>(), custom.get());
  EXPECT_EQ(custom_container.resolve<const int *>(), custom.get());
}

TEST(next_stored_shapes, const_nested_layers_serve_only_const_access) {
  using nested = std::optional<std::variant<int, float>>;
  const nested value{std::in_place, std::in_place_type<int>, 17};
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<const nested &>>(
      value);
  EXPECT_EQ(container.resolve<int>(), 17);
  EXPECT_EQ(&container.resolve<const int &>(), &std::get<int>(*value));
  EXPECT_EQ(container.resolve<const int *>(), &std::get<int>(*value));
  EXPECT_THROW(container.resolve<int &>(),
               dingo::type_not_convertible_exception);

  using doubly_nested = std::optional<nested>;
  const doubly_nested doubly{std::in_place, std::in_place,
                             std::in_place_type<int>, 19};
  dingo::container<> doubly_container;
  doubly_container
      .register_type<scope<dingo::external>, storage<const doubly_nested &>>(
          doubly);
  EXPECT_EQ(doubly_container.resolve<int>(), 19);
  EXPECT_EQ(&doubly_container.resolve<const int &>(), &std::get<int>(**doubly));
  EXPECT_EQ(doubly_container.resolve<const int *>(), &std::get<int>(**doubly));
}

TEST(next_stored_shapes, a_const_alternative_serves_only_const_access) {
  using alternatives = std::variant<int, float>;
  const alternatives value{std::in_place_type<int>, 13};
  dingo::container<> container;
  container
      .register_type<scope<dingo::external>, storage<const alternatives &>>(
          value);
  EXPECT_EQ(container.resolve<int>(), 13);
  EXPECT_EQ(&container.resolve<const int &>(), &std::get<int>(value));
  EXPECT_EQ(container.resolve<const int *>(), &std::get<int>(value));
  EXPECT_THROW(container.resolve<int &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<int *>(),
               dingo::type_not_convertible_exception);
}

TEST(next_stored_shapes, volatile_storage_keeps_the_volatile_qualifier) {
  volatile int value = 3;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<volatile int &>>(
      value);
  EXPECT_EQ(container.resolve<int>(), 3);
  EXPECT_EQ(&container.resolve<volatile int &>(), &value);
  EXPECT_EQ(&container.resolve<const volatile int &>(), &value);
  EXPECT_THROW(container.resolve<int &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<const int &>(),
               dingo::type_not_convertible_exception);
}

struct polymorphic_interface {
  virtual ~polymorphic_interface() = default;
  virtual int get_value() const = 0;
};

struct polymorphic_service : polymorphic_interface {
  explicit polymorphic_service(int init_value) : value(init_value) {}
  int get_value() const override { return value; }
  int value;
};

TEST(next_stored_shapes, a_handle_of_a_const_object_serves_only_const_access) {
  const std::shared_ptr<const service> shared =
      std::make_shared<const service>(service{23});
  dingo::container<> container;
  container.register_type<scope<dingo::external>,
                          storage<const std::shared_ptr<const service> &>>(
      shared);
  EXPECT_EQ(container.resolve<std::shared_ptr<const service>>(), shared);
  EXPECT_EQ(&container.resolve<const std::shared_ptr<const service> &>(),
            &shared);
  EXPECT_EQ(&container.resolve<const service &>(), shared.get());
  EXPECT_EQ(container.resolve<const service *>(), shared.get());
  EXPECT_THROW(container.resolve<service &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<service *>(),
               dingo::type_not_convertible_exception);

  const std::unique_ptr<const service> unique =
      std::make_unique<const service>(service{29});
  dingo::container<> unique_container;
  unique_container.register_type<
      scope<dingo::external>, storage<const std::unique_ptr<const service> &>>(
      unique);
  EXPECT_EQ(&unique_container.resolve<const std::unique_ptr<const service> &>(),
            &unique);
  EXPECT_EQ(&unique_container.resolve<const service &>(), unique.get());
  EXPECT_EQ(unique_container.resolve<const service *>(), unique.get());
  EXPECT_THROW(unique_container.resolve<service &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(unique_container.resolve<service *>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(unique_container.resolve<std::unique_ptr<const service>>(),
               dingo::type_not_convertible_exception);
}

// A const object serves its declared interfaces as const ones, through a
// handle, a reference, or an address.
TEST(next_stored_shapes,
     a_handle_of_a_const_object_serves_only_const_interfaces) {
  const std::shared_ptr<const polymorphic_service> shared =
      std::make_shared<const polymorphic_service>(31);
  dingo::container<> container;
  container.register_type<
      scope<dingo::external>,
      storage<const std::shared_ptr<const polymorphic_service> &>,
      dingo::interfaces<polymorphic_interface>>(shared);
  EXPECT_EQ(
      container.resolve<std::shared_ptr<const polymorphic_interface>>().get(),
      shared.get());
  EXPECT_EQ(&container.resolve<const polymorphic_interface &>(), shared.get());
  EXPECT_EQ(container.resolve<const polymorphic_interface *>(), shared.get());
  EXPECT_THROW(container.resolve<polymorphic_interface &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(container.resolve<polymorphic_interface *>(),
               dingo::type_not_convertible_exception);

  const std::unique_ptr<const polymorphic_service> unique =
      std::make_unique<const polymorphic_service>(37);
  dingo::container<> unique_container;
  unique_container.register_type<
      scope<dingo::external>,
      storage<const std::unique_ptr<const polymorphic_service> &>,
      dingo::interfaces<polymorphic_interface>>(unique);
  EXPECT_EQ(&unique_container.resolve<const polymorphic_interface &>(),
            unique.get());
  EXPECT_EQ(unique_container.resolve<const polymorphic_interface *>(),
            unique.get());
  EXPECT_THROW(unique_container.resolve<polymorphic_interface &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(unique_container.resolve<polymorphic_interface *>(),
               dingo::type_not_convertible_exception);

  const polymorphic_service object(41);
  dingo::container<> reference_container;
  reference_container.register_type<scope<dingo::external>,
                                    storage<const polymorphic_service &>,
                                    dingo::interfaces<polymorphic_interface>>(
      object);
  EXPECT_EQ(&reference_container.resolve<const polymorphic_interface &>(),
            &object);
  EXPECT_EQ(reference_container.resolve<const polymorphic_interface *>(),
            &object);
  EXPECT_THROW(reference_container.resolve<polymorphic_interface &>(),
               dingo::type_not_convertible_exception);

  // The interface of a const alternative, which the current implementation
  // declares as interfaces<int>.
  using alternatives = std::variant<int, float>;
  const alternatives value{std::in_place_type<int>, 13};
  dingo::container<> variant_container;
  variant_container
      .register_type<scope<dingo::external>, storage<const alternatives &>,
                     dingo::interfaces<int>>(value);
  EXPECT_EQ(variant_container.resolve<int>(), 13);
  EXPECT_EQ(&variant_container.resolve<const int &>(), &std::get<int>(value));
  EXPECT_EQ(variant_container.resolve<const int *>(), &std::get<int>(value));
}

TEST(next_stored_shapes, an_array_of_arrays_serves_its_rows_and_itself) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<int[2][3]>>();
  int (*rows)[3] = container.resolve<int (*)[3]>();
  int (*whole)[2][3] = container.resolve<int (*)[2][3]>();
  const int (*const_rows)[3] = container.resolve<const int (*)[3]>();
  volatile int (*volatile_rows)[3] = container.resolve<volatile int (*)[3]>();
  const volatile int (*cv_whole)[2][3] =
      container.resolve<const volatile int (*)[2][3]>();
  ASSERT_EQ(&(*whole)[0][0], &rows[0][0]);
  ASSERT_EQ(rows, const_rows);
  ASSERT_EQ(rows, volatile_rows);
  ASSERT_EQ(whole, cv_whole);
  int (&reference)[2][3] = container.resolve<int (&)[2][3]>();
  EXPECT_EQ(&reference, whole);
  (*whole)[1][1] = 7;
  EXPECT_EQ(rows[1][1], 7);
  EXPECT_THROW(container.resolve<int (*)[2]>(),
               dingo::type_not_convertible_exception);
}

struct handle_interface {
  virtual ~handle_interface() = default;
  virtual int value() const = 0;
};

struct handle_implementation : handle_interface {
  int value() const override { return 7; }
};

TEST(next_stored_shapes, a_consumed_user_handle_converts_by_its_traits) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>,
                          storage<user_shared<handle_implementation>>,
                          dingo::interfaces<handle_interface>>();
  auto resolved = container.resolve<user_shared<handle_interface>>();
  ASSERT_NE(resolved.get(), nullptr);
  EXPECT_EQ(resolved.get()->value(), 7);
  // The consumed source is moved from, not copied.
  EXPECT_FALSE(resolved.converted_from_const());
}

TEST(next_stored_shapes, a_borrowed_user_handle_converts_from_a_const_source) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>,
                          storage<user_shared<handle_implementation>>,
                          dingo::interfaces<handle_interface>>();
  auto &converted = container.resolve<user_shared<handle_interface> &>();
  ASSERT_NE(converted.get(), nullptr);
  EXPECT_TRUE(converted.converted_from_const());
  // The stored handle still holds its object.
  EXPECT_EQ(container.resolve<handle_interface &>().value(), 7);
  EXPECT_EQ(&container.resolve<handle_interface &>(), converted.get());
}

struct move_target {
  explicit move_target(int initial_value = 23) : value(initial_value) {}
  move_target(const move_target &) = delete;
  move_target(move_target &&other) noexcept
      : value(std::exchange(other.value, -1)) {}
  int value;
};

struct move_source : move_target, secondary {
  move_source() = default;
};

struct copy_target {
  explicit copy_target(int initial_value = 23) : value(initial_value) {}
  int value;
};

struct copy_source : copy_target, secondary {
  copy_source() = default;
};

} // namespace

namespace dingo {
template <> struct type_conversion_traits<move_target, move_source> {
  template <typename> using required_access = borrow;

  static move_target convert(const move_source &source) {
    return move_target(source.value);
  }
};

template <> struct type_conversion_traits<copy_target, copy_source> {
  template <typename> using required_access = borrow;

  static copy_target convert(const copy_source &) { return copy_target(47); }
};
} // namespace dingo

namespace {

TEST(next_stored_shapes, a_borrowed_leaf_converts_to_a_new_interface_value) {
  move_source moved;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<move_source *>,
                          dingo::interfaces<move_target, secondary>>(&moved);
  // The move-only interface is served by value by the declared conversion,
  // which borrows the source.
  EXPECT_EQ(container.resolve<move_target>().value, 23);
  EXPECT_EQ(moved.value, 23);
  EXPECT_EQ(&container.resolve<move_target &>(),
            static_cast<move_target *>(&moved));

  copy_source copied;
  dingo::container<> copy_container;
  copy_container.register_type<scope<dingo::external>, storage<copy_source *>,
                               dingo::interfaces<copy_target, secondary>>(
      &copied);
  // A declared conversion is preferred to the copy of the base subobject.
  EXPECT_EQ(copy_container.resolve<copy_target>().value, 47);
}

struct local_dependency {
  explicit local_dependency(int init_value) : value(init_value) {}
  int value;
};

local_dependency make_local_dependency() { return local_dependency{7}; }

struct local_service {
  explicit local_service(local_dependency &dependency)
      : value(dependency.value) {}
  int value;
};

TEST(next_stored_shapes,
     local_static_bindings_resolve_the_dependencies_of_their_registration) {
  dingo::container<> container;
  container.register_type<
      scope<dingo::shared>, storage<local_service>,
      dingo::dependencies<local_dependency &>,
      dingo::bindings<dingo::bind<
          scope<dingo::shared>, storage<local_dependency>,
          dingo::factory<dingo::function<&make_local_dependency>>>>>();
  EXPECT_EQ(container.resolve<local_service &>().value, 7);
  EXPECT_THROW(container.resolve<local_dependency &>(),
               dingo::type_not_found_exception);
}

} // namespace
