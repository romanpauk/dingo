//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "type_registration_common.h"

namespace {
bool runtime_proxy_service_should_throw = false;
}

TEST(type_registration_test,
     runtime_scoped_source_allocation_is_destroyed_when_resolution_throws) {
  runtime_scoped_source_dependency::destructions = 0;

  dingo::container<> container;
  container.register_type<scope<shared>,
                          storage<runtime_scoped_source_throwing_service>>();
  container.register_type<scope<unique>,
                          storage<runtime_scoped_source_dependency>>();

  EXPECT_THROW((container.resolve<runtime_scoped_source_throwing_service &>()),
               std::runtime_error);
  EXPECT_GT(runtime_scoped_source_dependency::destructions, 0);
}

TEST(type_registration_test,
     runtime_shared_storage_retains_unique_dependency_source) {
  runtime_retained_source_unique_dependency::live_count = 0;
  runtime_retained_source_unique_dependency::destructions = 0;
  runtime_retained_source_shared_service::should_throw = false;
  runtime_retained_source_shared_service::dependency_was_live_at_destruction =
      false;

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_retained_source_shared_service>,
        dependencies<runtime_retained_source_unique_dependency &>>();
    container.register_type<
        scope<unique>, storage<runtime_retained_source_unique_dependency>>();

    auto &service =
        container.resolve<runtime_retained_source_shared_service &>();
    EXPECT_TRUE(service.dependency_is_live());
    EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 1);
  }

  EXPECT_TRUE(runtime_retained_source_shared_service::
                  dependency_was_live_at_destruction);
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_shared_storage_retains_unique_pointer_dependency_source) {
  runtime_retained_source_pointer_dependency::live_count = 0;
  runtime_retained_source_pointer_service::dependency_was_live_at_destruction =
      false;

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_retained_source_pointer_service>,
        dependencies<runtime_retained_source_pointer_dependency *>>();
    container.register_type<
        scope<unique>, storage<runtime_retained_source_pointer_dependency *>,
        factory<runtime_retained_source_pointer_factory>>();

    auto &service =
        container.resolve<runtime_retained_source_pointer_service &>();
    EXPECT_TRUE(service.dependency_is_live());
    EXPECT_EQ(runtime_retained_source_pointer_dependency::live_count, 1);
  }

  EXPECT_TRUE(runtime_retained_source_pointer_service::
                  dependency_was_live_at_destruction);
  EXPECT_EQ(runtime_retained_source_pointer_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_retained_source_dependency_rolls_back_after_failed_shared) {
  runtime_retained_source_unique_dependency::live_count = 0;
  runtime_retained_source_unique_dependency::destructions = 0;
  runtime_retained_source_shared_service::should_throw = true;
  runtime_retained_source_shared_service::dependency_was_live_at_destruction =
      false;

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_retained_source_shared_service>,
      dependencies<runtime_retained_source_unique_dependency &>>();
  container.register_type<scope<unique>,
                          storage<runtime_retained_source_unique_dependency>>();

  EXPECT_THROW((container.resolve<runtime_retained_source_shared_service &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 0);

  runtime_retained_source_shared_service::should_throw = false;
  auto &service = container.resolve<runtime_retained_source_shared_service &>();
  EXPECT_TRUE(service.dependency_is_live());
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 1);
}

TEST(type_registration_test,
     committed_runtime_retained_source_survives_later_failed_transaction) {
  runtime_retained_source_unique_dependency::live_count = 0;
  runtime_retained_source_unique_dependency::destructions = 0;
  runtime_retained_source_shared_service::should_throw = false;
  runtime_retained_source_shared_service::dependency_was_live_at_destruction =
      false;
  runtime_scoped_local_dependency::constructions = 0;
  runtime_scoped_local_dependency::destructions = 0;
  runtime_scoped_local_service::should_throw = true;

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_retained_source_shared_service>,
      dependencies<runtime_retained_source_unique_dependency &>>();
  container.register_type<scope<unique>,
                          storage<runtime_retained_source_unique_dependency>>();
  container.register_type<scope<shared>, storage<runtime_scoped_local_service>,
                          dependencies<runtime_scoped_local_dependency &>>();
  container
      .register_type<scope<unique>, storage<runtime_scoped_local_dependency>>();

  auto &service = container.resolve<runtime_retained_source_shared_service &>();
  ASSERT_TRUE(service.dependency_is_live());

  EXPECT_THROW((container.resolve<runtime_scoped_local_service &>()),
               std::runtime_error);

  EXPECT_TRUE(service.dependency_is_live());
  EXPECT_EQ(std::addressof(service),
            std::addressof(
                container.resolve<runtime_retained_source_shared_service &>()));
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 1);
}

TEST(type_registration_test,
     runtime_retained_source_storage_resets_before_retained_sources) {
  std::vector<int> events;
  runtime_retained_source_order_dependency::events = std::addressof(events);

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_retained_source_order_service>,
        dependencies<runtime_retained_source_order_dependency &>>();
    container.register_type<
        scope<unique>, storage<runtime_retained_source_order_dependency>>();

    auto &service =
        container.resolve<runtime_retained_source_order_service &>();
    ASSERT_TRUE(service.dependency_is_live());
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
  runtime_retained_source_order_dependency::events = nullptr;
}

TEST(type_registration_test,
     runtime_policy_unique_shared_unique_keeps_shared_committed) {
  runtime_policy_nested_unique_dependency::live_count = 0;

  {
    dingo::container<> container;
    container.register_type<
        scope<unique>, storage<runtime_policy_outer_unique_service>,
        dependencies<runtime_policy_nested_shared_service &>>();
    container.register_type<
        scope<shared>, storage<runtime_policy_nested_shared_service>,
        dependencies<runtime_policy_nested_unique_dependency &>>();
    container.register_type<scope<unique>,
                            storage<runtime_policy_nested_unique_dependency>>();

    auto outer = container.resolve<runtime_policy_outer_unique_service>();
    EXPECT_TRUE(outer.shared_dependency_is_live());
    EXPECT_EQ(runtime_policy_nested_unique_dependency::live_count, 1);

    auto &shared = container.resolve<runtime_policy_nested_shared_service &>();
    EXPECT_EQ(std::addressof(shared), outer.shared_);
    EXPECT_TRUE(shared.dependency_is_live());
    EXPECT_EQ(runtime_policy_nested_unique_dependency::live_count, 1);
  }

  EXPECT_EQ(runtime_policy_nested_unique_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_policy_shared_value_unique_dependency_is_ephemeral) {
  runtime_policy_value_unique_dependency::live_count = 0;
  runtime_policy_value_unique_dependency::destructions = 0;

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_policy_value_shared_service>,
        dependencies<runtime_policy_value_unique_dependency>>();
    container.register_type<scope<unique>,
                            storage<runtime_policy_value_unique_dependency>>();

    auto &service = container.resolve<runtime_policy_value_shared_service &>();
    EXPECT_EQ(service.value, 29);
    EXPECT_EQ(runtime_policy_value_unique_dependency::live_count, 0);
    EXPECT_GT(runtime_policy_value_unique_dependency::destructions, 0);

    auto &again = container.resolve<runtime_policy_value_shared_service &>();
    EXPECT_EQ(std::addressof(again), std::addressof(service));
    EXPECT_EQ(runtime_policy_value_unique_dependency::live_count, 0);
  }

  EXPECT_EQ(runtime_policy_value_unique_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_local_resolution_container_rolls_back_after_failed_resolution) {
  runtime_scoped_local_dependency::constructions = 0;
  runtime_scoped_local_dependency::destructions = 0;
  runtime_scoped_local_service::should_throw = true;

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_scoped_local_service>,
      dependencies<runtime_scoped_local_dependency &>,
      dingo::bindings<dingo::bind<scope<shared>,
                                  storage<runtime_scoped_local_dependency>>>>();

  EXPECT_THROW((container.resolve<runtime_scoped_local_service &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_scoped_local_dependency::constructions, 1);
  EXPECT_EQ(runtime_scoped_local_dependency::destructions, 1);

  runtime_scoped_local_service::should_throw = false;
  (void)container.resolve<runtime_scoped_local_service &>();
  EXPECT_EQ(runtime_scoped_local_dependency::constructions, 2);
  EXPECT_EQ(runtime_scoped_local_dependency::destructions, 1);
}

TEST(type_registration_test,
     first_failed_shared_resolve_resets_partial_storage) {
  runtime_shared_retry_service::construction_attempts = 0;
  runtime_shared_retry_service::should_throw = true;

  dingo::container<> container;
  container
      .register_type<scope<shared>, storage<runtime_shared_retry_service>>();

  EXPECT_THROW((container.resolve<runtime_shared_retry_service &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_shared_retry_service::construction_attempts, 1);

  runtime_shared_retry_service::should_throw = false;
  (void)container.resolve<runtime_shared_retry_service &>();
  EXPECT_EQ(runtime_shared_retry_service::construction_attempts, 2);
}

TEST(type_registration_test,
     later_failed_dependent_resolve_preserves_committed_shared_storage) {
  runtime_shared_state_dependency::constructions = 0;
  runtime_shared_state_dependency::destructions = 0;

  dingo::container<> container;
  container
      .register_type<scope<shared>, storage<runtime_shared_state_dependency>>();

  auto &committed = container.resolve<runtime_shared_state_dependency &>();
  EXPECT_EQ(runtime_shared_state_dependency::constructions, 1);
  EXPECT_EQ(runtime_shared_state_dependency::destructions, 0);

  container.register_type<scope<shared>,
                          storage<runtime_shared_state_throwing_consumer>,
                          dependencies<runtime_shared_state_dependency &>>();

  EXPECT_THROW((container.resolve<runtime_shared_state_throwing_consumer &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_shared_state_dependency::constructions, 1);
  EXPECT_EQ(runtime_shared_state_dependency::destructions, 0);
  EXPECT_EQ(
      std::addressof(container.resolve<runtime_shared_state_dependency &>()),
      std::addressof(committed));
}

TEST(type_registration_test,
     failed_first_shared_cyclical_resolve_rolls_back_publication) {
  runtime_cyclical_rollback_implementation::should_throw = true;

  dingo::container<> container;
  container.register_type<
      scope<shared_cyclical>,
      storage<std::shared_ptr<runtime_cyclical_rollback_implementation>>,
      interfaces<runtime_cyclical_rollback_interface,
                 runtime_cyclical_rollback_implementation>>();

  EXPECT_THROW(
      (container
           .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>()),
      std::runtime_error);

  runtime_cyclical_rollback_implementation::should_throw = false;
  auto &resolved =
      container
          .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>();
  EXPECT_NE(resolved.get(), nullptr);
}

TEST(type_registration_test,
     later_failed_resolve_preserves_committed_shared_cyclical_storage) {
  runtime_cyclical_rollback_implementation::should_throw = false;

  dingo::container<> container;
  container.register_type<
      scope<shared_cyclical>,
      storage<std::shared_ptr<runtime_cyclical_rollback_implementation>>,
      interfaces<runtime_cyclical_rollback_interface,
                 runtime_cyclical_rollback_implementation>>();

  auto &committed =
      container
          .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>();

  container.register_type<
      scope<shared>, storage<runtime_cyclical_rollback_consumer>,
      dependencies<std::shared_ptr<runtime_cyclical_rollback_interface> &>>();
  EXPECT_THROW((container.resolve<runtime_cyclical_rollback_consumer &>()),
               std::runtime_error);

  auto &after_failure =
      container
          .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>();
  EXPECT_EQ(std::addressof(after_failure), std::addressof(committed));
  EXPECT_EQ(after_failure.get(), committed.get());
}

TEST(type_registration_test,
     runtime_context_construct_uses_explicit_lifetimes) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  {
    {
      container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
      runtime_transaction transaction(runtime, scratch);
      {
        runtime_context context(scratch, transaction);
        (void)context.construct<runtime_arena_tracker>(ephemeral_scope, &events,
                                                       1);
        (void)context.construct<runtime_arena_tracker>(persistent_scope,
                                                       &events, 2);
        (void)context.construct<runtime_arena_tracker>(ephemeral_scope, &events,
                                                       3);
        transaction.commit();
      }

      EXPECT_EQ(events, (std::vector<int>{3, 1}));
    }
  }

  EXPECT_EQ(events, (std::vector<int>{3, 1, 2}));
}

TEST(type_registration_test,
     runtime_context_detection_construction_uses_explicit_scope) {
  std::vector<int> events;
  runtime_policy_detected_tracker::events = std::addressof(events);

  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime_transaction transaction(runtime, scratch);
    {
      dingo::container<> container;
      runtime_context context(scratch, transaction);

      runtime_policy_detected_tracker::next_value = 1;
      (void)context.construct<runtime_policy_detected_tracker &,
                              detail::constructor_shape>(ephemeral_scope,
                                                         container);
      runtime_policy_detected_tracker::next_value = 2;
      (void)context.construct<runtime_policy_detected_tracker &,
                              detail::constructor_shape>(persistent_scope,
                                                         container);
      transaction.commit();
    }

    EXPECT_EQ(events, (std::vector<int>{1}));
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
  runtime_policy_detected_tracker::events = nullptr;
}

TEST(type_registration_test, runtime_context_explicit_scopes_are_independent) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime_transaction transaction(runtime, scratch);
    {
      runtime_context context(scratch, transaction);
      const auto retained = persistent_scope;
      const auto temporary = ephemeral_scope;
      (void)context.construct<runtime_arena_tracker>(retained, &events, 2);
      (void)context.construct<runtime_arena_tracker>(temporary, &events, 1);
      transaction.commit();
    }

    EXPECT_EQ(events, (std::vector<int>{1}));
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(type_registration_test,
     runtime_context_scratch_destructor_runs_once_on_commit) {
  runtime_scoped_source_dependency::destructions = 0;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction transaction(runtime, scratch);
  {
    runtime_context context(scratch, transaction);
    (void)context.construct<runtime_scoped_source_dependency>(ephemeral_scope);
    transaction.commit();
  }

  EXPECT_EQ(runtime_scoped_source_dependency::destructions, 1);
}

TEST(type_registration_test,
     runtime_context_scratch_destructor_runs_once_on_rollback) {
  runtime_scoped_source_dependency::destructions = 0;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    {
      runtime_context context(scratch, transaction);
      (void)context.construct<runtime_scoped_source_dependency>(
          ephemeral_scope);
    }
  }

  EXPECT_EQ(runtime_scoped_source_dependency::destructions, 1);
}

TEST(type_registration_test,
     runtime_context_ephemeral_store_preserves_pending_transaction_actions) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    {
      runtime_context context(scratch, transaction);
      (void)context.construct<runtime_arena_tracker>(ephemeral_scope, &events,
                                                     1);
      context.on_rollback([&events]() noexcept { events.push_back(2); });
    }

    EXPECT_EQ(events, (std::vector<int>{1}));
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(type_registration_test,
     successful_runtime_registration_keeps_state_until_container_destruction) {
  int factory_destructions = 0;
  {
    dingo::container<> container;
    runtime_registration_lifetime_factory factory(&factory_destructions);
    container.register_type<scope<shared>,
                            storage<runtime_registration_lifetime_service>>(
        std::move(factory));
    (void)container.resolve<runtime_registration_lifetime_service &>();
    EXPECT_EQ(factory_destructions, 0);
  }

  EXPECT_EQ(factory_destructions, 1);
}

TEST(type_registration_test,
     nested_runtime_registration_rolls_back_with_throwing_root_operation) {
  dingo::container<> container;

  EXPECT_THROW((container.construct<nested_transaction_outer_service>(
                   callable([&container]() -> nested_transaction_outer_service {
                     container.register_type<
                         scope<shared>,
                         storage<nested_transaction_registered_service>>();
                     throw std::runtime_error("root construction failed");
                   }))),
               std::runtime_error);

  EXPECT_THROW((container.resolve<nested_transaction_registered_service &>()),
               type_not_found_exception);

  container.register_type<scope<shared>,
                          storage<nested_transaction_registered_service>>();
  EXPECT_NO_THROW(
      (container.resolve<nested_transaction_registered_service &>()));
}

TEST(type_registration_test,
     nested_runtime_resolution_commits_with_root_resolution) {
  nested_transaction_resolved_service::constructions = 0;
  nested_transaction_resolved_service::destructions = 0;

  {
    dingo::container<> container;
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolved_service>>();
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolving_service>>(
        callable([&container] {
          return nested_transaction_resolving_service{std::addressof(
              container.resolve<nested_transaction_resolved_service &>())};
        }));

    auto &outer = container.resolve<nested_transaction_resolving_service &>();
    auto &dependency =
        container.resolve<nested_transaction_resolved_service &>();

    EXPECT_EQ(outer.dependency, std::addressof(dependency));
    EXPECT_EQ(nested_transaction_resolved_service::constructions, 1);
    EXPECT_EQ(nested_transaction_resolved_service::destructions, 0);
  }

  EXPECT_EQ(nested_transaction_resolved_service::destructions, 1);
}

TEST(type_registration_test,
     nested_runtime_resolution_rolls_back_with_throwing_root_resolution) {
  nested_transaction_resolved_service::constructions = 0;
  nested_transaction_resolved_service::destructions = 0;

  {
    dingo::container<> container;
    bool should_throw = true;
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolved_service>>();
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolving_service>>(
        callable([&container, &should_throw] {
          auto &dependency =
              container.resolve<nested_transaction_resolved_service &>();
          if (should_throw) {
            throw std::runtime_error("root resolution failed");
          }
          return nested_transaction_resolving_service{
              std::addressof(dependency)};
        }));

    EXPECT_THROW((container.resolve<nested_transaction_resolving_service &>()),
                 std::runtime_error);
    EXPECT_EQ(nested_transaction_resolved_service::constructions, 1);
    EXPECT_EQ(nested_transaction_resolved_service::destructions, 1);

    should_throw = false;
    auto &outer = container.resolve<nested_transaction_resolving_service &>();
    auto &dependency =
        container.resolve<nested_transaction_resolved_service &>();

    EXPECT_EQ(outer.dependency, std::addressof(dependency));
    EXPECT_EQ(nested_transaction_resolved_service::constructions, 2);
    EXPECT_EQ(nested_transaction_resolved_service::destructions, 1);
  }

  EXPECT_EQ(nested_transaction_resolved_service::destructions, 2);
}

TEST(type_registration_test,
     failed_duplicate_runtime_registration_rolls_back_constructed_state) {
  int first_factory_destructions = 0;
  int duplicate_factory_destructions = 0;
  {
    dingo::container<> container;
    runtime_registration_lifetime_factory first(&first_factory_destructions);
    runtime_registration_lifetime_factory duplicate(
        &duplicate_factory_destructions);

    container.register_type<scope<shared>,
                            storage<runtime_registration_lifetime_service>>(
        std::move(first));
    EXPECT_THROW(
        (container.register_type<
            scope<shared>, storage<runtime_registration_lifetime_service>>(
            std::move(duplicate))),
        lookup_already_registered_exception);

    EXPECT_EQ(first_factory_destructions, 0);
    EXPECT_EQ(duplicate_factory_destructions, 1);
  }

  EXPECT_EQ(first_factory_destructions, 1);
  EXPECT_EQ(duplicate_factory_destructions, 1);
}

TEST(type_registration_test,
     runtime_binding_state_constructs_child_container_on_demand) {
  counting_runtime_binding_container parent(nullptr);
  counting_runtime_binding_container::constructions = 0;

  struct owner {
    using allocator_type = counting_runtime_binding_container::allocator_type;
    using parent_container_type = counting_runtime_binding_container;

    explicit owner(parent_container_type *parent)
        : allocator(parent->get_allocator()),
          runtime_storage(parent->get_allocator()), parent_storage(parent) {}

    container_runtime<allocator_type> &runtime() { return runtime_storage; }
    parent_container_type *parent() { return parent_storage; }
    allocator_type &get_allocator() { return allocator; }

    allocator_type allocator;
    container_runtime<allocator_type> runtime_storage;
    parent_container_type *parent_storage;
  } state_owner(&parent);

  runtime_binding_state<owner, counting_runtime_binding_container,
                        counting_runtime_binding_storage>
      state(&state_owner);

  EXPECT_EQ(counting_runtime_binding_container::constructions, 0);
  (void)state.storage();
  EXPECT_EQ(counting_runtime_binding_container::constructions, 0);

  auto &container = state.container();
  EXPECT_EQ(counting_runtime_binding_container::constructions, 1);
  EXPECT_EQ(std::addressof(state.container()), std::addressof(container));
  EXPECT_EQ(counting_runtime_binding_container::constructions, 1);
}

TEST(type_registration_test,
     runtime_unique_registration_constructs_nested_container_on_demand) {
  runtime_unique_local_dependency::live_count = 0;

  dingo::container<> container;
  container.register_type<
      scope<unique>, storage<runtime_unique_local_service>,
      dependencies<runtime_unique_local_dependency &>,
      dingo::bindings<dingo::bind<
          scope<unique>, storage<runtime_unique_local_dependency>,
          factory<function<make_runtime_unique_local_dependency>>>>>();

  auto service = container.resolve<runtime_unique_local_service>();
  EXPECT_EQ(service.value, 13);
  EXPECT_TRUE(service.dependency_was_live);
  EXPECT_EQ(runtime_unique_local_dependency::live_count, 0);
}

TEST(type_registration_test,
     registration_containers_isolate_bindings_in_shared_runtime_domain) {
  dingo::container<runtime_domain_traits> container;
  auto first =
      container
          .register_type<scope<shared>, storage<runtime_domain_first_service>,
                         dependencies<runtime_domain_dependency &>>();
  auto second = container.register_type<
      scope<shared>, storage<runtime_domain_second_service>,
      dependencies<std::vector<runtime_domain_dependency *>>>();

  first->register_type<scope<shared>, storage<runtime_domain_dependency>>(
      callable([] { return runtime_domain_dependency{11}; }));
  second->register_type<scope<shared>, storage<runtime_domain_dependency>>(
      callable([] { return runtime_domain_dependency{22}; }));

  EXPECT_EQ(container.resolve<runtime_domain_first_service &>().value, 11);
  auto &second_service = container.resolve<runtime_domain_second_service &>();
  EXPECT_EQ(second_service.value, 22);
  EXPECT_EQ(second_service.count, 1);
}

TEST(type_registration_test,
     registration_static_bindings_and_proxy_registrations_compose) {
  dingo::container<> container;
  container
      .register_type<scope<shared>, storage<runtime_composed_root_dependency>>(
          callable([] { return runtime_composed_root_dependency{11}; }));
  auto child = container.register_type<
      scope<shared>, storage<runtime_composed_service>,
      dependencies<runtime_composed_static_dependency &,
                   runtime_composed_child_dependency &,
                   runtime_composed_root_dependency &>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_composed_static_dependency>>>>();

  child->register_type<scope<shared>,
                       storage<runtime_composed_child_dependency>>(
      callable([] { return runtime_composed_child_dependency{13}; }));

  auto &service = container.resolve<runtime_composed_service &>();
  EXPECT_EQ(service.static_result, 7);
  EXPECT_EQ(service.child_result, 13);
  EXPECT_EQ(service.root_result, 11);
}

TEST(type_registration_test,
     proxy_binding_dependencies_resolve_through_registration_container) {
  struct static_dependency {
    static_dependency() : value(7) {}

    int value;
  };
  struct runtime_dependency {
    runtime_dependency() : value(11) {}

    int value;
  };
  struct nested_runtime_dependency {
    nested_runtime_dependency() : value(13) {}

    int value;
  };
  struct proxy_service {
    proxy_service(static_dependency &static_value,
                  runtime_dependency &runtime_value,
                  nested_runtime_dependency &nested_runtime_value)
        : value(static_value.value + runtime_value.value +
                nested_runtime_value.value) {
      if (runtime_proxy_service_should_throw) {
        throw std::runtime_error("resolution failed");
      }
    }

    int value;
  };
  struct root_service {
    explicit root_service(proxy_service &dependency)
        : value(dependency.value) {}

    int value;
  };

  dingo::container<> container;
  runtime_proxy_service_should_throw = true;
  auto child = container.register_type<
      scope<shared>, storage<root_service>, dependencies<proxy_service &>,
      dingo::bindings<
          dingo::bind<scope<shared>, storage<static_dependency>>>>();
  child->register_type<scope<shared>, storage<runtime_dependency>>();
  auto nested = child->register_type<
      scope<shared>, storage<proxy_service>,
      dependencies<static_dependency &, runtime_dependency &,
                   nested_runtime_dependency &>>();
  nested->register_type<scope<shared>, storage<nested_runtime_dependency>>();

  auto *child_container = std::addressof(child.get());
  auto *nested_container = std::addressof(nested.get());
  EXPECT_THROW((container.resolve<root_service &>()), std::runtime_error);
  runtime_proxy_service_should_throw = false;

  EXPECT_EQ(std::addressof(child.get()), child_container);
  EXPECT_EQ(std::addressof(nested.get()), nested_container);
  EXPECT_EQ(container.resolve<root_service &>().value, 31);
}

TEST(type_registration_test,
     registration_static_runtime_and_root_collections_compose) {
  dingo::container<> container;
  container.register_type<scope<shared>,
                          storage<runtime_composed_collection_dependency>>(
      callable([] { return runtime_composed_collection_dependency{11}; }));
  auto child = container.register_type<
      scope<shared>, storage<runtime_composed_collection_service>,
      dependencies<std::vector<runtime_composed_collection_dependency *>>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_composed_collection_dependency>,
          factory<function<
              make_runtime_composed_static_collection_dependency>>>>>();

  child->register_type<scope<shared>,
                       storage<runtime_composed_collection_dependency>>(
      callable([] { return runtime_composed_collection_dependency{33}; }));

  auto &service = container.resolve<runtime_composed_collection_service &>();
  EXPECT_EQ(service.values, (std::vector<int>{11, 33, 22}));
}

TEST(type_registration_test,
     shared_runtime_domain_rolls_back_nested_registration) {
  runtime_domain_retry_factory::should_throw = true;

  dingo::container<> container;
  container.register_type<scope<shared>, storage<runtime_domain_retry_service>>(
      runtime_domain_retry_factory{});

  EXPECT_THROW((container.resolve<runtime_domain_retry_service &>()),
               std::runtime_error);

  runtime_domain_retry_factory::should_throw = false;
  EXPECT_NO_THROW((container.resolve<runtime_domain_retry_service &>()));
}

TEST(type_registration_test,
     registration_container_scopes_preserve_array_lookup_backend) {
  dingo::container<runtime_domain_array_traits> container;
  auto first = container.register_type<
      scope<shared>, storage<runtime_domain_array_service>,
      dependencies<
          dependency<runtime_domain_dependency &, key_type<std::size_t, 0>>>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_composed_static_dependency>>>>();
  auto second = container.register_type<
      scope<shared>, storage<runtime_domain_second_array_service>,
      dependencies<
          dependency<runtime_domain_dependency &, key_type<std::size_t, 0>>>>();

  first->register_type<scope<shared>, storage<runtime_domain_dependency>>(
      callable([] { return runtime_domain_dependency{11}; }),
      dingo::key_value{std::size_t(0)});
  second->register_type<scope<shared>, storage<runtime_domain_dependency>>(
      callable([] { return runtime_domain_dependency{22}; }),
      dingo::key_value{std::size_t(0)});

  EXPECT_EQ(container.resolve<runtime_domain_array_service &>().value.value,
            11);
  EXPECT_EQ(
      container.resolve<runtime_domain_second_array_service &>().value.value,
      22);
}

TEST(type_registration_test,
     registration_container_collections_use_local_entries_or_parent_fallback) {
  dingo::container<runtime_domain_collection_traits> container;
  container.register_type<scope<shared>,
                          storage<runtime_domain_associative_dependency>>(
      callable([] { return runtime_domain_associative_dependency{11}; }),
      dingo::key_value{std::size_t(0)});
  container
      .register_type<scope<shared>, storage<runtime_domain_typed_dependency>,
                     key_type<runtime_domain_typed_key>>(
          callable([] { return runtime_domain_typed_dependency{33}; }));
  auto child = container.register_type<
      scope<shared>, storage<runtime_domain_collection_service>,
      dependencies<
          dependency<std::vector<runtime_domain_associative_dependency *>,
                     key_type<std::size_t, 0>>,
          dependency<std::vector<runtime_domain_typed_dependency *>,
                     key_type<runtime_domain_typed_key>>>>();

  child->register_type<scope<shared>,
                       storage<runtime_domain_associative_dependency>>(
      callable([] { return runtime_domain_associative_dependency{22}; }),
      dingo::key_value{std::size_t(0)});

  auto &service = container.resolve<runtime_domain_collection_service &>();
  ASSERT_EQ(service.associative_dependencies.size(), 1);
  EXPECT_EQ(service.associative_dependencies.front()->value, 22);
  ASSERT_EQ(service.typed_dependencies.size(), 1);
  EXPECT_EQ(service.typed_dependencies.front()->value, 33);
}
