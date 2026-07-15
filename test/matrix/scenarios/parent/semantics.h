//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/scenarios/parent/common.h"

#include <dingo/type/type_name.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace dingo::matrix {

struct parent_null_element {
  virtual ~parent_null_element() = default;
};

struct parent_null_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<parent_null_element>>;
};

inline void assert_null_runtime_parent() {
  using parent_type = dingo::runtime_container<parent_null_traits>;
  dingo::runtime_container<parent_null_traits,
                           parent_null_traits::allocator_type, parent_type>
      child(nullptr);

  ASSERT_THROW(child.template resolve<parent_null_element &>(),
               dingo::type_not_found_exception);
  ASSERT_TRUE(
      child.template resolve<std::vector<parent_null_element *>>().empty());
}

struct parent_cached_service {
  virtual ~parent_cached_service() = default;
  virtual int id() const = 0;
};

template <int Id> struct parent_cached_service_impl : parent_cached_service {
  int id() const override { return Id; }
};

struct parent_keyed_cache_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, parent_cached_service>>;
};

template <typename ParentShape, typename ChildShape>
void assert_parent_keyed_cache_shadowing() {
  using parent_type =
      typename ParentShape::template type<parent_keyed_cache_traits>;
  using child_type =
      typename ChildShape::template type<parent_keyed_cache_traits,
                                         parent_type>;

  parent_type parent;
  child_type child(&parent);
  parent.template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<parent_cached_service_impl<1>>,
                                dingo::interfaces<parent_cached_service>>(
      dingo::key_value{std::size_t{7}});

  auto &from_parent =
      child.template resolve<parent_cached_service &>(std::size_t{7});
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<parent_cached_service_impl<2>>,
                               dingo::interfaces<parent_cached_service>>(
      dingo::key_value{std::size_t{7}});
  auto &from_child =
      child.template resolve<parent_cached_service &>(std::size_t{7});

  ASSERT_EQ(from_parent.id(), 1);
  ASSERT_EQ(from_child.id(), 2);
  ASSERT_NE(&from_child, &from_parent);
  ASSERT_EQ(&parent.template resolve<parent_cached_service &>(std::size_t{7}),
            &from_parent);
}

struct parent_diagnostic_dependency {};

struct parent_diagnostic_consumer {
  explicit parent_diagnostic_consumer(
      dingo::dependency<parent_diagnostic_dependency &,
                        dingo::key_type<std::size_t, 7>>) {}
};

struct parent_diagnostic_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, parent_diagnostic_dependency>>;
};

template <typename RootShape, typename ParentShape, typename ChildShape>
void assert_parent_keyed_diagnostics() {
  using root_type = typename RootShape::template type<parent_diagnostic_traits>;
  using parent_type =
      typename ParentShape::template type<parent_diagnostic_traits, root_type>;
  using child_type =
      typename ChildShape::template type<parent_diagnostic_traits, parent_type>;

  root_type root;
  parent_type parent(&root);
  child_type child(&parent);
  child.template register_type<dingo::scope<dingo::unique>,
                               dingo::storage<parent_diagnostic_consumer>>();

  try {
    (void)child.template resolve<parent_diagnostic_consumer>();
    FAIL();
  } catch (const dingo::type_not_found_exception &error) {
    const std::string message = error.what();
    ASSERT_NE(message.find(dingo::type_name<parent_diagnostic_dependency>()),
              std::string::npos);
    ASSERT_NE(message.find(dingo::type_name<dingo::key_type<std::size_t, 7>>()),
              std::string::npos);
    ASSERT_NE(message.find(dingo::type_name<parent_diagnostic_consumer>()),
              std::string::npos);
  }
}

struct parent_mutation_trigger {};
struct parent_keyed_mutation_service {};

struct parent_keyed_mutation_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, parent_keyed_mutation_service>>;
};

template <typename ParentShape, typename ChildShape>
void assert_parent_keyed_registration_transaction() {
  using parent_type =
      typename ParentShape::template type<parent_keyed_mutation_traits>;
  using child_type =
      typename ChildShape::template type<parent_keyed_mutation_traits,
                                         parent_type>;

  parent_type parent;
  child_type child(&parent);
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<parent_mutation_trigger>>(
      dingo::callable([&parent, &child]() -> parent_mutation_trigger {
        parent.template register_type<
            dingo::scope<dingo::shared>,
            dingo::storage<parent_keyed_mutation_service>>(
            dingo::key_value{std::size_t{7}});
        (void)child.template resolve<parent_keyed_mutation_service &>(
            std::size_t{7});
        throw std::runtime_error("child construction failed");
      }));

  ASSERT_THROW(child.template resolve<parent_mutation_trigger &>(),
               std::runtime_error);
  ASSERT_EQ(
      &child.template resolve<parent_keyed_mutation_service &>(std::size_t{7}),
      &parent.template resolve<parent_keyed_mutation_service &>(
          std::size_t{7}));
}

struct parent_mutation_processor {
  virtual ~parent_mutation_processor() = default;
};

struct parent_mutation_processor_impl : parent_mutation_processor {};

struct parent_mutation_collection_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<parent_mutation_processor>>;
};

template <typename ParentShape, typename ChildShape>
void assert_parent_collection_registration_transaction() {
  using parent_type =
      typename ParentShape::template type<parent_mutation_collection_traits>;
  using child_type =
      typename ChildShape::template type<parent_mutation_collection_traits,
                                         parent_type>;

  parent_type parent;
  child_type child(&parent);
  child.template register_type<dingo::scope<dingo::shared>,
                               dingo::storage<parent_mutation_trigger>>(
      dingo::callable([&parent]() -> parent_mutation_trigger {
        parent.template register_type<
            dingo::scope<dingo::shared>,
            dingo::storage<parent_mutation_processor_impl>,
            dingo::interfaces<parent_mutation_processor>>();
        throw std::runtime_error("child construction failed");
      }));

  ASSERT_THROW(child.template resolve<parent_mutation_trigger &>(),
               std::runtime_error);
  auto values =
      child.template resolve<std::vector<parent_mutation_processor *>>();
  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values[0], &parent.template resolve<parent_mutation_processor &>());
}

inline void exercise_parent_semantics() {
  assert_null_runtime_parent();
  assert_parent_keyed_cache_shadowing<nested_runtime_parent_shape,
                                      nested_runtime_child_shape>();
  assert_parent_keyed_cache_shadowing<nested_runtime_parent_shape,
                                      nested_container_child_shape>();
  assert_parent_keyed_cache_shadowing<nested_container_parent_shape,
                                      nested_runtime_child_shape>();
  assert_parent_keyed_cache_shadowing<nested_container_parent_shape,
                                      nested_container_child_shape>();

  assert_parent_keyed_diagnostics<nested_runtime_parent_shape,
                                  nested_runtime_child_shape,
                                  nested_runtime_child_shape>();
  assert_parent_keyed_diagnostics<nested_runtime_parent_shape,
                                  nested_runtime_child_shape,
                                  nested_container_child_shape>();
  assert_parent_keyed_diagnostics<nested_runtime_parent_shape,
                                  nested_container_child_shape,
                                  nested_runtime_child_shape>();
  assert_parent_keyed_diagnostics<nested_runtime_parent_shape,
                                  nested_container_child_shape,
                                  nested_container_child_shape>();
  assert_parent_keyed_diagnostics<nested_container_parent_shape,
                                  nested_runtime_child_shape,
                                  nested_runtime_child_shape>();
  assert_parent_keyed_diagnostics<nested_container_parent_shape,
                                  nested_runtime_child_shape,
                                  nested_container_child_shape>();
  assert_parent_keyed_diagnostics<nested_container_parent_shape,
                                  nested_container_child_shape,
                                  nested_runtime_child_shape>();
  assert_parent_keyed_diagnostics<nested_container_parent_shape,
                                  nested_container_child_shape,
                                  nested_container_child_shape>();
}

inline void exercise_parent_registration_transactions() {
  assert_parent_keyed_registration_transaction<nested_runtime_parent_shape,
                                               nested_runtime_child_shape>();
  assert_parent_keyed_registration_transaction<nested_runtime_parent_shape,
                                               nested_container_child_shape>();
  assert_parent_keyed_registration_transaction<nested_container_parent_shape,
                                               nested_runtime_child_shape>();
  assert_parent_keyed_registration_transaction<nested_container_parent_shape,
                                               nested_container_child_shape>();
  assert_parent_collection_registration_transaction<
      nested_runtime_parent_shape, nested_runtime_child_shape>();
  assert_parent_collection_registration_transaction<
      nested_runtime_parent_shape, nested_container_child_shape>();
  assert_parent_collection_registration_transaction<
      nested_container_parent_shape, nested_runtime_child_shape>();
  assert_parent_collection_registration_transaction<
      nested_container_parent_shape, nested_container_child_shape>();
}

} // namespace dingo::matrix
