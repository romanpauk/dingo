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
void assert_parent_transactions() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  {
    nested_transaction_parent::constructions = 0;
    nested_transaction_child::constructions = 0;
    nested_transaction_child::destructions = 0;
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<dingo::scope<dingo::shared>,
                                  dingo::storage<nested_transaction_parent>>();
    child.template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<nested_transaction_failure>>(
        dingo::callable([&child]() -> nested_transaction_failure {
          (void)child.template resolve<nested_transaction_parent &>();
          child.template register_type<
              dingo::scope<dingo::shared>,
              dingo::storage<nested_transaction_child>>();
          (void)child.template resolve<nested_transaction_child &>();
          throw std::runtime_error("child construction failed");
        }));

    ASSERT_THROW(child.template resolve<nested_transaction_failure &>(),
                 std::runtime_error);
    ASSERT_EQ(nested_transaction_parent::constructions, 1);
    ASSERT_EQ(nested_transaction_child::constructions, 1);
    ASSERT_EQ(nested_transaction_child::destructions, 1);
    ASSERT_THROW(child.template resolve<nested_transaction_child &>(),
                 dingo::type_not_found_exception);
    ASSERT_EQ(&child.template resolve<nested_transaction_parent &>(),
              &parent.template resolve<nested_transaction_parent &>());
  }

  {
    nested_transaction_parent::constructions = 0;
    nested_transaction_child::constructions = 0;
    nested_transaction_child::destructions = 0;
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_transaction_parent>>>();
    child.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_transaction_child>>>();
    child.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_throwing_dependency>>>(
        dingo::callable([]() -> std::shared_ptr<nested_throwing_dependency> {
          throw std::runtime_error("construction failed");
        }));
    child.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_failed_service>>>(
        dingo::callable([&child] {
          (void)child
              .template resolve<std::shared_ptr<nested_transaction_parent>>();
          (void)child
              .template resolve<std::shared_ptr<nested_transaction_child>>();
          (void)child.template resolve<nested_failing_pack>();
          return std::make_shared<nested_failed_service>();
        }));

    ASSERT_THROW(
        child.template resolve<std::shared_ptr<nested_failed_service>>(),
        std::runtime_error);
    ASSERT_EQ(nested_transaction_parent::constructions, 1);
    ASSERT_EQ(nested_transaction_child::constructions, 1);
    ASSERT_EQ(nested_transaction_child::destructions, 1);

    (void)child.template resolve<std::shared_ptr<nested_transaction_child>>();
    (void)parent.template resolve<std::shared_ptr<nested_transaction_parent>>();
    ASSERT_EQ(nested_transaction_parent::constructions, 1);
    ASSERT_EQ(nested_transaction_child::constructions, 2);
  }

  {
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<dingo::scope<dingo::shared>,
                                  dingo::storage<nested_transaction_trigger>>(
        dingo::callable([&child]() -> nested_transaction_trigger {
          child.template register_type<
              dingo::scope<dingo::shared>,
              dingo::storage<nested_child_registration>>();
          throw std::runtime_error("parent construction failed");
        }));

    ASSERT_THROW(child.template resolve<nested_transaction_trigger &>(),
                 std::runtime_error);
    ASSERT_THROW(child.template resolve<nested_child_registration &>(),
                 dingo::type_not_found_exception);
  }

  {
    parent_type parent;
    child_type child(&parent);
    child.template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<nested_transaction_trigger>>(
        dingo::callable([&parent]() -> nested_transaction_trigger {
          parent.template register_type<
              dingo::scope<dingo::shared>,
              dingo::storage<nested_parent_registration>>();
          throw std::runtime_error("child construction failed");
        }));

    ASSERT_THROW(child.template resolve<nested_transaction_trigger &>(),
                 std::runtime_error);
    ASSERT_EQ(&child.template resolve<nested_parent_registration &>(),
              &parent.template resolve<nested_parent_registration &>());
  }

  {
    parent_type parent;
    child_type child(&parent);
    parent.template register_type<dingo::scope<dingo::shared>,
                                  dingo::storage<nested_transaction_trigger>>(
        dingo::callable([&child]() -> nested_transaction_trigger {
          child.template register_type<
              dingo::scope<dingo::shared>,
              dingo::storage<nested_child_registration>>();
          throw std::runtime_error("parent construction failed");
        }));

    ASSERT_THROW(parent.template resolve<nested_transaction_trigger &>(),
                 std::runtime_error);
    (void)child.template resolve<nested_child_registration &>();
  }
}

template <typename ParentShape, typename ChildShape>
void assert_parent_transaction_regressions() {
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

    {
      child_type child(&parent);
      child.template register_type<
          dingo::scope<dingo::shared>,
          dingo::storage<std::shared_ptr<nested_failing_child_service>>>();

      ASSERT_THROW(child.template resolve<
                       std::shared_ptr<nested_failing_child_service>>(),
                   std::runtime_error);
      ASSERT_EQ(nested_parent_owned_service::constructions, 1);
      ASSERT_EQ(nested_parent_owned_service::instances, 1);
    }

    ASSERT_EQ(nested_parent_owned_service::instances, 1);
    ASSERT_EQ(nested_parent_owned_service::constructions, 1);
    ASSERT_NE(
        parent.template resolve<std::shared_ptr<nested_parent_owned_service>>(),
        nullptr);
    ASSERT_EQ(nested_parent_owned_service::constructions, 1);
  }
  ASSERT_EQ(nested_parent_owned_service::instances, 0);

  nested_parent_owned_service::constructions = 0;
  nested_parent_owned_service::instances = 0;
  int attempts = 0;
  {
    parent_type parent;
    parent.template register_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<nested_parent_owned_service>>>(
        dingo::callable(
            [&attempts]() -> std::shared_ptr<nested_parent_owned_service> {
              if (++attempts == 1) {
                throw std::runtime_error("parent construction failed");
              }
              return std::make_shared<nested_parent_owned_service>();
            }));
    child_type child(&parent);

    ASSERT_THROW(
        child.template resolve<std::shared_ptr<nested_parent_owned_service>>(),
        std::runtime_error);
    ASSERT_EQ(nested_parent_owned_service::constructions, 0);
    ASSERT_EQ(nested_parent_owned_service::instances, 0);

    ASSERT_NE(
        child.template resolve<std::shared_ptr<nested_parent_owned_service>>(),
        nullptr);
    ASSERT_EQ(attempts, 2);
    ASSERT_EQ(nested_parent_owned_service::constructions, 1);
    ASSERT_EQ(nested_parent_owned_service::instances, 1);
  }
  ASSERT_EQ(nested_parent_owned_service::instances, 0);
}

template <typename = void> void exercise_parent_transaction_pairs() {
  assert_parent_transactions<nested_runtime_parent_shape,
                             nested_runtime_child_shape>();
  assert_parent_transactions<nested_runtime_parent_shape,
                             nested_container_child_shape>();
  assert_parent_transactions<nested_container_parent_shape,
                             nested_runtime_child_shape>();
  assert_parent_transactions<nested_container_parent_shape,
                             nested_container_child_shape>();
  assert_parent_transaction_regressions<nested_runtime_parent_shape,
                                        nested_runtime_child_shape>();
  assert_parent_transaction_regressions<nested_container_parent_shape,
                                        nested_container_child_shape>();
}

} // namespace dingo::matrix
