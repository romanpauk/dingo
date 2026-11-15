//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "type_registration_common.h"

#include <dingo/static/binding.h>

#include "support/custom_wrappers.h"
#include "support/resolution_test_types.h"

#include <array>
#include <memory>
#include <optional>
#include <variant>

template <typename Request, typename Binding>
inline constexpr bool plans_v =
    detail::static_execution<Request, Binding>::resolves;

// A volatile reference is reached only through volatile references, and
// copied as a value.
TEST(type_registration_test, volatile_binding_plans_volatile_access) {
  using volatile_model = detail::binding_model<
      type_registration<scope<external>, storage<volatile int &>>>;
  using volatile_binding = detail::binding<int, volatile_model>;
  static_assert(plans_v<int, volatile_binding>);
  static_assert(plans_v<volatile int &, volatile_binding>);
  static_assert(plans_v<const volatile int &, volatile_binding>);
  static_assert(!plans_v<int &, volatile_binding>);
  static_assert(!plans_v<const int &, volatile_binding>);
}

TEST(type_registration_test, consumed_binding_executes_custom_type_conversion) {
  struct service_interface {
    virtual ~service_interface() = default;
    virtual int value() const = 0;
  };
  struct implementation : service_interface {
    int value() const override { return 7; }
  };

  container<> container;
  container.register_type<scope<unique>, storage<test_shared<implementation>>,
                          interfaces<service_interface>>();

  auto resolved = container.resolve<test_shared<service_interface>>();
  ASSERT_NE(resolved.get(), nullptr);
  EXPECT_EQ(resolved.get()->value(), 7);
}

TEST(type_registration_test,
     binding_model_rewrites_single_interface_storage_leaf) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};

  using registration =
      type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                        interfaces<I>>;
  using model = detail::binding_model<registration>;

  static_assert(model::storage_tag_is_complete);
  static_assert(model::stores_interface_leaf);
  static_assert(std::is_same_v<typename model::stored_leaf_type, I>);
  static_assert(
      std::is_same_v<typename model::stored_type, std::shared_ptr<I>>);
  static_assert(std::is_same_v<typename model::storage_type::stored_type,
                               std::shared_ptr<I>>);
  static_assert(model::valid);
}

TEST(type_registration_test,
     binding_model_preserves_storage_for_multi_interface_registration) {
  struct I {
    virtual ~I() = default;
  };
  struct J {
    virtual ~J() = default;
  };
  struct A : I, J {};

  using registration =
      type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                        interfaces<I, J>>;
  using model = detail::binding_model<registration>;
  using expansion = detail::binding_expansion<model>;

  static_assert(model::storage_tag_is_complete);
  static_assert(!model::stores_interface_leaf);
  static_assert(std::is_same_v<typename model::stored_leaf_type, A>);
  static_assert(
      std::is_same_v<typename model::stored_type, std::shared_ptr<A>>);
  static_assert(
      std::is_same_v<
          typename expansion::interface_bindings,
          type_list<detail::binding<I, model>, detail::binding<J, model>>>);
  static_assert(model::valid);
}

TEST(type_registration_test, recursive_leaf_and_rebind_traits) {
  struct I {
    virtual ~I() = default;
  };
  struct J {
    virtual ~J() = default;
  };
  struct A : I, J {};

  using nested_handle = const std::shared_ptr<std::unique_ptr<A>> &;
  using nested_list = type_list<nested_handle, std::unique_ptr<A *>>;

  static_assert(std::is_same_v<leaf_type_t<nested_handle>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_handle, I>,
                               std::shared_ptr<std::unique_ptr<I>> &>);
  static_assert(std::is_same_v<leaf_type_t<nested_list>, type_list<A, A>>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_list, I>,
                               type_list<std::shared_ptr<std::unique_ptr<I>> &,
                                         std::unique_ptr<I *>>>);
  static_assert(std::is_same_v<leaf_type_t<std::optional<const A *>>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<const A *, I>, const I *>);
  static_assert(std::is_same_v<rebind_leaf_t<std::optional<const A *>, I>,
                               std::optional<const I *>>);
  static_assert(std::is_same_v<rebind_leaf_t<std::shared_ptr<const A>, I>,
                               std::shared_ptr<const I>>);
  static_assert(std::is_same_v<rebind_leaf_t<std::unique_ptr<const A>, I>,
                               std::unique_ptr<const I>>);
  static_assert(std::is_same_v<rebind_leaf_t<std::shared_ptr<volatile A>, I>,
                               std::shared_ptr<volatile I>>);
  static_assert(
      std::is_same_v<rebind_leaf_t<std::unique_ptr<const volatile A>, I>,
                     std::unique_ptr<const volatile I>>);

  EXPECT_TRUE(detail::qualifies_as(describe_type<A **>(),
                                   describe_type<const A *const *>()));
  EXPECT_FALSE(
      detail::qualifies_as(describe_type<A **>(), describe_type<const A **>()));
  constexpr type_descriptor optional_pointer_target =
      detail::described_type_v<std::optional<A> *>;
  static_assert(detail::qualifies_as(
      optional_pointer_target, detail::described_type_v<std::optional<A> *>));
  static_assert(
      detail::qualifies_as(optional_pointer_target,
                           detail::described_type_v<const std::optional<A> *>));
  static_assert(!detail::qualifies_as(
      optional_pointer_target, detail::described_type_v<std::optional<I> *>));
}

TEST(type_registration_test,
     nested_raw_pointer_storage_resolves_exact_request_shapes) {
  struct service {
    int value;
  };

  std::optional<service> external_value{service{7}};
  container<> external_container;
  external_container
      .register_type<scope<external>, storage<std::optional<service> *>>(
          std::addressof(external_value));

  auto *external_pointer =
      external_container.resolve<std::optional<service> *>();
  EXPECT_EQ(external_pointer, std::addressof(external_value));
  ASSERT_TRUE(external_pointer->has_value());
  EXPECT_EQ(external_pointer->value().value, 7);
  auto &external_reference =
      external_container.resolve<std::optional<service> &>();
  EXPECT_EQ(std::addressof(external_reference), std::addressof(external_value));
  auto external_copy = external_container.resolve<std::optional<service>>();
  ASSERT_TRUE(external_copy.has_value());
  EXPECT_EQ(external_copy->value, 7);

  const std::array<int, 2> external_array{3, 5};
  container<> external_array_container;
  external_array_container
      .register_type<scope<external>, storage<const std::array<int, 2> *>>(
          std::addressof(external_array));
  EXPECT_EQ((external_array_container.resolve<std::array<int, 2>>()),
            external_array);

  auto external_shared_value = std::make_shared<service>(service{9});
  container<> external_shared_container;
  external_shared_container
      .register_type<scope<external>, storage<std::shared_ptr<service> *>>(
          std::addressof(external_shared_value));
  EXPECT_EQ(external_shared_container.resolve<std::shared_ptr<service>>(),
            external_shared_value);

  struct move_only_service {
    explicit move_only_service(int init_value) : value(init_value) {}
    move_only_service(const move_only_service &) = delete;
    move_only_service(move_only_service &&) = default;

    int value;
  };

  move_only_service external_move_only_value{10};
  container<> external_move_only_container;
  external_move_only_container
      .register_type<scope<external>, storage<move_only_service *>>(
          std::addressof(external_move_only_value));
  EXPECT_THROW((void)external_move_only_container.resolve<move_only_service>(),
               type_not_convertible_exception);
  EXPECT_EQ(external_move_only_value.value, 10);

  std::optional<move_only_service> external_nested_move_only_value{
      std::in_place, 12};
  container<> external_nested_move_only_container;
  external_nested_move_only_container.register_type<
      scope<external>, storage<std::optional<move_only_service> *>>(
      std::addressof(external_nested_move_only_value));
  EXPECT_THROW((void)external_nested_move_only_container
                   .resolve<std::optional<move_only_service>>(),
               type_not_convertible_exception);
  ASSERT_TRUE(external_nested_move_only_value.has_value());
  EXPECT_EQ(external_nested_move_only_value->value, 12);

  using external_variant = std::variant<int, float>;
  const external_variant *null_external_variant = nullptr;
  container<> null_external_variant_container;
  null_external_variant_container
      .register_type<scope<external>, storage<const external_variant *>>(
          null_external_variant);
  EXPECT_EQ(null_external_variant_container.resolve<const external_variant *>(),
            nullptr);

  container<> shared_container;
  shared_container
      .register_type<scope<shared>, storage<const std::optional<service> *>>(
          callable([]() -> const std::optional<service> * {
            return new std::optional<service>{service{11}};
          }));

  auto *shared_pointer =
      shared_container.resolve<const std::optional<service> *>();
  ASSERT_NE(shared_pointer, nullptr);
  ASSERT_TRUE(shared_pointer->has_value());
  EXPECT_EQ(shared_pointer->value().value, 11);
  EXPECT_EQ(shared_container.resolve<const std::optional<service> *>(),
            shared_pointer);
  auto &shared_reference =
      shared_container.resolve<const std::optional<service> &>();
  EXPECT_EQ(std::addressof(shared_reference), shared_pointer);

  container<> unique_container;
  unique_container
      .register_type<scope<unique>, storage<std::optional<service> *>>(
          callable([]() { return new std::optional<service>{service{13}}; }));
  auto unique_value =
      unique_container.resolve<std::unique_ptr<std::optional<service>>>();
  ASSERT_TRUE(unique_value->has_value());
  EXPECT_EQ(unique_value->value().value, 13);
  auto unique_rvalue =
      unique_container.resolve<std::unique_ptr<std::optional<service>> &&>();
  ASSERT_TRUE(unique_rvalue->has_value());
  EXPECT_EQ(unique_rvalue->value().value, 13);
  auto shared_value =
      unique_container.resolve<std::shared_ptr<std::optional<service>>>();
  ASSERT_TRUE(shared_value->has_value());
  EXPECT_EQ(shared_value->value().value, 13);

  struct service_interface {
    int value;
  };
  struct secondary {
    virtual ~secondary() = default;
  };
  struct implementation : service_interface, secondary {
    explicit implementation(int init) { value = init; }
  };

  std::optional<implementation> converted_value{std::in_place, 17};
  container<> converted_container;
  converted_container
      .register_type<scope<external>, storage<std::optional<implementation> *>,
                     interfaces<service_interface, secondary>>(
          std::addressof(converted_value));

  // An optional of the interface would hold a copy of it, which slices the
  // implementation.
  EXPECT_THROW(
      (void)converted_container.resolve<std::optional<service_interface>>(),
      type_not_convertible_exception);

  test_optional<implementation> custom_value{implementation{19}};
  container<> custom_container;
  custom_container
      .register_type<scope<external>, storage<test_optional<implementation> *>,
                     interfaces<service_interface, secondary>>(
          std::addressof(custom_value));

  // So would an optional of a user's.
  EXPECT_THROW(
      (void)custom_container.resolve<test_optional<service_interface>>(),
      type_not_convertible_exception);
}

TEST(type_registration_test, const_reference_storage_preserves_const_access) {
  using model = detail::binding_model<
      type_registration<scope<external>, storage<const int &>>>;
  using binding = detail::binding<int, model>;
  static_assert(plans_v<const int &, binding>);
  static_assert(plans_v<const int *, binding>);
  static_assert(!plans_v<int &, binding>);
  static_assert(!plans_v<int *, binding>);

  const int value = 7;
  container<> value_container;
  value_container.register_type<scope<external>, storage<const int &>>(value);

  EXPECT_EQ(value_container.resolve<int>(), 7);
  EXPECT_EQ(&value_container.resolve<const int &>(), &value);
  EXPECT_EQ(value_container.resolve<const int *>(), &value);

  const std::optional<int> optional_value{11};

  container<> optional_container;
  optional_container
      .register_type<scope<external>, storage<const std::optional<int> &>>(
          optional_value);

  EXPECT_EQ(optional_container.resolve<int>(), 11);
  EXPECT_EQ(&optional_container.resolve<const int &>(), &*optional_value);
  EXPECT_EQ(optional_container.resolve<const int *>(), &*optional_value);
  EXPECT_EQ(&optional_container.resolve<const std::optional<int> &>(),
            &optional_value);

  const std::optional<int> empty_optional;
  container<> empty_optional_container;
  empty_optional_container
      .register_type<scope<external>, storage<const std::optional<int> &>>(
          empty_optional);
  EXPECT_EQ(empty_optional_container.resolve<const int *>(), nullptr);

  const test_optional<int> custom_optional_value{13};
  container<> custom_optional_container;
  custom_optional_container
      .register_type<scope<external>, storage<const test_optional<int> &>>(
          custom_optional_value);

  EXPECT_EQ(custom_optional_container.resolve<int>(), 13);
  EXPECT_EQ(&custom_optional_container.resolve<const int &>(),
            custom_optional_value.get());
  EXPECT_EQ(custom_optional_container.resolve<const int *>(),
            custom_optional_value.get());

  using nested_optional_type = std::optional<std::variant<int, float>>;
  const nested_optional_type nested_optional_value{std::in_place,
                                                   std::in_place_type<int>, 17};
  container<> nested_optional_container;
  nested_optional_container.register_type<
      scope<external>, storage<const nested_optional_type &>, interfaces<int>>(
      nested_optional_value);

  EXPECT_EQ(nested_optional_container.resolve<int>(), 17);
  EXPECT_EQ(&nested_optional_container.resolve<const int &>(),
            &std::get<int>(*nested_optional_value));
  EXPECT_EQ(nested_optional_container.resolve<const int *>(),
            &std::get<int>(*nested_optional_value));

  using doubly_nested_optional_type = std::optional<nested_optional_type>;
  const doubly_nested_optional_type doubly_nested_optional_value{
      std::in_place, std::in_place, std::in_place_type<int>, 19};
  container<> doubly_nested_optional_container;
  doubly_nested_optional_container.register_type<
      scope<external>, storage<const doubly_nested_optional_type &>,
      interfaces<int>>(doubly_nested_optional_value);

  EXPECT_EQ(doubly_nested_optional_container.resolve<int>(), 19);
  EXPECT_EQ(&doubly_nested_optional_container.resolve<const int &>(),
            &std::get<int>(**doubly_nested_optional_value));
  EXPECT_EQ(doubly_nested_optional_container.resolve<const int *>(),
            &std::get<int>(**doubly_nested_optional_value));
}

TEST(type_registration_test,
     const_external_smart_pointer_storage_preserves_const_access) {
  struct service_interface {
    virtual ~service_interface() = default;
    virtual int get_value() const = 0;
  };
  struct service {
    int value;
  };
  struct interface_service : service_interface {
    explicit interface_service(int init_value) : value(init_value) {}

    int get_value() const override { return value; }

    int value;
  };

  const std::shared_ptr<const service> shared_value =
      std::make_shared<const service>(service{23});
  container<> shared_container;
  shared_container.register_type<
      scope<external>, storage<const std::shared_ptr<const service> &>>(
      shared_value);

  auto shared_copy = shared_container.resolve<std::shared_ptr<const service>>();
  EXPECT_EQ(shared_copy, shared_value);
  EXPECT_EQ(&shared_container.resolve<const std::shared_ptr<const service> &>(),
            &shared_value);
  EXPECT_EQ(&shared_container.resolve<const service &>(), shared_value.get());
  EXPECT_EQ(shared_container.resolve<const service *>(), shared_value.get());
  EXPECT_THROW((void)shared_container.resolve<service &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)shared_container.resolve<service *>(),
               type_not_convertible_exception);

  const std::shared_ptr<const interface_service> shared_interface_value =
      std::make_shared<const interface_service>(31);
  container<> shared_interface_container;
  shared_interface_container
      .register_type<scope<external>,
                     storage<const std::shared_ptr<const interface_service> &>,
                     interfaces<service_interface>>(shared_interface_value);

  auto shared_interface_copy =
      shared_interface_container
          .resolve<std::shared_ptr<const service_interface>>();
  EXPECT_EQ(shared_interface_copy.get(), shared_interface_value.get());
  EXPECT_EQ(&shared_interface_container.resolve<const service_interface &>(),
            shared_interface_value.get());
  EXPECT_EQ(shared_interface_container.resolve<const service_interface *>(),
            shared_interface_value.get());
  EXPECT_THROW((void)shared_interface_container.resolve<service_interface &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)shared_interface_container.resolve<service_interface *>(),
               type_not_convertible_exception);

  const std::unique_ptr<const service> unique_value =
      std::make_unique<const service>(service{29});
  container<> unique_container;
  unique_container.register_type<
      scope<external>, storage<const std::unique_ptr<const service> &>>(
      unique_value);

  EXPECT_EQ(&unique_container.resolve<const std::unique_ptr<const service> &>(),
            &unique_value);
  EXPECT_EQ(&unique_container.resolve<const service &>(), unique_value.get());
  EXPECT_EQ(unique_container.resolve<const service *>(), unique_value.get());
  EXPECT_THROW((void)unique_container.resolve<service &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)unique_container.resolve<service *>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)unique_container.resolve<std::unique_ptr<const service>>(),
               type_not_convertible_exception);

  const std::unique_ptr<const interface_service> unique_interface_value =
      std::make_unique<const interface_service>(37);
  container<> unique_interface_container;
  unique_interface_container
      .register_type<scope<external>,
                     storage<const std::unique_ptr<const interface_service> &>,
                     interfaces<service_interface>>(unique_interface_value);

  EXPECT_EQ(&unique_interface_container.resolve<const service_interface &>(),
            unique_interface_value.get());
  EXPECT_EQ(unique_interface_container.resolve<const service_interface *>(),
            unique_interface_value.get());
  EXPECT_THROW((void)unique_interface_container.resolve<service_interface &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)unique_interface_container.resolve<service_interface *>(),
               type_not_convertible_exception);
}

TEST(type_registration_test,
     const_alternative_storage_only_publishes_const_borrows) {
  using variant_type = std::variant<int, float>;

  const variant_type value{std::in_place_type<int>, 13};
  container<> variant_container;
  variant_container.register_type<
      scope<external>, storage<const variant_type &>, interfaces<int>>(value);

  EXPECT_EQ(variant_container.resolve<int>(), 13);
  EXPECT_EQ(&variant_container.resolve<const int &>(), &std::get<int>(value));
  EXPECT_EQ(variant_container.resolve<const int *>(), &std::get<int>(value));
}

TEST(type_registration_test,
     multidimensional_array_storage_preserves_exact_and_row_shapes) {
  using model = detail::binding_model<
      type_registration<scope<shared>, storage<int[2][3]>>>;
  // Each shape is planned by the interface it selects: the whole array by
  // itself, its rows by the row type.
  static_assert(plans_v<int (&)[2][3], detail::binding<int[2][3], model>>);
  static_assert(plans_v<int (*)[2][3], detail::binding<int[2][3], model>>);
  static_assert(plans_v<int (*)[3], detail::binding<int[3], model>>);

  container<> container;
  container.register_type<scope<shared>, storage<int[2][3]>>();

  auto *rows = container.resolve<int (*)[3]>();
  auto *whole = container.resolve<int (*)[2][3]>();
  auto *const_rows = container.resolve<const int (*)[3]>();
  auto *volatile_rows = container.resolve<volatile int (*)[3]>();
  auto *cv_whole = container.resolve<const volatile int (*)[2][3]>();

  ASSERT_EQ(&(*whole)[0][0], &rows[0][0]);
  ASSERT_EQ(rows, const_rows);
  ASSERT_EQ(rows, volatile_rows);
  ASSERT_EQ(whole, cv_whole);
  (*whole)[1][1] = 7;
  EXPECT_EQ(rows[1][1], 7);
}

TEST(type_registration_test, normalized_type_trait) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};
  struct annotation_tag {};
  using selected_key = detail::type_selector<annotation_tag>;

  static_assert(
      std::is_same_v<
          normalized_type_t<const std::shared_ptr<std::unique_ptr<A>> &>, A>);
  static_assert(
      std::is_same_v<normalized_type_t<const volatile A *const volatile>, A>);
  static_assert(std::is_same_v<normalized_type_t<const A *const *volatile>, A>);
  static_assert(std::is_same_v<normalized_type_t<volatile A(*)[3]>, A[3]>);
  static_assert(
      std::is_same_v<normalized_type_t<const volatile A(*)[3]>, A[3]>);
  static_assert(std::is_same_v<normalized_type_t<volatile A(&)[3]>, A[3]>);
  static_assert(
      std::is_same_v<normalized_type_t<annotated<I &, annotation_tag>>,
                     annotated<I, annotation_tag>>);
  static_assert(!request_may_escape_v<A>);
  static_assert(!request_may_escape_v<A &&>);
  static_assert(request_may_escape_v<A &>);
  static_assert(request_may_escape_v<const A &>);
  static_assert(request_may_escape_v<A *>);
  static_assert(request_may_escape_v<annotated<A &, annotation_tag>>);
  static_assert(request_may_escape_v<const A *>);
  static_assert(
      request_may_escape_v<dependency<A &, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<const A &, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<A *, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<const A *, key_type<annotation_tag>>>);
  static_assert(!request_may_escape_v<dependency<A, key_type<annotation_tag>>>);
  static_assert(
      !request_may_escape_v<dependency<A &&, key_type<annotation_tag>>>);
  static_assert(request_may_escape_v<detail::selected<A &, selected_key>>);
  static_assert(
      request_may_escape_v<detail::selected<const A &, selected_key>>);
  static_assert(request_may_escape_v<detail::selected<A *, selected_key>>);
  static_assert(
      request_may_escape_v<detail::selected<const A *, selected_key>>);
  static_assert(!request_may_escape_v<detail::selected<A, selected_key>>);
  static_assert(!request_may_escape_v<detail::selected<A &&, selected_key>>);
}

namespace abstract_interface_test {
struct I {
  virtual ~I() = default;
  virtual int value() const = 0;
};
struct A : I {
  int value() const override { return 7; }
};
// Reads the instance it is constructed with.
struct reader {
  explicit reader(I &instance) : value(instance.value()) {}
  int value;
};
} // namespace abstract_interface_test

TEST(type_registration_test, scope_stores_reference_only_when_given_it) {
  struct A {};

  static_assert(detail::has_storage_cell_v<external, A &>);
  static_assert(!detail::has_storage_cell_v<unique, A &>);
  static_assert(!detail::has_storage_cell_v<shared, A &>);
  static_assert(!detail::has_storage_cell_v<shared_cyclical, A &>);
  static_assert(!detail::binding_model<
                type_registration<scope<shared>, storage<A &>>>::valid);
  static_assert(detail::binding_model<
                type_registration<scope<external>, storage<A &>>>::valid);
}

TEST(type_registration_test, cyclical_scope_does_not_store_dynamic_arrays) {
  struct A {};

  static_assert(!detail::has_storage_cell_v<shared_cyclical, A[]>);
  static_assert(
      !detail::has_storage_cell_v<shared_cyclical, std::shared_ptr<A[]>>);
  static_assert(detail::has_storage_cell_v<shared_cyclical, A[2]>);
  static_assert(
      detail::has_storage_cell_v<shared_cyclical, std::shared_ptr<A>>);
}

// The routes of an abstract interface over a value include no value wrapper
// of the interface, which cannot exist.
TEST(type_registration_test, abstract_interface_binds_over_value_storage) {
  using abstract_interface_test::A;
  using abstract_interface_test::I;

  using abstract_interface_test::reader;

  container<> unique_container;
  unique_container.register_type<scope<unique>, storage<A>, interfaces<I>>();
  unique_container.register_type<scope<shared>, storage<reader>>();
  EXPECT_EQ(unique_container.resolve<reader &>().value, 7);

  container<> optional_container;
  optional_container
      .register_type<scope<shared>, storage<std::optional<A>>, interfaces<I>>();
  EXPECT_EQ(optional_container.resolve<I &>().value(), 7);
  EXPECT_EQ(optional_container.resolve<I *>()->value(), 7);

  static_container<bindings<
      dingo::bind<scope<shared>, storage<std::optional<A>>, interfaces<I>>>>
      static_optional;
  EXPECT_EQ(static_optional.resolve<I &>().value(), 7);
}

// A unique instance lives in the resolution context: a dependency can refer
// to it, the caller of the container cannot.
TEST(type_registration_test,
     unique_instance_is_referred_to_only_by_what_a_scope_keeps) {
  using abstract_interface_test::A;
  using abstract_interface_test::I;
  using abstract_interface_test::reader;

  // A class a shared scope keeps keeps the unique instance it refers to.
  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>, interfaces<I>>();
  runtime.register_type<scope<shared>, storage<reader>>();
  EXPECT_EQ(runtime.resolve<reader &>().value, 7);
  EXPECT_THROW(runtime.resolve<I &>(), type_not_convertible_exception);
  EXPECT_THROW(runtime.resolve<I *>(), type_not_convertible_exception);

  static_container<
      bindings<dingo::bind<scope<unique>, storage<A>, interfaces<I>>,
               dingo::bind<scope<shared>, storage<reader>>>>
      compile_time;
  EXPECT_EQ(compile_time.resolve<reader &>().value, 7);

  // A class returned to the caller would refer to an instance that is gone
  // when the resolution ends.
  container<> returned;
  returned.register_type<scope<unique>, storage<A>, interfaces<I>>();
  returned.register_type<scope<unique>, storage<reader>>();
  EXPECT_THROW(returned.resolve<reader>(), type_not_convertible_exception);
  EXPECT_THROW(returned.construct<reader>(), type_not_convertible_exception);
}

// A unique pointer is handed to the caller, not referred to in the context.
TEST(type_registration_test, unique_pointer_is_handed_to_caller) {
  using abstract_interface_test::A;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A *>>();
  std::unique_ptr<A> instance(runtime.resolve<A *>());
  EXPECT_EQ(instance->value(), 7);
}

// A resolved rvalue reference is returned by value, also when it is annotated.
TEST(type_registration_test, annotated_rvalue_request_returns_value) {
  struct tag {};
  struct A {};

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>,
                        interfaces<annotated<A, tag>>>();
  static_assert(
      std::is_same_v<decltype(runtime.resolve<annotated<A &&, tag>>()), A>);
  static_assert(
      std::is_same_v<decltype(runtime.resolve<annotated<A, tag> &&>()), A>);
  (void)runtime.resolve<annotated<A &&, tag>>();
}

TEST(type_registration_test, cyclical_scope_does_not_store_raw_pointers) {
  struct A {};

  static_assert(!detail::has_storage_cell_v<shared_cyclical, A *>);
  static_assert(detail::has_storage_cell_v<shared, A *>);
}

// A kept optional is copied as its element or as itself.
TEST(type_registration_test, kept_optional_is_copied_as_itself) {
  using abstract_interface_test::A;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<std::optional<A>>>();
  std::optional<A> copy = runtime.resolve<std::optional<A>>();
  ASSERT_TRUE(copy.has_value());
  EXPECT_NE(&*copy, &*runtime.resolve<std::optional<A> &>());
}

// A cyclical bounded array is published by its first element, as a shared one.
TEST(type_registration_test, cyclical_bounded_array_publishes_its_rows) {
  struct A {};

  container<> runtime;
  runtime.register_type<scope<shared_cyclical>, storage<A[2]>>();
  A *rows = runtime.resolve<A *>();
  A(&whole)[2] = runtime.resolve<A(&)[2]>();
  EXPECT_EQ(rows, &whole[0]);
  EXPECT_EQ(rows, runtime.resolve<A *>());
}

namespace fresh_value_test {
struct I {
  virtual ~I() = default;
  virtual int value() const = 0;
};
struct A : I {
  int value() const override { return 7; }
};
struct move_only {
  move_only() = default;
  move_only(move_only &&) = default;
  move_only(const move_only &) = delete;
};
// Owns a new instance of its interface.
struct owner {
  explicit owner(std::unique_ptr<I> init) : instance(std::move(init)) {}
  std::unique_ptr<I> instance;
};
} // namespace fresh_value_test

// A new value of a unique scope is moved into an owning handle of itself or of
// its interface.
TEST(type_registration_test, unique_value_is_moved_into_owning_handle) {
  using fresh_value_test::A;
  using fresh_value_test::I;
  using fresh_value_test::owner;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>, interfaces<A, I>>();
  runtime.register_type<scope<unique>, storage<owner>>();
  EXPECT_EQ(runtime.resolve<std::unique_ptr<A>>()->value(), 7);
  EXPECT_EQ(runtime.resolve<std::shared_ptr<I>>()->value(), 7);
  EXPECT_EQ(runtime.resolve<owner>().instance->value(), 7);

  static_container<
      bindings<dingo::bind<scope<unique>, storage<A>, interfaces<I>>,
               dingo::bind<scope<unique>, storage<owner>>>>
      compile_time;
  EXPECT_EQ(compile_time.resolve<std::unique_ptr<I>>()->value(), 7);
  EXPECT_EQ(compile_time.resolve<owner>().instance->value(), 7);
}

// A new optional of a unique scope gives its element away.
TEST(type_registration_test, unique_optional_is_moved_out_of) {
  using fresh_value_test::move_only;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<std::optional<move_only>>>();
  move_only value = runtime.resolve<move_only>();
  (void)value;
  static_assert(
      std::is_same_v<decltype(runtime.resolve<move_only &&>()), move_only>);
}

namespace operation_test {
struct A {
  int value() const { return 7; }
};
struct tag {};
// Takes its dependency as an annotated rvalue reference.
struct rvalue_consumer {
  explicit rvalue_consumer(annotated<A &&, tag> init) {
    A taken = static_cast<A &&>(init);
    value = taken.value();
  }
  int value = 0;
};
} // namespace operation_test

// construct<T &>() refers to an instance as resolve<T &>() does; nothing is
// constructed for it.
TEST(type_registration_test, construct_of_reference_resolves_it) {
  using operation_test::A;

  container<> kept;
  kept.register_type<scope<shared>, storage<std::optional<A>>>();
  EXPECT_EQ(&kept.construct<A &>(), &kept.resolve<A &>());
  EXPECT_EQ(&kept.construct<std::optional<A> &>(),
            &kept.resolve<std::optional<A> &>());

  container<> fresh;
  fresh.register_type<scope<unique>, storage<A>>();
  EXPECT_THROW(fresh.construct<A &>(), type_not_convertible_exception);
}

// A dependency of an annotated rvalue reference is made of the rvalue.
TEST(type_registration_test, annotated_rvalue_reference_dependency) {
  using operation_test::A;
  using operation_test::rvalue_consumer;
  using operation_test::tag;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>,
                        interfaces<annotated<A, tag>>>();
  runtime.register_type<
      scope<unique>, storage<rvalue_consumer>,
      factory<constructor<rvalue_consumer(annotated<A &&, tag>)>>>();
  EXPECT_EQ(runtime.resolve<rvalue_consumer>().value, 7);
}

// A request with a key in its type is looked up under that key by the caller
// of a runtime container, as it is for a dependency.
TEST(type_registration_test, runtime_request_keyed_by_type) {
  using operation_test::A;
  using key = key_type<std::size_t, 1>;
  struct traits : runtime_container_traits {
    using lookup_definition_type = lookups<associative<std::size_t, A>>;
  };

  container<traits> runtime;
  runtime.register_type<scope<shared>, storage<A>, interfaces<A>>(
      key_value{std::size_t(1)});
  A &kept = runtime.resolve<dependency<A &, key>>();
  EXPECT_EQ(&kept, &runtime.resolve<A &>(std::size_t(1)));
  EXPECT_EQ((runtime.construct<dependency<A, key>>().value()), 7);
}

namespace kept_dependency_test {
// Counts the parts alive, so that a test sees whether a service refers to one
// that is gone.
struct part {
  part() { ++alive; }
  part(const part &) { ++alive; }
  ~part() { --alive; }

  int value = 7;
  inline static int alive = 0;
};

struct service {
  explicit service(part &p) : used(p) {}

  part &used;
};

template <typename Scope> void check_runtime() {
  {
    container<> runtime;
    runtime.register_type<scope<unique>, storage<part>>();
    runtime.register_type<scope<Scope>, storage<service>,
                          factory<constructor<service(part &)>>>();
    service &kept = runtime.resolve<service &>();
    EXPECT_EQ(part::alive, 1);
    EXPECT_EQ(kept.used.value, 7);
  }
  EXPECT_EQ(part::alive, 0);
}

template <typename Scope> void check_static() {
  {
    static_container<
        bindings<dingo::bind<scope<unique>, storage<part>>,
                 dingo::bind<scope<Scope>, storage<service>,
                             factory<constructor<service(part &)>>>>>
        fixed;
    service &kept = fixed.template resolve<service &>();
    EXPECT_EQ(part::alive, 1);
    EXPECT_EQ(kept.used.value, 7);
  }
  EXPECT_EQ(part::alive, 0);
}
} // namespace kept_dependency_test

// A unique instance a kept one refers to is constructed for it, and lives as
// long as the container.
TEST(type_registration_test, kept_instance_keeps_unique_dependency) {
  using namespace kept_dependency_test;
  check_runtime<shared>();
  check_runtime<shared_cyclical>();
  check_static<shared>();
  check_static<shared_cyclical>();
}

// A binding publishes only the requests that select it: a bounded array binds
// its rows and itself, and each of them serves its own requests.
TEST(type_registration_test, binding_publishes_only_requests_it_selects) {
  struct A {};
  using model =
      detail::binding_model<type_registration<scope<shared>, storage<A[2]>>>;
  static_assert(plans_v<A *, detail::binding<A, model>>);
  static_assert(plans_v<A(*)[2], detail::binding<A[2], model>>);
  static_assert(plans_v<A(&)[2], detail::binding<A[2], model>>);
}

// A pointer to a bounded array is not constructed: new cannot make one, so
// construct resolves it.
TEST(type_registration_test, construct_of_pointer_to_bounded_array_resolves) {
  struct A {};

  container<> runtime;
  runtime.register_type<scope<shared>, storage<A[2]>>();
  A(*constructed)[2] = runtime.construct<A(*)[2]>();
  EXPECT_EQ(constructed, &runtime.resolve<A(&)[2]>());
}

// A handle of a dynamic array is made of the bound value by both containers;
// a default factory has no size to construct one with.
TEST(type_registration_test, construct_of_dynamic_array_handle_holds_value) {
  using operation_test::A;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>>();
  std::shared_ptr<A[]> made = runtime.construct<std::shared_ptr<A[]>>();
  EXPECT_EQ(made[0].value(), 7);

  static_container<bindings<dingo::bind<scope<unique>, storage<A>>>> fixed;
  std::shared_ptr<A[]> fixed_made = fixed.construct<std::shared_ptr<A[]>>();
  EXPECT_EQ(fixed_made[0].value(), 7);
}

// invoke returns what the callable returns, a reference included.
TEST(type_registration_test, invoke_returns_what_the_callable_returns) {
  using operation_test::A;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<A>>();
  A &kept = runtime.invoke([](A &a) -> A & { return a; });
  EXPECT_EQ(&kept, &runtime.resolve<A &>());

  static_container<bindings<dingo::bind<scope<shared>, storage<A>>>> fixed;
  A &fixed_kept = fixed.invoke([](A &a) -> A & { return a; });
  EXPECT_EQ(&fixed_kept, &fixed.resolve<A &>());
}

namespace detected_wrapper_test {
struct A {
  A() {}
  int value = 7;
};
// An aggregate that can be neither copied nor moved.
struct immovable {
  const std::unique_ptr<A> held;
};
} // namespace detected_wrapper_test

// invoke calls a callable directly, so a parameter of a type that can be
// neither copied nor moved compiles; such a value is not auto-constructed.
TEST(type_registration_test, invoke_takes_immovable_parameter) {
  using namespace detected_wrapper_test;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>>();
  EXPECT_THROW(runtime.invoke([](immovable) { return 0; }),
               type_not_found_exception);
}

namespace annotated_construct_test {
struct I {
  virtual ~I() = default;
  virtual int value() const = 0;
};
struct A : I {
  int value() const override { return 7; }
};
struct tag {};
} // namespace annotated_construct_test

// construct of an annotated pointer to an abstract interface refers to the
// instance, as for a plain one; nothing abstract is constructed.
TEST(type_registration_test, construct_of_annotated_abstract_pointer) {
  using namespace annotated_construct_test;
  using request = annotated<I *, tag>;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<A>,
                        interfaces<annotated<I, tag>>>();
  I *constructed = runtime.construct<request>();
  EXPECT_EQ(constructed, static_cast<I *>(runtime.resolve<request>()));
  EXPECT_EQ(constructed->value(), 7);

  static_container<bindings<
      dingo::bind<scope<shared>, storage<A>, interfaces<annotated<I, tag>>>>>
      fixed;
  I *fixed_constructed = fixed.construct<request>();
  EXPECT_EQ(fixed_constructed, static_cast<I *>(fixed.resolve<request>()));
}

// A shared_ptr of a bounded array owns the array, publishes its rows, and is
// made without a size from its factory.
TEST(type_registration_test, shared_pointer_of_bounded_array) {
  using operation_test::A;
  using handle = std::shared_ptr<A[2]>;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<handle>>();
  handle &kept = runtime.resolve<handle &>();
  EXPECT_EQ(runtime.resolve<A *>(), &kept[0]);
  EXPECT_EQ(kept[1].value(), 7);

  static_container<bindings<dingo::bind<scope<unique>, storage<handle>>>> fixed;
  handle made = fixed.resolve<handle>();
  EXPECT_EQ(made[1].value(), 7);
}

namespace immovable_unique_test {
struct immovable {
  immovable() {}
  immovable(const immovable &) = delete;
  immovable &operator=(const immovable &) = delete;
  int value = 7;
};
} // namespace immovable_unique_test

// A unique value a reference refers to is placed in the context of the
// resolution in place, so a value that can be neither copied nor moved is
// placed too.
TEST(type_registration_test, reference_to_immovable_unique_value) {
  using namespace immovable_unique_test;
  auto read = [](immovable &value) { return value.value; };

  container<> runtime;
  runtime.register_type<scope<unique>, storage<immovable>>();
  EXPECT_EQ(runtime.invoke(read), 7);

  static_container<bindings<dingo::bind<scope<unique>, storage<immovable>>>>
      fixed;
  EXPECT_EQ(fixed.invoke(read), 7);
}

// A shared optional is constructed in place, as its leaf, which can be
// neither copied nor moved, has to be.
TEST(type_registration_test, shared_optional_of_immovable_value) {
  using namespace immovable_unique_test;
  using stored = std::optional<immovable>;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<stored>>();
  EXPECT_EQ(runtime.resolve<immovable &>().value, 7);
  EXPECT_EQ(&runtime.resolve<stored &>().value(),
            &runtime.resolve<immovable &>());

  static_container<bindings<dingo::bind<scope<shared>, storage<stored>>>> fixed;
  EXPECT_EQ(fixed.resolve<immovable &>().value, 7);
}

namespace undetected_construct_test {
struct ambiguous {
  ambiguous(int) {}
  ambiguous(float) {}
};
using alternative = std::variant<ambiguous, int>;
alternative make() { return alternative(std::in_place_type<int>, 7); }
} // namespace undetected_construct_test

// The runtime container learns only at run time whether a type is bound: a
// type its default factory cannot construct is not found when it is not, and
// resolves when it is, as in the static container.
TEST(type_registration_test, runtime_construct_of_undetected_type) {
  using namespace undetected_construct_test;

  container<> runtime;
  EXPECT_THROW(runtime.construct<ambiguous>(), type_not_found_exception);
  runtime.register_type<scope<shared>, storage<alternative>,
                        factory<function<&make>>>();
  EXPECT_EQ(std::get<int>(runtime.construct<alternative>()), 7);

  static_container<bindings<dingo::bind<scope<shared>, storage<alternative>,
                                        factory<function<&make>>>>>
      fixed;
  EXPECT_EQ(std::get<int>(fixed.construct<alternative>()), 7);
}

namespace keyed_array_test {
struct A {
  A() {}
  int value = 7;
};
struct element_lookup : runtime_container_traits {
  using lookup_definition_type = lookups<associative<std::size_t, A>>;
};
struct array_lookups : runtime_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, A[2]>, associative<std::size_t, A>>;
};
} // namespace keyed_array_test

// A lookup is defined for the interface it names: an array and its element
// are two interfaces of a registration of an array, each with its own lookup.
// A keyed request selects the lookup of its interface, or else that of its
// normalized one.
TEST(type_registration_test, keyed_registration_of_array) {
  using namespace keyed_array_test;

  container<array_lookups> both;
  both.register_type<scope<shared>, storage<A[2]>>(key_value{std::size_t(1)});
  EXPECT_EQ(both.resolve<A(&)[2]>(key_value{std::size_t(1)})[1].value, 7);
  EXPECT_EQ(both.resolve<A *>(key_value{std::size_t(1)})->value, 7);
  EXPECT_THROW(both.resolve<A(&)[2]>(), type_not_found_exception);

  container<element_lookup> element;
  element.register_type<scope<shared>, storage<A[2]>>(
      key_value{std::size_t(1)});
  EXPECT_EQ(element.resolve<A *>(key_value{std::size_t(1)})->value, 7);
  EXPECT_THROW(element.resolve<A(&)[2]>(key_value{std::size_t(1)}),
               type_not_convertible_exception);
}

namespace construct_rvalue_test {
struct A {
  A() {}
  int value = 7;
};
} // namespace construct_rvalue_test

// construct<T &&>() resolves the request, as resolve<T &&>() does: it moves
// out of an instance made for it, and is refused one the container keeps.
TEST(type_registration_test, construct_of_rvalue_reference_resolves_it) {
  using construct_rvalue_test::A;

  container<> made;
  made.register_type<scope<unique>, storage<A>>();
  EXPECT_EQ(made.construct<A &&>().value, 7);
  EXPECT_EQ(made.construct<std::unique_ptr<A> &&>()->value, 7);

  container<> kept;
  kept.register_type<scope<shared>, storage<A>>();
  EXPECT_THROW(kept.construct<A &&>(), type_not_convertible_exception);

  static_container<bindings<dingo::bind<scope<unique>, storage<A>>>> fixed;
  EXPECT_EQ(fixed.construct<A &&>().value, 7);
}

namespace qualification_test {
struct I {
  virtual ~I() = default;
  virtual int get() const = 0;
};
struct T : I {
  T() {}
  int value = 7;
  int get() const override { return value; }
};
struct U {
  U() {}
  int value = 3;
};

// A handle the binding keeps is converted to one of a const element, which
// shares it, alone or held by an optional, and so is one of its interface.
template <typename Container> void check_kept(Container &container) {
  auto &stored = container.template resolve<std::shared_ptr<T> &>();
  auto converted = container.template resolve<std::shared_ptr<const T>>();
  EXPECT_EQ(converted.get(), stored.get());
  EXPECT_EQ(converted.use_count(), stored.use_count());
  auto held =
      container.template resolve<std::optional<std::shared_ptr<const T>>>();
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(held->get(), stored.get());
  auto interface = container.template resolve<std::shared_ptr<const I>>();
  EXPECT_EQ(interface.get(), static_cast<I *>(stored.get()));
}

// A handle a request makes of a value is made of a const element at once.
template <typename Container> void check_made(Container &container) {
  auto sole = container.template resolve<std::unique_ptr<const U>>();
  ASSERT_NE(sole, nullptr);
  EXPECT_EQ(sole->value, 3);
  auto shared = container.template resolve<std::shared_ptr<const U>>();
  ASSERT_NE(shared, nullptr);
  EXPECT_EQ(shared->value, 3);
}
} // namespace qualification_test

TEST(type_registration_test, handle_converts_to_one_of_a_const_element) {
  using namespace qualification_test;
  container<> runtime;
  runtime.register_type<scope<shared>, storage<std::shared_ptr<T>>,
                        interfaces<T, I>>();
  runtime.register_type<scope<unique>, storage<U>>();
  check_kept(runtime);
  check_made(runtime);

  static_container<bindings<
      dingo::bind<scope<shared>, storage<std::shared_ptr<T>>, interfaces<T, I>>,
      dingo::bind<scope<unique>, storage<U>>>>
      fixed;
  check_kept(fixed);
  check_made(fixed);
}

namespace unbound_test {
struct E {
  E() {}
  int value = 5;
};

// A handle of an array of a known bound converts to one of an array of
// unknown bound, which shares the same array: copied of one the binding
// keeps, moved out of one a request owns.
template <typename Container> void check_kept(Container &container) {
  auto &stored = container.template resolve<std::shared_ptr<E[2]> &>();
  auto converted = container.template resolve<std::shared_ptr<E[]>>();
  EXPECT_EQ(converted.get(), stored.get());
  EXPECT_EQ(converted.use_count(), stored.use_count());
  EXPECT_EQ(converted[1].value, 5);
}

template <typename Container> void check_made(Container &container) {
  auto converted = container.template resolve<std::shared_ptr<E[]>>();
  ASSERT_NE(converted, nullptr);
  EXPECT_EQ(converted.use_count(), 1);
  EXPECT_EQ(converted[1].value, 5);
}
} // namespace unbound_test

namespace adoption_test {
// An array a unique scope makes is taken over by a handle of an array: of
// its bound, as the pointer to the whole array it hands over, and of unknown
// bound, as the pointer to its first element it decays to.
template <typename Container> void check_adopted(Container &container) {
  using unbound_test::E;
  auto bounded = container.template resolve<std::shared_ptr<E[2]>>();
  ASSERT_NE(bounded, nullptr);
  EXPECT_EQ(bounded[1].value, 5);
  auto shared = container.template resolve<std::shared_ptr<E[]>>();
  ASSERT_NE(shared, nullptr);
  EXPECT_EQ(shared[1].value, 5);
  auto sole = container.template resolve<std::unique_ptr<E[]>>();
  ASSERT_NE(sole, nullptr);
  EXPECT_EQ(sole[1].value, 5);
}

} // namespace adoption_test

TEST(type_registration_test, handle_takes_over_an_array_a_unique_scope_makes) {
  using unbound_test::E;
  container<> runtime;
  runtime.register_type<scope<unique>, storage<E[2]>>();
  adoption_test::check_adopted(runtime);

  static_container<bindings<dingo::bind<scope<unique>, storage<E[2]>>>> fixed;
  adoption_test::check_adopted(fixed);
}

TEST(type_registration_test,
     handle_converts_to_one_of_an_array_of_unknown_bound) {
  using namespace unbound_test;
  container<> kept;
  kept.register_type<scope<shared>, storage<std::shared_ptr<E[2]>>>();
  check_kept(kept);
  container<> made;
  made.register_type<scope<unique>, storage<std::shared_ptr<E[2]>>>();
  check_made(made);

  static_container<
      bindings<dingo::bind<scope<shared>, storage<std::shared_ptr<E[2]>>>>>
      fixed_kept;
  check_kept(fixed_kept);
  static_container<
      bindings<dingo::bind<scope<unique>, storage<std::shared_ptr<E[2]>>>>>
      fixed_made;
  check_made(fixed_made);
}
