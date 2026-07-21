//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "type_registration_common.h"

#include <optional>

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
