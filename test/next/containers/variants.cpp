//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A route through an alternative of a variant is served only when the variant
// holds that alternative: the route is chosen when it is derived, and the
// object it reaches into is checked when it is walked.

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace {

struct service {
  int value = 7;
};

using alternatives = std::variant<service, std::unique_ptr<service>>;

using dingo::scope;
using dingo::storage;

// A variant holding its second alternative.
alternatives holding_handle() {
  return alternatives(std::in_place_index<1>, std::make_unique<service>());
}

TEST(next_variants, serve_the_held_alternative) {
  alternatives stored = holding_handle();
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  EXPECT_EQ(&container.resolve<std::unique_ptr<service> &>(),
            &std::get<1>(stored));
}

TEST(next_variants, serve_through_the_held_alternative) {
  alternatives stored = holding_handle();
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  // The shortest route to the service takes the first alternative; the
  // variant holds the second, whose route reaches the service too.
  EXPECT_EQ(&container.resolve<service &>(), std::get<1>(stored).get());
  stored.emplace<0>();
  EXPECT_EQ(&container.resolve<service &>(), &std::get<0>(stored));
}

TEST(next_variants, refuse_an_alternative_not_held) {
  alternatives stored(std::in_place_index<0>);
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  // Only the second alternative reaches the handle, which the variant does
  // not hold.
  try {
    (void)container.resolve<std::unique_ptr<service> &>();
    FAIL() << "the variant does not hold the handle";
  } catch (const dingo::type_not_convertible_exception &exception) {
    EXPECT_NE(std::string(exception.what())
                  .find("holds another alternative than the one the request "
                        "is routed through"),
              std::string::npos)
        << exception.what();
  }
}

TEST(next_variants, check_the_held_alternative_each_time) {
  alternatives stored = holding_handle();
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<alternatives &>>(
      stored);
  EXPECT_EQ(&container.resolve<std::unique_ptr<service> &>(),
            &std::get<1>(stored));
  stored.emplace<0>();
  EXPECT_THROW((void)container.resolve<std::unique_ptr<service> &>(),
               dingo::type_not_convertible_exception);
  EXPECT_EQ(container.resolve<service &>().value, 7);
}

TEST(next_variants, check_the_held_alternative_in_a_static_container) {
  alternatives stored = holding_handle();
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::external>, storage<alternatives &>>>;
  dingo::static_container<bindings> container(stored);
  EXPECT_EQ(&container.resolve<std::unique_ptr<service> &>(),
            &std::get<1>(stored));
  EXPECT_EQ(&container.resolve<service &>(), std::get<1>(stored).get());
  stored.emplace<0>();
  EXPECT_EQ(&container.resolve<service &>(), &std::get<0>(stored));
  EXPECT_THROW((void)container.resolve<std::unique_ptr<service> &>(),
               dingo::type_not_convertible_exception);
}

// Both alternatives reach the service, each by a handle of its own.
TEST(next_variants, serve_by_the_handle_held) {
  using handles =
      std::variant<std::unique_ptr<service>, std::shared_ptr<service>>;
  handles stored(std::in_place_index<1>, std::make_shared<service>());
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<handles &>>(stored);
  EXPECT_EQ(&container.resolve<service &>(), std::get<1>(stored).get());
  stored.emplace<0>(std::make_unique<service>());
  EXPECT_EQ(&container.resolve<service &>(), std::get<0>(stored).get());
}

struct copy_only {
  copy_only() = default;
  copy_only(const copy_only &) = default;
  copy_only(copy_only &&) = delete;
};

struct immovable {
  immovable() = default;
  immovable(const immovable &) = delete;
};

// Alternatives that are wrappers of different kinds: an optional and a variant,
// which both reach a copy_only, each from a wrapper of its own type.
using layered =
    std::variant<std::optional<copy_only>, std::variant<copy_only, immovable>>;

// The message of the type_not_convertible_exception `request()` throws.
template <typename Request> std::string refusal(Request &&request) {
  try {
    request();
  } catch (const dingo::type_not_convertible_exception &exception) {
    return exception.what();
  }
  return "not refused";
}

constexpr const char *alternative_not_held =
    "holds another alternative than the one the request is routed through";

// `container` serves a copy_only through the alternative `stored` holds: the
// element of the optional, the copy_only alternative of the inner variant,
// or neither.
template <typename Container>
void serve_through_layered_alternatives(Container &container, layered &stored) {
  stored.emplace<0>(std::in_place);
  copy_only *element = &*std::get<0>(stored);
  EXPECT_EQ(&container.template resolve<copy_only &>(), element);
  EXPECT_EQ(container.template resolve<copy_only *>(), element);
  EXPECT_EQ(container.template resolve<const copy_only *>(), element);
  EXPECT_EQ(&container.template resolve<std::optional<copy_only> &>(),
            &std::get<0>(stored));
  copy_only copied = container.template resolve<copy_only>();
  (void)copied;

  // The address of the element of an empty optional is null.
  stored.emplace<0>();
  EXPECT_EQ(container.template resolve<copy_only *>(), nullptr);
  EXPECT_NE(refusal([&] {
              (void)container.template resolve<copy_only &>();
            }).find("holds no element"),
            std::string::npos);

  // The routes to a borrowed copy_only continue along the inner variant;
  // the copy of the element of the optional has no route through it.
  stored.emplace<1>(std::in_place_index<0>);
  element = &std::get<0>(std::get<1>(stored));
  EXPECT_EQ(&container.template resolve<copy_only &>(), element);
  EXPECT_EQ(container.template resolve<copy_only *>(), element);
  EXPECT_EQ(container.template resolve<const copy_only *>(), element);
  EXPECT_NE(refusal([&] {
              (void)container.template resolve<copy_only>();
            }).find(alternative_not_held),
            std::string::npos);
  EXPECT_NE(refusal([&] {
              (void)container.template resolve<std::optional<copy_only> &>();
            }).find(alternative_not_held),
            std::string::npos);

  // The inner variant holds no copy_only, and the address is not null.
  stored.emplace<1>(std::in_place_index<1>);
  EXPECT_NE(refusal([&] {
              (void)container.template resolve<copy_only &>();
            }).find(alternative_not_held),
            std::string::npos);
  EXPECT_NE(refusal([&] {
              (void)container.template resolve<copy_only *>();
            }).find(alternative_not_held),
            std::string::npos);
}

TEST(next_variants, serve_through_alternatives_of_different_kinds) {
  layered stored;
  dingo::container<> container;
  container.register_type<scope<dingo::external>, storage<layered &>,
                          dingo::interfaces<layered>>(stored);
  serve_through_layered_alternatives(container, stored);
}

TEST(next_variants,
     serve_through_alternatives_of_different_kinds_in_a_static_container) {
  layered stored;
  using bindings =
      dingo::bindings<dingo::bind<scope<dingo::external>, storage<layered &>>>;
  dingo::static_container<bindings> container(stored);
  serve_through_layered_alternatives(container, stored);
}

struct first_alternative {
  first_alternative() : value(1) {}
  int value;
};

struct second_alternative {
  second_alternative() : value(2) {}
  float value;
};

using named_alternatives = std::variant<first_alternative, second_alternative>;

// A binding of a variant names each of its alternatives: a request of one the
// object does not hold finds the binding and is not convertible.
template <typename Unique, typename External>
void name_every_alternative(Unique &unique, External &external,
                            named_alternatives &stored) {
  EXPECT_EQ(unique.template resolve<first_alternative>().value, 1);
  EXPECT_NE(refusal([&] {
              (void)unique.template resolve<second_alternative>();
            }).find(alternative_not_held),
            std::string::npos);

  stored.emplace<0>();
  EXPECT_EQ(&external.template resolve<first_alternative &>(),
            &std::get<0>(stored));
  EXPECT_NE(refusal([&] {
              (void)external.template resolve<second_alternative &>();
            }).find(alternative_not_held),
            std::string::npos);
  stored.emplace<1>();
  EXPECT_EQ(&external.template resolve<second_alternative &>(),
            &std::get<1>(stored));
  EXPECT_THROW((void)external.template resolve<first_alternative &>(),
               dingo::type_not_convertible_exception);
}

TEST(next_variants, a_binding_names_every_alternative) {
  dingo::container<> unique;
  unique.register_type<scope<dingo::unique>, storage<named_alternatives>,
                       dingo::factory<dingo::constructor<first_alternative>>>();
  named_alternatives stored;
  dingo::container<> external;
  external.register_type<scope<dingo::external>, storage<named_alternatives &>>(
      stored);
  name_every_alternative(unique, external, stored);
}

TEST(next_variants, a_static_binding_names_every_alternative) {
  dingo::static_container<dingo::bindings<
      dingo::bind<scope<dingo::unique>, storage<named_alternatives>,
                  dingo::factory<dingo::constructor<first_alternative>>>>>
      unique;
  named_alternatives stored;
  dingo::static_container<dingo::bindings<
      dingo::bind<scope<dingo::external>, storage<named_alternatives &>>>>
      external(stored);
  name_every_alternative(unique, external, stored);
}

} // namespace
