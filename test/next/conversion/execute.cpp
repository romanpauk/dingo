//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The execution of the routes of a binding: a binding keeps the objects its
// cached rules derive only when one of its routes takes such a rule, however
// deeply the rule is nested in the stored type tree; a declared conversion
// constructs its target from what it returns; and a request the stored object
// cannot serve is refused when it is resolved, not when it is compiled.
// spec: routes

#include <dingo/container.h>
#include <dingo/conversion/convert.h>
#include <dingo/conversion/type_conversion.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace {

struct leaf {};
struct other_leaf {};

struct iface {
  virtual ~iface() = default;
};
struct impl : iface {};

template <typename Scope, typename Stored, typename... Interfaces>
using binding =
    dingo::core::binding_type<Scope, Stored,
                              dingo::core::type_list<Interfaces...>>;

template <typename Binding>
inline constexpr bool keeps_objects_v =
    Binding::keeps &&
    std::is_same_v<typename Binding::context_type, dingo::core::kept_objects>;

template <typename Binding>
inline constexpr bool keeps_nothing_v =
    !Binding::keeps && std::is_same_v<typename Binding::context_type,
                                      dingo::core::no_kept_objects>;

TEST(next_execute, a_binding_keeps_objects_only_for_a_retaining_route) {
  // A borrowed sharing handle converted to the handle of an interface is
  // kept, at whatever depth of the stored type tree the handle is.
  static_assert(
      keeps_objects_v<binding<dingo::shared, std::shared_ptr<impl>, iface>>);
  static_assert(
      keeps_objects_v<
          binding<dingo::shared, std::optional<std::shared_ptr<impl>>, iface>>);
  static_assert(
      keeps_objects_v<
          binding<dingo::shared,
                  std::variant<std::shared_ptr<impl>, other_leaf>, iface>>);
  static_assert(
      keeps_objects_v<binding<dingo::external, std::shared_ptr<impl>, iface>>);

  // A leaf, a handle of the leaf itself, and a consumed handle keep nothing.
  static_assert(keeps_nothing_v<binding<dingo::shared, leaf>>);
  static_assert(keeps_nothing_v<binding<dingo::shared, impl, iface>>);
  static_assert(keeps_nothing_v<binding<dingo::shared, std::shared_ptr<leaf>>>);
  static_assert(keeps_nothing_v<
                binding<dingo::shared, std::optional<std::shared_ptr<leaf>>>>);
  static_assert(
      keeps_nothing_v<binding<dingo::unique, std::shared_ptr<impl>, iface>>);
  static_assert(std::is_empty_v<dingo::core::no_kept_objects>);
}

struct conversion_source {};
struct conversion_result {};

struct conversion_target {
  explicit conversion_target(conversion_result) : value(53) {}

  int value;
};

} // namespace

template <>
struct dingo::type_conversion_traits<conversion_target, conversion_source> {
  template <typename> using required_access = dingo::borrow;

  static conversion_result convert(const conversion_source &) { return {}; }
};

namespace {

TEST(next_execute, a_declared_conversion_constructs_its_target_explicitly) {
  using dingo::core::converts;
  using dingo::core::declares_conversion;
  static_assert(
      declares_conversion<conversion_target, conversion_source>::value);
  static_assert(converts<conversion_target, conversion_source,
                         const conversion_source &>());
  static_assert(
      converts<conversion_target, conversion_source, conversion_source &&>());
  static_assert(
      !declares_conversion<conversion_source, conversion_target>::value);

  const conversion_source source;
  const conversion_target converted =
      dingo::core::convert_to<conversion_target>(source);
  EXPECT_EQ(converted.value, 53);
}

struct immovable {
  immovable() = default;
  immovable(const immovable &) = delete;
  immovable(immovable &&) = delete;
  immovable &operator=(const immovable &) = delete;
  immovable &operator=(immovable &&) = delete;
};

TEST(next_execute, an_immovable_object_is_referred_to_and_never_copied) {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<immovable>>();

  immovable &object = container.resolve<immovable &>();
  EXPECT_EQ(container.resolve<immovable *>(), &object);
  // The value request compiles, so no copy is instantiated, and is refused.
  EXPECT_THROW(container.resolve<immovable>(),
               dingo::type_not_convertible_exception);
}

struct alternative_a {
  explicit alternative_a(int init) : value(init) {}
  int value;
};

struct alternative_b {
  explicit alternative_b(float init) : value(init) {}
  float value;
};

struct unrelated_alternative {
  int value;
};

TEST(next_execute, an_alternative_refuses_unrelated_targets_when_resolved) {
  std::variant<alternative_a, alternative_b> value(
      std::in_place_type<alternative_a>, 7);
  dingo::container<> container;
  container.register_type<
      dingo::scope<dingo::external>,
      dingo::storage<std::variant<alternative_a, alternative_b> &>>(value);

  EXPECT_EQ(container.resolve<alternative_a &>().value, 7);
  EXPECT_THROW(container.resolve<unrelated_alternative &>(),
               dingo::type_not_found_exception);
  EXPECT_THROW(container.resolve<unrelated_alternative *>(),
               dingo::type_not_found_exception);
}

} // namespace
