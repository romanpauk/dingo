//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/scope_traits.h>

#include <gtest/gtest.h>

#include <utility>

// A scope defined outside the library: a tag and one scope_traits
// specialization, which names the storage the scope keeps its instances in.
namespace {
struct counted {};

struct counted_value {
  counted_value() : value(7) {}

  int value;
};

int constructions = 0;

// Constructs an instance for every request, as unique does, and counts them.
template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class counted_storage : Factory {
public:
  template <typename... Args>
  counted_storage(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  using conversions = Conversions;
  using factory_type = Factory;
  using type = Type;
  using stored_type = StoredType;
  using resolved_type = Type;
  using tag_type = counted;

  template <typename Context, typename Container>
  Type resolve(dingo::construction_scope scope, Context &context,
               Container &container) {
    ++constructions;
    return Factory::template construct<Type>(scope, context, container);
  }
};
} // namespace

template <> struct dingo::scope_traits<counted> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;
  static constexpr bool consumes = true;
  static constexpr materialization_guard guard = materialization_guard::always;
  static constexpr bool retains_unresolved_source = false;

  static constexpr storage_exposure exposed(storage_form form) {
    return form == storage_form::plain ? storage_exposure::rvalue
                                       : storage_exposure::none;
  }

  template <typename Type, typename StoredType, typename Factory,
            typename Conversions>
  using storage = counted_storage<Type, StoredType, Factory, Conversions>;
};

namespace {
template <typename Container> void expect_counted_values(Container &container) {
  constructions = 0;
  EXPECT_EQ(container.template resolve<counted_value>().value, 7);
  EXPECT_EQ(container.template resolve<counted_value>().value, 7);
  EXPECT_EQ(constructions, 2);
}
} // namespace

TEST(custom_scope_test, runtime_container_registers_a_user_scope) {
  dingo::container<> container;
  container
      .register_type<dingo::scope<counted>, dingo::storage<counted_value>>();
  expect_counted_values(container);
}

TEST(custom_scope_test, static_container_binds_a_user_scope) {
  dingo::static_container<dingo::bindings<
      dingo::bind<dingo::scope<counted>, dingo::storage<counted_value>>>>
      container;
  expect_counted_values(container);
}
