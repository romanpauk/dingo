//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "type_registration_common.h"

#include <array>
#include <optional>
#include <variant>

namespace {
template <typename Type> struct fresh_value_storage {
  using type = Type;

  struct conversions {
    static constexpr bool is_stable = true;

    using value_types = type_list<runtime_type>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<>;
  };
};
} // namespace

TEST(type_registration_test,
     runtime_local_bindings_declare_the_registration_container) {
  using root_container = dingo::container<>;
  using empty_local_registration =
      type_registration<scope<shared>, storage<runtime_binding_local_service>,
                        dingo::bindings<>>;
  using empty_local_model = detail::binding_model<empty_local_registration>;
  static_assert(std::is_void_v<typename empty_local_model::bindings_type>);
  using empty_proxy =
      decltype(std::declval<root_container &>()
                   .template register_type<
                       scope<shared>, storage<runtime_binding_local_service>,
                       dingo::bindings<>>());
  using empty_container = typename empty_proxy::container_type;
  using empty_runtime_state = detail::runtime_binding_state_t<
      typename root_container::registry_type, empty_container,
      typename empty_local_model::storage_type,
      typename empty_local_model::bindings_type>;
  static_assert(std::is_same_v<typename empty_runtime_state::container_type,
                               empty_container>);
  static_assert(
      std::is_same_v<typename empty_runtime_state::resolution_container_type,
                     empty_container>);

  using non_empty_local_registration = type_registration<
      scope<shared>, storage<runtime_binding_local_service>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_binding_local_dependency>,
          factory<function<make_runtime_binding_local_dependency>>>>>;
  using non_empty_local_model =
      detail::binding_model<non_empty_local_registration>;
  static_assert(!std::is_void_v<typename non_empty_local_model::bindings_type>);
  using non_empty_proxy =
      decltype(std::declval<root_container &>()
                   .template register_type<
                       scope<shared>, storage<runtime_binding_local_service>,
                       dingo::bindings<dingo::bind<
                           scope<shared>,
                           storage<runtime_binding_local_dependency>,
                           factory<function<
                               make_runtime_binding_local_dependency>>>>>());
  using non_empty_container = typename non_empty_proxy::container_type;
  using non_empty_runtime_state = detail::runtime_binding_state_t<
      typename root_container::registry_type, non_empty_container,
      typename non_empty_local_model::storage_type,
      typename non_empty_local_model::bindings_type>;
  static_assert(!std::is_same_v<non_empty_container, empty_container>);
  static_assert(
      std::is_same_v<typename non_empty_container::static_bindings_type,
                     typename non_empty_local_model::bindings_type>);
  static_assert(std::is_same_v<typename non_empty_runtime_state::container_type,
                               non_empty_container>);
  static_assert(std::is_same_v<
                typename non_empty_runtime_state::resolution_container_type,
                non_empty_container>);

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_binding_local_service>,
      dependencies<runtime_binding_local_dependency &>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_binding_local_dependency>,
          factory<function<make_runtime_binding_local_dependency>>>>>();

  EXPECT_EQ(container.resolve<runtime_binding_local_service &>().value, 7);
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
  static_assert(model::use_interface_as_stored_leaf);
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
  static_assert(!model::use_interface_as_stored_leaf);
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
  struct A : I {};
  struct move_only {
    move_only() = default;
    move_only(const move_only &) = delete;
    move_only(move_only &&) = default;
  };

  using nested_handle = const std::shared_ptr<std::unique_ptr<A>> &;
  using nested_list = type_list<nested_handle, std::unique_ptr<A *>>;

  static_assert(std::is_same_v<leaf_type_t<nested_handle>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_handle, I>,
                               std::shared_ptr<std::unique_ptr<I>> &>);
  static_assert(
      std::is_same_v<rebind_type_t<nested_handle, I>, std::shared_ptr<I> &>);
  static_assert(std::is_same_v<leaf_type_t<nested_list>, type_list<A, A>>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_list, I>,
                               type_list<std::shared_ptr<std::unique_ptr<I>> &,
                                         std::unique_ptr<I *>>>);
  static_assert(std::is_same_v<leaf_type_t<std::optional<const A *>>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<const A *, I>, const I *>);
  static_assert(std::is_same_v<rebind_leaf_t<std::optional<const A *>, I>,
                               std::optional<const I *>>);
  static_assert(
      std::is_same_v<request_lookup_type_t<const A *>, runtime_type *>);
  static_assert(std::is_same_v<request_lookup_type_t<std::optional<const A *>>,
                               std::optional<const runtime_type *>>);
  using exact_const_optional_pointer = exact_lookup<const std::optional<A>> *;
  static_assert(std::is_same_v<lookup_type_t<exact_const_optional_pointer>,
                               std::optional<runtime_type> *>);
  static_assert(std::is_same_v<resolved_type_t<exact_const_optional_pointer,
                                               const std::optional<A> *>,
                               const std::optional<A> *>);
  using exact_optional_value = exact_lookup<std::optional<A>>;
  static_assert(std::is_same_v<lookup_type_t<exact_optional_value>,
                               std::optional<runtime_type>>);
  using exact_optional_reference = exact_lookup<std::optional<A>> &;
  static_assert(std::is_same_v<lookup_type_t<exact_optional_reference>,
                               std::optional<runtime_type> &>);
  using exact_optional_unique_rvalue =
      exact_lookup<std::unique_ptr<std::optional<A>>> &&;
  static_assert(
      std::is_same_v<lookup_type_t<exact_optional_unique_rvalue>,
                     std::unique_ptr<std::optional<runtime_type>> &&>);

  using plain_pointer_model =
      detail::binding_model<type_registration<scope<external>, storage<A *>>>;
  using plain_pointer_capabilities = detail::binding_capability_types<
      A, typename plain_pointer_model::storage_type>;
  static_assert(std::is_same_v<typename plain_pointer_capabilities::value_types,
                               type_list<runtime_type>>);
  static_assert(
      std::is_same_v<typename plain_pointer_capabilities::pointer_types,
                     type_list<runtime_type *>>);

  using optional_pointer = std::optional<A> *;
  using optional_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<optional_pointer>>>;
  using optional_pointer_capabilities = detail::binding_capability_types<
      A, typename optional_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<typename optional_pointer_capabilities::value_types,
                     type_list<exact_lookup<std::optional<A>>>>);
  static_assert(std::is_same_v<
                typename optional_pointer_capabilities::lvalue_reference_types,
                type_list<exact_lookup<std::optional<A>> &>>);
  static_assert(
      std::is_same_v<typename optional_pointer_capabilities::pointer_types,
                     type_list<exact_lookup<std::optional<A>> *>>);

  using external_move_only_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<move_only *>>>;
  using external_capabilities = detail::binding_capability_types<
      move_only, typename external_move_only_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<typename external_capabilities::value_types, type_list<>>);
  static_assert(std::is_same_v<typename external_capabilities::pointer_types,
                               type_list<runtime_type *>>);

  using shared_move_only_model = detail::binding_model<
      type_registration<scope<shared>, storage<move_only>>>;
  using shared_capabilities = detail::binding_capability_types<
      move_only, typename shared_move_only_model::storage_type>;
  static_assert(
      std::is_same_v<typename shared_capabilities::value_types, type_list<>>);
  static_assert(
      std::is_same_v<typename shared_capabilities::lvalue_reference_types,
                     type_list<runtime_type &>>);

  using unique_move_only_model = detail::binding_model<
      type_registration<scope<unique>, storage<move_only>>>;
  using unique_capabilities = detail::binding_capability_types<
      move_only, typename unique_move_only_model::storage_type>;
  static_assert(
      std::is_same_v<typename unique_capabilities::value_types,
                     type_list<runtime_type, std::optional<runtime_type>>>);

  using fresh_capabilities =
      detail::binding_capability_types<move_only,
                                       fresh_value_storage<move_only>>;
  static_assert(std::is_same_v<typename fresh_capabilities::value_types,
                               type_list<runtime_type>>);

  using shared_variant =
      std::variant<std::shared_ptr<move_only>, std::monostate>;
  using shared_variant_model = detail::binding_model<
      type_registration<scope<shared>, storage<shared_variant>>>;
  using shared_variant_capabilities = detail::binding_capability_types<
      std::shared_ptr<move_only>, typename shared_variant_model::storage_type>;
  static_assert(
      std::is_same_v<typename shared_variant_capabilities::value_types,
                     type_list<std::shared_ptr<move_only>>>);

  using unique_variant =
      std::variant<std::unique_ptr<move_only>, std::monostate>;
  using unique_variant_model = detail::binding_model<
      type_registration<scope<unique>, storage<unique_variant>>>;
  using unique_variant_capabilities = detail::binding_capability_types<
      std::unique_ptr<move_only>, typename unique_variant_model::storage_type>;
  static_assert(
      std::is_same_v<typename unique_variant_capabilities::value_types,
                     type_list<std::unique_ptr<move_only>, runtime_type>>);

  using external_nested_move_only_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<std::optional<move_only> *>>>;
  using nested_capabilities = detail::binding_capability_types<
      move_only,
      typename external_nested_move_only_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<typename nested_capabilities::value_types, type_list<>>);
  static_assert(
      std::is_same_v<typename nested_capabilities::pointer_types,
                     type_list<exact_lookup<std::optional<move_only>> *>>);

  using const_array_pointer = const std::array<A, 2> *;
  using const_array_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<const_array_pointer>>>;
  using const_array_pointer_capabilities = detail::binding_capability_types<
      A, typename const_array_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<typename const_array_pointer_capabilities::value_types,
                     type_list<exact_lookup<std::array<A, 2>>>>);
  static_assert(
      std::is_same_v<
          typename const_array_pointer_capabilities::lvalue_reference_types,
          type_list<exact_lookup<const std::array<A, 2>> &>>);
  static_assert(
      std::is_same_v<typename const_array_pointer_capabilities::pointer_types,
                     type_list<exact_lookup<const std::array<A, 2>> *>>);

  using unique_optional_pointer_model = detail::binding_model<
      type_registration<scope<unique>, storage<optional_pointer>>>;
  using unique_optional_pointer_capabilities = detail::binding_capability_types<
      A, typename unique_optional_pointer_model::storage_type>;
  static_assert(std::is_same_v<
                typename unique_optional_pointer_capabilities::value_types,
                type_list<exact_lookup<std::unique_ptr<std::optional<A>>>,
                          exact_lookup<std::shared_ptr<std::optional<A>>>>>);
  static_assert(
      std::is_same_v<
          typename unique_optional_pointer_capabilities::rvalue_reference_types,
          type_list<exact_lookup<std::unique_ptr<std::optional<A>>> &&,
                    exact_lookup<std::shared_ptr<std::optional<A>>> &&>>);
  static_assert(std::is_same_v<
                typename unique_optional_pointer_capabilities::pointer_types,
                type_list<exact_lookup<std::optional<A>> *>>);
}

TEST(type_registration_test,
     nested_raw_pointer_storage_resolves_exact_capability_shapes) {
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
