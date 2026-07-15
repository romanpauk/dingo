//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <stdexcept>
#include <vector>

namespace dingo::matrix {

struct parent_collection_state {
  parent_collection_state() { ++constructions; }

  inline static int constructions = 0;
};

struct tracked_collection_processor : nested_processor {
  tracked_collection_processor() { ++constructions; }
  ~tracked_collection_processor() override { ++destructions; }

  int id() const override { return 4; }

  inline static int constructions = 0;
  inline static int destructions = 0;
};

template <typename ParentShape, typename ChildShape>
void assert_parent_collection_factory_fallback() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  parent_type parent;
  child_type child(&parent);
  int parent_appends = 0;
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<1>>,
                                dingo::interfaces<nested_processor>>();
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<2>>,
                                dingo::interfaces<nested_processor>>();
  parent.template register_type_collection<
      dingo::scope<dingo::unique>,
      dingo::storage<std::vector<nested_processor *>>>(
      [&parent_appends](auto &values, nested_processor *value) {
        ++parent_appends;
        values.push_back(value);
      });

  auto values = child.template resolve<std::vector<nested_processor *>>();

  ASSERT_EQ(values.size(), 2u);
  ASSERT_EQ(values[0]->id(), 1);
  ASSERT_EQ(values[1]->id(), 2);
  ASSERT_EQ(parent_appends, 2);
}

template <typename ParentShape, typename ChildShape>
void assert_child_collection_factory_override() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  parent_type parent;
  child_type child(&parent);
  int parent_appends = 0;
  int child_appends = 0;
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<1>>,
                                dingo::interfaces<nested_processor>>();
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<nested_processor_impl<2>>,
                               dingo::interfaces<nested_processor>>();
  parent.template register_type_collection<
      dingo::scope<dingo::unique>,
      dingo::storage<std::vector<nested_processor *>>>(
      [&parent_appends](auto &values, nested_processor *value) {
        ++parent_appends;
        values.push_back(value);
      });
  child.template register_type_collection<
      dingo::scope<dingo::unique>,
      dingo::storage<std::vector<nested_processor *>>>(
      [&child_appends](auto &values, nested_processor *value) {
        ++child_appends;
        values.push_back(value);
      });

  auto values = child.template resolve<std::vector<nested_processor *>>();

  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values[0]->id(), 2);
  ASSERT_EQ(parent_appends, 0);
  ASSERT_EQ(child_appends, 1);
}

template <typename ParentShape, typename ChildShape>
void assert_child_collection_entries_override_parent_factory() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  parent_type parent;
  child_type child(&parent);
  int parent_appends = 0;
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<nested_processor_impl<1>>,
                                dingo::interfaces<nested_processor>>();
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<nested_processor_impl<2>>,
                               dingo::interfaces<nested_processor>>();
  parent.template register_type_collection<
      dingo::scope<dingo::unique>,
      dingo::storage<std::vector<nested_processor *>>>(
      [&parent_appends](auto &values, nested_processor *value) {
        ++parent_appends;
        values.push_back(value);
      });

  auto values = child.template resolve<std::vector<nested_processor *>>();

  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values[0]->id(), 2);
  ASSERT_EQ(parent_appends, 0);
}

template <typename ParentShape, typename ChildShape>
void assert_throwing_child_collection_factory_rollback() {
  using parent_type =
      typename ParentShape::template type<nested_container_traits>;
  using child_type =
      typename ChildShape::template type<nested_container_traits, parent_type>;

  tracked_collection_processor::constructions = 0;
  tracked_collection_processor::destructions = 0;
  parent_collection_state::constructions = 0;
  bool should_throw = true;

  parent_type parent;
  child_type child(&parent);
  parent.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<std::shared_ptr<parent_collection_state>>>();
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<tracked_collection_processor>,
                               dingo::interfaces<nested_processor>>();
  child.template register_type_collection<
      dingo::scope<dingo::unique>,
      dingo::storage<std::vector<nested_processor *>>>(
      [&child, &should_throw](auto &values, nested_processor *value) {
        (void)
            child.template resolve<std::shared_ptr<parent_collection_state>>();
        if (should_throw) {
          throw std::runtime_error("collection append failed");
        }
        values.push_back(value);
      });

  ASSERT_THROW(child.template resolve<std::vector<nested_processor *>>(),
               std::runtime_error);
  ASSERT_EQ(tracked_collection_processor::constructions, 1);
  ASSERT_EQ(tracked_collection_processor::destructions, 1);
  ASSERT_EQ(parent_collection_state::constructions, 1);

  should_throw = false;
  auto values = child.template resolve<std::vector<nested_processor *>>();

  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(tracked_collection_processor::constructions, 2);
  ASSERT_EQ(parent_collection_state::constructions, 1);
}

template <typename ParentShape, typename ChildShape>
void assert_parent_collection_pair() {
  assert_parent_collection_factory_fallback<ParentShape, ChildShape>();
  assert_child_collection_factory_override<ParentShape, ChildShape>();
  assert_child_collection_entries_override_parent_factory<ParentShape,
                                                          ChildShape>();
  assert_throwing_child_collection_factory_rollback<ParentShape, ChildShape>();
}

template <typename = void> void exercise_parent_collection_pairs() {
  assert_parent_collection_pair<nested_runtime_parent_shape,
                                nested_runtime_child_shape>();
  assert_parent_collection_pair<nested_runtime_parent_shape,
                                nested_container_child_shape>();
  assert_parent_collection_pair<nested_container_parent_shape,
                                nested_runtime_child_shape>();
  assert_parent_collection_pair<nested_container_parent_shape,
                                nested_container_child_shape>();
}

} // namespace dingo::matrix
