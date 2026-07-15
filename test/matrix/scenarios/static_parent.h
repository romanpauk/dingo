//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <vector>

namespace dingo::matrix {

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

struct second_child_config : parent_config {
  second_child_config() { value = 3; }
};

struct container_parent_shape {
  template <typename Bindings> using type = dingo::container<Bindings>;
};

struct static_parent_shape {
  template <typename Bindings> using type = dingo::static_container<Bindings>;
};

struct container_child_shape {
  static constexpr bool reports_ambiguity_at_runtime = true;

  template <typename Bindings, typename Parent>
  using type = dingo::container<Bindings, Parent>;
};

struct static_child_shape {
  static constexpr bool reports_ambiguity_at_runtime = false;

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
  child_type child(&parent);

  ASSERT_EQ(parent.template resolve<parent_config &>().value, 1);
  ASSERT_EQ(child.template resolve<parent_config &>().value, 2);
  ASSERT_EQ(child.template resolve<child_service>().value, 2);
}

template <typename ParentShape, typename ChildShape>
void assert_static_parent_child_ambiguity() {
  using parent_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_config>>>;
  using child_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<child_config>,
                  dingo::interfaces<parent_config>>,
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<second_child_config>,
                  dingo::interfaces<parent_config>>>;
  using parent_type = typename ParentShape::template type<parent_bindings>;
  using child_type =
      typename ChildShape::template type<child_bindings, parent_type>;

  parent_type parent;
  child_type child(&parent);

  ASSERT_THROW(child.template resolve<parent_config &>(),
               dingo::type_ambiguous_exception);
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
  child_type child(&parent);

  auto values = child.template resolve<std::vector<parent_config *>>();

  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values[0]->value, 2);
}

template <typename ParentShape, typename ChildShape>
void exercise_static_parent_container_pair() {
  assert_static_parent_dependency_fallback<ParentShape, ChildShape>();
  assert_static_parent_child_override<ParentShape, ChildShape>();
  if constexpr (ChildShape::reports_ambiguity_at_runtime) {
    assert_static_parent_child_ambiguity<ParentShape, ChildShape>();
  }
  assert_static_parent_collection_fallback<ParentShape, ChildShape>();
  assert_static_parent_collection_override<ParentShape, ChildShape>();
}

inline void assert_static_binding_child_runtime_parent() {
  dingo::runtime_container<> parent;
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<parent_config>>();

  using fallback_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<child_service>>>;
  dingo::container<fallback_bindings, decltype(parent)> fallback_child(&parent);
  ASSERT_EQ(fallback_child.template resolve<parent_config &>().value, 1);
  ASSERT_EQ(fallback_child.template resolve<child_service>().value, 1);
  auto inherited =
      fallback_child.template resolve<std::vector<parent_config *>>();
  ASSERT_EQ(inherited.size(), 1u);
  ASSERT_EQ(inherited[0]->value, 1);

  using override_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<child_config>,
                  dingo::interfaces<parent_config>>,
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<child_service>>>;
  dingo::container<override_bindings, decltype(parent)> override_child(&parent);
  ASSERT_EQ(override_child.template resolve<parent_config &>().value, 2);
  ASSERT_EQ(override_child.template resolve<child_service>().value, 2);
  auto local = override_child.template resolve<std::vector<parent_config *>>();
  ASSERT_EQ(local.size(), 1u);
  ASSERT_EQ(local[0]->value, 2);
}

inline void assert_runtime_child_static_binding_parent() {
  using parent_bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_config>>>;
  dingo::container<parent_bindings> parent;
  using child_type =
      dingo::runtime_container<dingo::dynamic_container_traits,
                               dingo::dynamic_container_traits::allocator_type,
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

inline void exercise_mixed_runtime_static_parent_pairs() {
  assert_static_binding_child_runtime_parent();
  assert_runtime_child_static_binding_parent();
}

} // namespace dingo::matrix
