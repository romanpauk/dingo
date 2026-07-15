//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

namespace dingo::matrix {

template <typename ParentShape, typename ChildShape>
void assert_parent_ownership() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  {
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_owned_service>>>();
    child.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_owned_dependency>>>(
        dingo::callable(
            [] { return std::make_shared<nested_owned_dependency>(2); }));

    ASSERT_THROW(
        child.template resolve<std::shared_ptr<nested_owned_service>>(),
        dingo::type_not_found_exception);
  }

  {
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_owned_dependency>>>(
        dingo::callable(
            [] { return std::make_shared<nested_owned_dependency>(1); }));
    parent.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_owned_service>>>();
    child.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_owned_dependency>>>(
        dingo::callable(
            [] { return std::make_shared<nested_owned_dependency>(2); }));

    auto service =
        child.template resolve<std::shared_ptr<nested_owned_service>>();
    ASSERT_EQ(
        service->dependency,
        parent.template resolve<std::shared_ptr<nested_owned_dependency>>());
    ASSERT_EQ(service->dependency->value, 1);
    ASSERT_EQ(
        child.template resolve<std::shared_ptr<nested_owned_dependency>>()
            ->value,
        2);
  }

  {
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<std::unique_ptr<nested_transient>>>();

    auto first = child.template resolve<std::unique_ptr<nested_transient>>();
    auto second = child.template resolve<std::unique_ptr<nested_transient>>();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(first.get(), second.get());
  }

  {
    nested_external instance;
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<dingo::scope<dingo::external>,
                                  dingo::storage<nested_external &>>(instance);

    ASSERT_EQ(&child.template resolve<nested_external &>(), &instance);
    ASSERT_EQ(&parent.template resolve<nested_external &>(), &instance);
  }

  {
    parent_type parent;
    parent.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_processor_impl<1>>>,
        dingo::interfaces<nested_processor>>();
    std::weak_ptr<nested_processor> retained;

    {
      child_type child(&parent);
      auto values = child.template resolve<
          std::vector<std::shared_ptr<nested_processor>>>();
      ASSERT_EQ(values.size(), 1u);
      retained = values[0];
    }

    ASSERT_FALSE(retained.expired());
    auto values =
        parent
            .template resolve<std::vector<std::shared_ptr<nested_processor>>>();
    ASSERT_EQ(values.size(), 1u);
    ASSERT_EQ(values[0], retained.lock());
  }
}

template <typename ParentShape, typename ChildShape>
void assert_parent_lifetime_regression() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  nested_parent_owned_service::constructions = 0;
  nested_parent_owned_service::instances = 0;
  {
    parent_type parent;
    parent.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_parent_owned_service>>>();

    std::weak_ptr<nested_parent_owned_service> instance;
    {
      child_type child(&parent);
      instance =
          child
              .template resolve<std::shared_ptr<nested_parent_owned_service>>();
      ASSERT_FALSE(instance.expired());
    }

    ASSERT_FALSE(instance.expired());
    ASSERT_EQ(nested_parent_owned_service::constructions, 1);
    ASSERT_EQ(nested_parent_owned_service::instances, 1);
    ASSERT_EQ(
        parent.template resolve<std::shared_ptr<nested_parent_owned_service>>(),
        instance.lock());
  }
  ASSERT_EQ(nested_parent_owned_service::instances, 0);
}

template <typename = void> void exercise_parent_ownership_pairs() {
  assert_parent_ownership<nested_runtime_parent_shape,
                          nested_runtime_child_shape>();
  assert_parent_ownership<nested_runtime_parent_shape,
                          nested_container_child_shape>();
  assert_parent_ownership<nested_container_parent_shape,
                          nested_runtime_child_shape>();
  assert_parent_ownership<nested_container_parent_shape,
                          nested_container_child_shape>();
  assert_parent_lifetime_regression<nested_runtime_parent_shape,
                                    nested_runtime_child_shape>();
  assert_parent_lifetime_regression<nested_container_parent_shape,
                                    nested_container_child_shape>();
}

} // namespace dingo::matrix
