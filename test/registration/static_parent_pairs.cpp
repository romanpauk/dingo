//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <gtest/gtest.h>

#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <vector>

namespace dingo::scenarios {

struct parent_config {
  parent_config() : value(1) {}

  int value;
};

struct child_service {
  explicit child_service(parent_config &config) : value(config.value) {}

  int value;
};

struct child_config : parent_config {
  child_config() { value = 2; }
};

// A parent made of Bindings: a static container binds them in its type, and
// a runtime container registers them once it is made.
struct static_parent_shape {
  template <typename Bindings> using type = dingo::static_container<Bindings>;

  template <typename Bindings, typename Parent> static void prepare(Parent &) {}
};

struct runtime_parent_shape {
  template <typename Bindings> using type = dingo::runtime_container<>;

  template <typename Bindings, typename Parent>
  static void prepare(Parent &parent) {
    add(parent, Bindings{});
  }

private:
  template <typename Parent, typename... Registrations>
  static void add(Parent &parent, dingo::bindings<Registrations...>) {
    (add(parent, static_cast<Registrations *>(nullptr)), ...);
  }

  template <typename Parent, typename... Args>
  static void add(Parent &parent, dingo::bind<Args...> *) {
    parent.template register_type<Args...>();
  }
};

struct static_child_shape {
  template <typename Bindings, typename Parent>
  using type = dingo::static_container<Bindings, Parent>;
};

template <typename ParentShape, typename ChildShape>
void assert_static_parent_dependency_fallback() {
  using parent_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_config>>>;
  using child_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<child_service>>>;
  using parent_type = typename ParentShape::template type<parent_bindings>;
  using child_type =
      typename ChildShape::template type<child_bindings, parent_type>;

  parent_type parent;
  ParentShape::template prepare<parent_bindings>(parent);
  child_type child(&parent);

  ASSERT_EQ(child.template resolve<parent_config &>().value, 1);
  ASSERT_EQ(child.template resolve<child_service>().value, 1);
}

template <typename ParentShape, typename ChildShape>
void assert_static_parent_child_override() {
  using parent_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_config>>>;
  using child_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<child_config>,
                  dingo::interfaces<parent_config>>,
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<child_service>>>;
  using parent_type = typename ParentShape::template type<parent_bindings>;
  using child_type =
      typename ChildShape::template type<child_bindings, parent_type>;

  parent_type parent;
  ParentShape::template prepare<parent_bindings>(parent);
  child_type child(&parent);

  ASSERT_EQ(parent.template resolve<parent_config &>().value, 1);
  ASSERT_EQ(child.template resolve<parent_config &>().value, 2);
  ASSERT_EQ(child.template resolve<child_service>().value, 2);
}

template <typename ParentShape, typename ChildShape>
void assert_static_parent_collection_fallback() {
  using parent_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_config>>>;
  using child_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<child_service>>>;
  using parent_type = typename ParentShape::template type<parent_bindings>;
  using child_type =
      typename ChildShape::template type<child_bindings, parent_type>;

  parent_type parent;
  ParentShape::template prepare<parent_bindings>(parent);
  child_type child(&parent);

  auto values = child.template resolve<std::vector<parent_config *>>();

  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values[0]->value, 1);
}

template <typename ParentShape, typename ChildShape>
void assert_static_parent_collection_override() {
  using parent_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_config>>>;
  using child_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<child_config>,
                  dingo::interfaces<parent_config>>>;
  using parent_type = typename ParentShape::template type<parent_bindings>;
  using child_type =
      typename ChildShape::template type<child_bindings, parent_type>;

  parent_type parent;
  ParentShape::template prepare<parent_bindings>(parent);
  child_type child(&parent);

  auto values = child.template resolve<std::vector<parent_config *>>();

  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values[0]->value, 2);
}

template <typename ParentShape, typename ChildShape>
void exercise_static_parent_container_pair() {
  assert_static_parent_dependency_fallback<ParentShape, ChildShape>();
  assert_static_parent_child_override<ParentShape, ChildShape>();
  assert_static_parent_collection_fallback<ParentShape, ChildShape>();
  assert_static_parent_collection_override<ParentShape, ChildShape>();
}

inline void exercise_runtime_child_static_parent() {
  using parent_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_config>>>;
  dingo::static_container<parent_bindings> parent;
  using child_type =
      dingo::runtime_container<dingo::runtime_container_traits,
                               dingo::runtime_container_traits::allocator_type,
                               decltype(parent)>;

  child_type fallback_child(&parent);
  fallback_child.template register_type<dingo::scope<dingo::unique>,
                                        dingo::storage<child_service>>();
  ASSERT_EQ(fallback_child.template resolve<parent_config &>().value, 1);
  ASSERT_EQ(fallback_child.template resolve<child_service>().value, 1);
  auto inherited =
      fallback_child.template resolve<std::vector<parent_config *>>();
  ASSERT_EQ(inherited.size(), 1u);
  ASSERT_EQ(inherited[0]->value, 1);

  child_type override_child(&parent);
  override_child.template register_type<dingo::scope<dingo::shared>,
                                        dingo::storage<child_config>,
                                        dingo::interfaces<parent_config>>();
  override_child.template register_type<dingo::scope<dingo::unique>,
                                        dingo::storage<child_service>>();
  ASSERT_EQ(override_child.template resolve<parent_config &>().value, 2);
  ASSERT_EQ(override_child.template resolve<child_service>().value, 2);
  auto local = override_child.template resolve<std::vector<parent_config *>>();
  ASSERT_EQ(local.size(), 1u);
  ASSERT_EQ(local[0]->value, 2);
}

} // namespace dingo::scenarios

namespace dingo::scenarios {

TEST(static_parent_container, runtime_child) {
  exercise_runtime_child_static_parent();
}

TEST(static_parent_container, static_child) {
  exercise_static_parent_container_pair<static_parent_shape,
                                        static_child_shape>();
}

TEST(runtime_parent_container, static_child) {
  exercise_static_parent_container_pair<runtime_parent_shape,
                                        static_child_shape>();
}

} // namespace dingo::scenarios
