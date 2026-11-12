//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Resolution of wrappers nested two deep and of user-defined wrapper traits.
// Single wrappers over a leaf are covered by spec/probe.py.

#include <dingo/base/exceptions.h>
#include <dingo/container.h>
#include <dingo/factory/function.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include "support/custom_wrappers.h"

#include "support/containers.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

using namespace dingo;

namespace {

struct marked {
  int marker() const { return 3; }
};

struct regular : marked {};

struct move_only : marked {
  move_only() = default;
  move_only(const move_only &) = delete;
  move_only(move_only &&) = default;
};

struct copy_only : marked {
  copy_only() = default;
  copy_only(const copy_only &) = default;
  copy_only(copy_only &&) = delete;
};

struct immovable : marked {
  immovable() = default;
  immovable(const immovable &) = delete;
  immovable(immovable &&) = delete;
};

struct other {
  int marker() const { return 7; }
};

struct service {
  virtual ~service() = default;
  virtual int marker() const = 0;
};

struct service_impl : service {
  int marker() const override { return 3; }
};

// The marker of the leaf a composition holds; -1 when a layer is empty. The
// overloads are members so that each layer sees all of them.
struct leaf {
  template <typename T>
  static auto marker(const T &value) -> decltype(value.marker()) {
    return value.marker();
  }
  template <typename T>
  static auto marker(const T &handle) -> decltype(*handle, int()) {
    return handle ? marker(*handle) : -1;
  }
  template <typename T, std::size_t N>
  static int marker(const std::array<T, N> &array) {
    return marker(array[N - 1]);
  }
  template <typename... T> static int marker(const std::variant<T...> &held) {
    return std::visit([](const auto &value) { return marker(value); }, held);
  }
};

template <typename T> int marker(const T &value) { return leaf::marker(value); }

// Builds a composition that holds a leaf in every layer. A raw pointer nested
// in a composition points at a static leaf.
template <typename T> struct builder {
  static T make() { return T{}; }
};
template <typename T> T build() { return builder<std::remove_cv_t<T>>::make(); }

// Calls make with the content of a T, or with nothing when a T cannot be
// moved, so that a handle constructs its pointee in place.
template <typename T, typename Make> auto fill(Make make) {
  if constexpr (std::is_move_constructible_v<T>) {
    return make(build<T>());
  } else {
    return make();
  }
}

template <typename T> struct builder<T *> {
  static T *make() {
    static std::remove_const_t<T> value = build<T>();
    return &value;
  }
};
template <typename T> struct builder<std::shared_ptr<T>> {
  static auto make() {
    return fill<T>(
        [](auto &&...v) { return std::make_shared<T>(std::move(v)...); });
  }
};
template <typename T> struct builder<std::unique_ptr<T>> {
  static auto make() {
    return fill<T>(
        [](auto &&...v) { return std::make_unique<T>(std::move(v)...); });
  }
};
template <typename T> struct builder<std::unique_ptr<T[]>> {
  static auto make() { return std::make_unique<T[]>(2); }
};
template <typename T> struct builder<std::optional<T>> {
  static auto make() { return std::optional<T>(std::in_place, build<T>()); }
};
template <typename T> struct builder<test_optional<T>> {
  static auto make() { return test_optional<T>(build<T>()); }
};
template <typename T> struct builder<std::array<T, 2>> {
  static std::array<T, 2> make() { return {build<T>(), build<T>()}; }
};
template <typename T, typename... U> struct builder<std::variant<T, U...>> {
  static auto make() {
    return std::variant<T, U...>(std::in_place_index<0>, build<T>());
  }
};

// The factory of a registration, which counts its calls. A raw pointer it
// returns owns its pointee.
template <typename X> std::size_t made = 0;
template <typename X> X make() {
  ++made<X>;
  if constexpr (std::is_pointer_v<X>) {
    using pointee = std::remove_const_t<std::remove_pointer_t<X>>;
    return fill<pointee>(
        [](auto &&...v) { return new pointee(std::move(v)...); });
  } else {
    return build<X>();
  }
}

using optional_pointer = std::optional<regular> *;

// The nested_wrappers shapes.
using shared_unique = std::shared_ptr<std::unique_ptr<regular>>;
using shared_unique_array = std::shared_ptr<std::unique_ptr<regular[]>>;
using unique_shared = std::unique_ptr<std::shared_ptr<regular>>;
using unique_variant = std::unique_ptr<std::variant<regular, other>>;
using shared_variant = std::shared_ptr<std::variant<regular, other>>;
using variant_unique = std::variant<std::unique_ptr<regular>, other>;
using variant_shared = std::variant<std::shared_ptr<regular>, other>;
using variant_shared_unique = std::variant<shared_unique, other>;
using variant_std_array = std::variant<std::array<regular, 2>, other>;
using shared_const = std::shared_ptr<const regular>;
using shared_optional = std::shared_ptr<std::optional<regular>>;
using shared_custom_optional = std::shared_ptr<test_optional<regular>>;
using shared_unique_move_only = std::shared_ptr<std::unique_ptr<move_only>>;
using shared_optional_move_only = std::shared_ptr<std::optional<move_only>>;

// A container holding one registration of Parts, made by Kind.
template <typename Kind, typename... Parts> struct with_binding {
  typename Kind::template container<dingo::bind<Parts...>> container;
};

// A registration made by make<X>, optionally bound as X itself.
template <typename Mode, typename Scope, typename X, bool Bound = false>
using made_by_factory = std::conditional_t<
    Bound,
    with_binding<Mode, scope<Scope>, storage<X>, interfaces<X>,
                 factory<function<&make<X>>>>,
    with_binding<Mode, scope<Scope>, storage<X>, factory<function<&make<X>>>>>;

// Every cv-qualified reference to a stable composition, resolved or injected,
// is the stored object itself.
template <typename X, typename Container>
void expect_stable(Container &container, X &stored) {
  ASSERT_EQ(marker(stored), 3);
  auto is_stored = [&](const volatile X &object) {
    return std::addressof(object) == std::addressof(stored);
  };
  EXPECT_TRUE(is_stored(container.template resolve<X &>()));
  EXPECT_TRUE(is_stored(container.template resolve<const X &>()));
  EXPECT_TRUE(is_stored(container.template resolve<volatile X &>()));
  EXPECT_TRUE(is_stored(container.template resolve<const volatile X &>()));
  EXPECT_TRUE(container.invoke([&](X &d) { return is_stored(d); }));
  EXPECT_TRUE(container.invoke([&](const X &d) { return is_stored(d); }));
  EXPECT_TRUE(container.invoke([&](volatile X &d) { return is_stored(d); }));
}

template <typename Mode, typename X, bool Bound = true>
void expect_shared_composition() {
  made_by_factory<Mode, shared, X, Bound> fixture;
  made<X> = 0;
  auto &container = fixture.container;
  expect_stable(container, container.template resolve<X &>());
  EXPECT_EQ(made<X>, 1u);
}

template <typename Mode, typename X, bool Bound = true>
void expect_unique_composition() {
  made_by_factory<Mode, unique, X, Bound> fixture;
  made<X> = 0;
  auto &container = fixture.container;
  X value = container.template resolve<X>();
  EXPECT_EQ(marker(value), 3);
  X moved = container.template resolve<X &&>();
  EXPECT_EQ(marker(moved), 3);
  EXPECT_EQ(container.invoke([](X d) { return marker(d); }), 3);
  EXPECT_EQ(container.invoke([](X &&d) { return marker(d); }), 3);
  EXPECT_EQ(made<X>, 4u);
}

template <typename Mode, typename X> void expect_pointer_composition() {
  made_by_factory<Mode, shared, X, true> fixture;
  auto &container = fixture.container;
  X pointer = container.template resolve<X>();
  ASSERT_NE(pointer, nullptr);
  EXPECT_EQ(marker(pointer), 3);
  EXPECT_EQ(container.template resolve<X>(), pointer);
  EXPECT_EQ(container.invoke([](X d) { return d; }), pointer);
}

template <typename Mode> struct wrappers_test : ::testing::Test {};
TYPED_TEST_SUITE(wrappers_test, container_kinds, );

} // namespace

// dependency_composition: a stored composition bound as itself.

TYPED_TEST(wrappers_test, shared_composition_resolves_as_its_stable_object) {
  expect_shared_composition<TypeParam,
                            std::optional<std::shared_ptr<regular>>>();
  expect_shared_composition<
      TypeParam,
      std::variant<std::shared_ptr<regular>, std::unique_ptr<regular>>>();
  expect_shared_composition<TypeParam,
                            std::array<std::optional<move_only>, 2>>();
  expect_shared_composition<TypeParam,
                            std::variant<std::array<move_only, 2>,
                                         std::variant<regular, move_only>>>();
  expect_shared_composition<TypeParam, std::unique_ptr<const copy_only>>();
  expect_shared_composition<TypeParam,
                            std::shared_ptr<std::array<immovable, 2>>>();
  // nested_wrappers registers the same shapes without binding them as X.
  expect_shared_composition<TypeParam, variant_std_array, false>();
}

TYPED_TEST(wrappers_test, unique_composition_resolves_as_a_new_value) {
  expect_unique_composition<TypeParam,
                            std::optional<std::unique_ptr<regular>>>();
  expect_unique_composition<TypeParam,
                            std::unique_ptr<std::array<copy_only, 2>>>();
  expect_unique_composition<TypeParam, std::shared_ptr<const immovable>>();
  expect_unique_composition<TypeParam, std::shared_ptr<const regular *>>();
  expect_unique_composition<TypeParam, std::array<const regular *, 2>>();
  expect_unique_composition<TypeParam, unique_shared, false>();
  expect_unique_composition<TypeParam, unique_variant, false>();
  expect_unique_composition<TypeParam, variant_unique, false>();
}

TYPED_TEST(wrappers_test, pointer_composition_resolves_as_its_pointer) {
  expect_pointer_composition<TypeParam, optional_pointer>();
  expect_pointer_composition<TypeParam, const std::array<immovable, 2> *>();
}

TEST(wrappers_runtime_test,
     external_composition_resolves_as_the_referenced_object) {
  using composition = std::variant<std::array<copy_only, 2>, immovable>;
  composition stored;
  runtime_container<> container;
  container.register_type<scope<external>, storage<composition &>,
                          interfaces<composition>>(stored);
  expect_stable(container, stored);

  std::optional<regular> optional(std::in_place);
  optional_pointer pointer = &optional;
  runtime_container<> pointer_container;
  pointer_container.register_type<scope<external>, storage<optional_pointer &>,
                                  interfaces<optional_pointer>>(pointer);
  EXPECT_EQ(pointer_container.resolve<optional_pointer>(), &optional);
}

// nested_wrappers: every layer of a nested wrapper is published.

TYPED_TEST(wrappers_test, shared_handle_of_unique_handle_publishes_each_layer) {
  made_by_factory<TypeParam, shared, shared_unique> fixture;
  auto &container = fixture.container;
  auto &handle = container.template resolve<shared_unique &>();
  ASSERT_EQ(marker(handle), 3);
  EXPECT_EQ(container.template resolve<shared_unique>(), handle);
  EXPECT_EQ(&container.template resolve<std::unique_ptr<regular> &>(),
            handle.get());
  auto &leaf = container.template resolve<regular &>();
  EXPECT_EQ(&leaf, handle->get());
  EXPECT_EQ(container.template resolve<regular *>(), &leaf);
  EXPECT_EQ(&container.template resolve<const regular &>(), &leaf);
  regular copy = container.template resolve<regular>();
  EXPECT_NE(&copy, &leaf);

  made_by_factory<TypeParam, shared, shared_unique_array> array_fixture;
  auto &array_container = array_fixture.container;
  auto &array = array_container.template resolve<shared_unique_array &>();
  ASSERT_TRUE(array && *array);
  EXPECT_EQ(array_container.template resolve<shared_unique_array>(), array);
  EXPECT_EQ(&array_container.template resolve<std::unique_ptr<regular[]> &>(),
            array.get());
  EXPECT_EQ(array_container.template resolve<regular *>(), array->get());
}

TYPED_TEST(wrappers_test, variant_and_handle_layers_publish_each_other) {
  made_by_factory<TypeParam, shared, shared_variant> shared_variant_fixture;
  auto &container = shared_variant_fixture.container;
  auto &handle = container.template resolve<shared_variant &>();
  ASSERT_TRUE((handle && std::holds_alternative<regular>(*handle)));
  EXPECT_EQ(&container.template resolve<regular &>(),
            &std::get<regular>(*handle));

  made_by_factory<TypeParam, shared, variant_shared> variant_shared_fixture;
  auto &shared_container = variant_shared_fixture.container;
  auto &variant = shared_container.template resolve<variant_shared &>();
  ASSERT_TRUE(std::holds_alternative<std::shared_ptr<regular>>(variant));
  EXPECT_EQ(shared_container.template resolve<std::shared_ptr<regular>>(),
            std::get<std::shared_ptr<regular>>(variant));

  made_by_factory<TypeParam, shared, variant_shared_unique> nested_fixture;
  auto &nested_container = nested_fixture.container;
  auto &nested = nested_container.template resolve<variant_shared_unique &>();
  ASSERT_TRUE(std::holds_alternative<shared_unique>(nested));
  auto &nested_handle = std::get<shared_unique>(nested);
  EXPECT_EQ(nested_container.template resolve<shared_unique>(), nested_handle);
  EXPECT_EQ(&nested_container.template resolve<std::unique_ptr<regular> &>(),
            nested_handle.get());
  EXPECT_EQ(&nested_container.template resolve<regular &>(),
            nested_handle->get());
}

TYPED_TEST(wrappers_test, shared_handle_of_value_wrapper_copies_its_leaf) {
  made_by_factory<TypeParam, shared, shared_const> shared_const_fixture;
  EXPECT_EQ(marker(shared_const_fixture.container.template resolve<regular>()),
            3);
  made_by_factory<TypeParam, shared, shared_optional> optional_fixture;
  EXPECT_EQ(marker(optional_fixture.container.template resolve<regular>()), 3);
  made_by_factory<TypeParam, shared, shared_custom_optional> custom_fixture;
  EXPECT_EQ(marker(custom_fixture.container.template resolve<regular>()), 3);
}

TEST(wrappers_runtime_test, nested_handle_of_move_only_leaf_refuses_a_copy) {
  made_by_factory<runtime_kind<>, shared, shared_unique_move_only> unique;
  auto &leaf = unique.container.resolve<move_only &>();
  EXPECT_EQ(unique.container.resolve<move_only *>(), &leaf);
  EXPECT_THROW((void)unique.container.resolve<move_only>(),
               type_not_convertible_exception);

  made_by_factory<runtime_kind<>, shared, shared_optional_move_only> optional;
  EXPECT_THROW((void)optional.container.resolve<move_only>(),
               type_not_convertible_exception);
}

TEST(wrappers_runtime_test, external_nested_handle_publishes_its_leaf) {
  auto stored = build<shared_unique>();
  runtime_container<> container;
  container.register_type<scope<external>, storage<shared_unique>>(stored);
  auto &leaf = container.resolve<regular &>();
  EXPECT_EQ(&leaf, stored->get());
  EXPECT_EQ(container.resolve<regular *>(), &leaf);
  EXPECT_EQ(marker(container.resolve<regular>()), 3);

  shared_const const_stored = build<shared_const>();
  runtime_container<> const_container;
  const_container.register_type<scope<external>, storage<shared_const>>(
      const_stored);
  EXPECT_EQ(marker(const_container.resolve<regular>()), 3);
}

// custom_wrappers: user-defined wrappers described by type_traits.

namespace {
// A stored custom shared handle is published by reference, by a copy sharing
// its instance and through a pointer to its leaf.
template <typename Leaf, typename Container>
Leaf *expect_custom_shared(Container &container) {
  auto &handle = container.template resolve<test_shared<Leaf> &>();
  EXPECT_NE(handle.get(), nullptr);
  EXPECT_EQ(container.template resolve<Leaf *>(), handle.get());
  EXPECT_EQ(container.template resolve<test_shared<Leaf>>().get(),
            handle.get());
  return handle.get();
}

// A stored custom optional is published by reference and so is its value.
template <typename Container>
test_optional<regular> &expect_custom_optional(Container &container) {
  auto &optional = container.template resolve<test_optional<regular> &>();
  EXPECT_NE(optional.get(), nullptr);
  EXPECT_EQ(&container.template resolve<regular &>(), optional.get());
  return optional;
}
} // namespace

TYPED_TEST(wrappers_test, custom_shared_handle_shares_its_instance) {
  with_binding<TypeParam, scope<shared>, storage<test_shared<regular>>> fixture;
  expect_custom_shared<regular>(fixture.container);

  with_binding<TypeParam, scope<shared>, storage<test_shared<service_impl>>,
               interfaces<service>>
      interface_fixture;
  EXPECT_EQ(
      expect_custom_shared<service>(interface_fixture.container)->marker(), 3);
}

TYPED_TEST(wrappers_test, custom_unique_handle_converts_to_its_shared_handle) {
  with_binding<TypeParam, scope<unique>, storage<test_unique<service_impl>>,
               interfaces<service>>
      fixture;
  auto &container = fixture.container;
  auto unique_handle = container.template resolve<test_unique<service>>();
  ASSERT_NE(unique_handle.get(), nullptr);
  EXPECT_EQ(unique_handle.get()->marker(), 3);
  auto shared_handle = container.template resolve<test_shared<service>>();
  ASSERT_NE(shared_handle.get(), nullptr);
  EXPECT_EQ(shared_handle.get()->marker(), 3);
}

TYPED_TEST(wrappers_test, custom_optional_publishes_its_value) {
  using optional_type = test_optional<regular>;
  with_binding<TypeParam, scope<shared>, storage<optional_type>> fixture;
  expect_custom_optional(fixture.container);

  with_binding<TypeParam, scope<unique>, storage<optional_type>> unique_fixture;
  auto value = unique_fixture.container.template resolve<optional_type>();
  ASSERT_NE(value.get(), nullptr);
  EXPECT_EQ(value.get()->marker(), 3);
}

TEST(wrappers_runtime_test, external_custom_wrappers_resolve_as_stored) {
  test_shared<regular> handle(std::make_shared<regular>());
  runtime_container<> moved;
  moved.register_type<scope<external>, storage<test_shared<regular>>>(
      test_shared<regular>(handle));
  EXPECT_EQ(expect_custom_shared<regular>(moved), handle.get());

  runtime_container<> referenced;
  referenced.register_type<scope<external>, storage<test_shared<regular> &>>(
      handle);
  EXPECT_EQ(expect_custom_shared<regular>(referenced), handle.get());
  EXPECT_EQ(&referenced.resolve<test_shared<regular> &>(), &handle);

  test_optional<regular> optional{regular{}};
  runtime_container<> optional_moved;
  optional_moved
      .register_type<scope<external>, storage<test_optional<regular>>>(
          test_optional<regular>(optional));
  expect_custom_optional(optional_moved);

  runtime_container<> optional_referenced;
  optional_referenced
      .register_type<scope<external>, storage<test_optional<regular> &>>(
          optional);
  EXPECT_EQ(&expect_custom_optional(optional_referenced), &optional);
}

namespace {
// Counts the live objects of a leaf a unique scope makes as a raw pointer.
struct counted_pointee {
  counted_pointee() { ++live; }
  counted_pointee(const counted_pointee &) { ++live; }
  counted_pointee(counted_pointee &&) noexcept { ++live; }
  ~counted_pointee() { --live; }
  static int live;
};
int counted_pointee::live = 0;

counted_pointee *make_counted_pointee() { return new counted_pointee; }

// A value of what a pointer the scope makes points to is moved out of it,
// and the pointer is deleted: no object outlives what was resolved.
template <typename Container> void expect_values_of_made_pointee() {
  {
    Container container;
    if constexpr (std::is_same_v<Container, runtime_container<>>) {
      container
          .template register_type<scope<unique>, storage<counted_pointee *>,
                                  factory<function<make_counted_pointee>>>();
    }
    {
      auto value = container.template resolve<counted_pointee>();
      auto moved = container.template resolve<counted_pointee &&>();
      auto optional =
          container.template resolve<std::optional<counted_pointee>>();
      auto shared = container.template resolve<
          std::shared_ptr<std::optional<counted_pointee>>>();
      EXPECT_TRUE(optional.has_value());
      EXPECT_TRUE(shared->has_value());
      EXPECT_EQ(counted_pointee::live, 4);
      (void)value, (void)moved;
    }
    EXPECT_EQ(counted_pointee::live, 0);
  }
  EXPECT_EQ(counted_pointee::live, 0);
}
} // namespace

TEST(wrappers_test, runtime_values_of_made_pointee_are_moved_out_of_it) {
  expect_values_of_made_pointee<runtime_container<>>();
}

TEST(wrappers_test, static_values_of_made_pointee_are_moved_out_of_it) {
  expect_values_of_made_pointee<static_container<
      bindings<dingo::bind<scope<unique>, storage<counted_pointee *>,
                           factory<function<make_counted_pointee>>>>>>();
}

namespace {
using nested_optional = std::optional<std::optional<regular>>;

nested_optional empty_nested_optional() { return std::nullopt; }
} // namespace

// The optional an optional of an optional holds is given as it is: an empty
// one gives an empty optional, whether the scope makes it or is given it.
TEST(wrappers_test, empty_optional_of_optional_gives_empty_optional) {
  container<> runtime;
  runtime.register_type<scope<unique>, storage<nested_optional>,
                        factory<function<&empty_nested_optional>>>();
  EXPECT_FALSE(runtime.resolve<std::optional<regular>>().has_value());
  EXPECT_FALSE(runtime.invoke(
      [](std::optional<regular> &&value) { return value.has_value(); }));

  static_container<
      bindings<dingo::bind<scope<unique>, storage<nested_optional>,
                           factory<function<&empty_nested_optional>>>>>
      compile_time;
  EXPECT_FALSE(compile_time.resolve<std::optional<regular>>().has_value());
  EXPECT_FALSE(compile_time.invoke(
      [](std::optional<regular> &&value) { return value.has_value(); }));

  nested_optional given;
  container<> external_runtime;
  external_runtime.register_type<scope<external>, storage<nested_optional &>>(
      given);
  EXPECT_FALSE(external_runtime.resolve<std::optional<regular>>().has_value());
  given.emplace(std::in_place);
  EXPECT_TRUE(external_runtime.resolve<std::optional<regular>>().has_value());
}
