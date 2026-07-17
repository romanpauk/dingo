//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace dingo {
namespace {

struct introspection_dependency {};

struct introspection_service {
  explicit introspection_service(introspection_dependency &) {}
};

using introspection_static_source =
    bindings<bind<scope<shared>, storage<introspection_dependency>>,
             bind<scope<unique>, storage<introspection_service>,
                  dependencies<introspection_dependency &>>>;

struct introspection_static_traits : static_container_traits<> {
  static constexpr bool dependency_observation_enabled = true;
};

struct introspection_runtime_traits : dynamic_container_traits {
  template <typename> using rebind_t = introspection_runtime_traits;
  static constexpr bool dependency_observation_enabled = true;
};

TEST(introspection_test, static_container_visits_declared_registrations) {
  static_container<introspection_static_source> container;

  std::vector<registration_view> registrations;
  container.visit_registrations(
      [&](const registration_view &view) { registrations.push_back(view); });

  ASSERT_EQ(registrations.size(), 2u);
  EXPECT_EQ(registrations[0].origin, registration_origin::static_binding);
  EXPECT_EQ(registrations[0].registered_type,
            describe_type<introspection_dependency>());
  EXPECT_EQ(registrations[0].interfaces.size(), 1u);
  EXPECT_TRUE(registrations[0].dependencies_known);
  EXPECT_EQ(registrations[0].dependencies.size(), 0u);
  EXPECT_EQ(registrations[1].registered_type,
            describe_type<introspection_service>());
  EXPECT_EQ(registrations[1].dependencies.size(), 1u);
}

struct introspection_lookup_traits : dynamic_container_traits {
  template <typename> using rebind_t = introspection_lookup_traits;
  using lookup_definition_type =
      lookups<associative<std::size_t, introspection_service, one, array<4>>>;
};

TEST(introspection_test, runtime_container_exposes_runtime_lookup_keys) {
  container<introspection_lookup_traits> container;
  container.register_type<scope<shared>, storage<introspection_service>,
                          dependencies<introspection_dependency &>>(
      key_value{std::size_t{2}});
  container.register_type<scope<shared>, storage<introspection_dependency>>();

  bool found_key = false;
  container.visit_registrations([&](const registration_view &view) {
    if (view.interface_type == describe_type<introspection_service>()) {
      const auto *key = view.key.get_if<std::size_t>();
      if (key != nullptr && *key == 2) {
        found_key = true;
        EXPECT_EQ(view.origin, registration_origin::runtime_binding);
        EXPECT_EQ(view.materialization,
                  registration_materialization::not_materialized);
        EXPECT_TRUE(view.dependencies_known);
        EXPECT_EQ(view.dependencies.size(), 1u);
      }
    }
  });

  EXPECT_TRUE(found_key);
}

TEST(introspection_test,
     runtime_observer_reports_constructed_type_dependencies) {
  container<introspection_runtime_traits> container;
  container.register_type<scope<shared>, storage<introspection_dependency>>();
  container.register_type<scope<shared>, storage<introspection_service>,
                          dependencies<introspection_dependency &>>();

  registration_id service_registration;
  container.visit_registrations([&](const registration_view &registration) {
    if (registration.registered_type ==
        describe_type<introspection_service>()) {
      service_registration = registration.id;
    }
  });

  std::vector<dependency_view> dependencies;
  dependencies.reserve(1);
  auto observer = [&](const dependency_view &dependency) noexcept {
    dependencies.push_back(dependency);
  };
  auto subscription = container.observe_dependencies(observer);

  (void)container.resolve<introspection_service &>();
  (void)container.resolve<introspection_service &>();

  ASSERT_EQ(dependencies.size(), 1u);
  EXPECT_EQ(dependencies[0].registration, service_registration);
  EXPECT_EQ(dependencies[0].container_id, 0u);
  EXPECT_EQ(dependencies[0].constructed_type,
            describe_type<introspection_service>());
  EXPECT_EQ(dependencies[0].dependency_type,
            describe_type<introspection_dependency &>());
}

TEST(introspection_test,
     static_observer_reports_constructed_type_dependencies) {
  static_container<introspection_static_source, introspection_static_traits>
      container;

  std::vector<dependency_view> dependencies;
  dependencies.reserve(1);
  auto observer = [&](const dependency_view &dependency) noexcept {
    dependencies.push_back(dependency);
  };
  auto subscription = container.observe_dependencies(observer);

  (void)container.resolve<introspection_service>();

  ASSERT_EQ(dependencies.size(), 1u);
  EXPECT_NE(dependencies[0].registration.value, nullptr);
  EXPECT_EQ(dependencies[0].constructed_type,
            describe_type<introspection_service>());
  EXPECT_EQ(dependencies[0].dependency_type,
            describe_type<introspection_dependency &>());
}

struct introspection_automatic_type {
  explicit introspection_automatic_type(introspection_dependency &) {}
};

struct introspection_local_leaf {};

struct introspection_local_dependency {
  explicit introspection_local_dependency(introspection_local_leaf &) {}
};

struct introspection_local_service {
  explicit introspection_local_service(introspection_local_dependency &) {}
};

TEST(introspection_test, observer_reports_automatic_constructor_dependencies) {
  container<introspection_runtime_traits> container;
  container.register_type<scope<shared>, storage<introspection_dependency>>();

  std::vector<dependency_view> dependencies;
  dependencies.reserve(1);
  auto observer = [&](const dependency_view &dependency) noexcept {
    dependencies.push_back(dependency);
  };
  auto subscription = container.observe_dependencies(observer);

  (void)container.construct<introspection_automatic_type>();

  ASSERT_EQ(dependencies.size(), 1u);
  EXPECT_EQ(dependencies[0].constructed_type,
            describe_type<introspection_automatic_type>());
  EXPECT_EQ(dependencies[0].dependency_type,
            describe_type<introspection_dependency &>());
  EXPECT_EQ(dependencies[0].registration.value, nullptr);
}

TEST(introspection_test,
     root_observer_reports_registration_child_dependencies) {
  container<introspection_runtime_traits> container;
  container.register_type<
      scope<shared>, storage<introspection_local_service>,
      dependencies<introspection_local_dependency &>,
      bindings<bind<scope<shared>, storage<introspection_local_dependency>,
                    dependencies<introspection_local_leaf &>>,
               bind<scope<shared>, storage<introspection_local_leaf>>>>();

  std::vector<dependency_view> dependencies;
  dependencies.reserve(2);
  auto observer = [&](const dependency_view &dependency) noexcept {
    dependencies.push_back(dependency);
  };
  auto subscription = container.observe_dependencies(observer);

  (void)container.resolve<introspection_local_service &>();

  bool found_child_dependency = false;
  for (const auto &dependency : dependencies) {
    if (dependency.constructed_type ==
            describe_type<introspection_local_dependency>() &&
        dependency.dependency_type ==
            describe_type<introspection_local_leaf &>() &&
        dependency.container_id != 0) {
      found_child_dependency = true;
    }
  }
  EXPECT_TRUE(found_child_dependency);
}

struct opaque_lookup_storage {};

TEST(introspection_test, opaque_custom_lookup_reports_incomplete_visit) {
  opaque_lookup_storage storage;
  auto visitor = [](const auto &, const auto &) {};

  EXPECT_FALSE(detail::visit_lookup_storage(storage, visitor));
}

} // namespace
} // namespace dingo
