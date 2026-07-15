//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/runtime_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dingo::matrix {

struct nested_value_type {
  explicit nested_value_type(int dependency) : value(dependency) {}

  int value;
};

struct nested_parent_value_type {
  explicit nested_parent_value_type(int dependency) : value(dependency) {}

  int value;
};

struct nested_runtime_parent_shape {
  template <typename Traits> using type = dingo::runtime_container<Traits>;
};

struct nested_container_parent_shape {
  template <typename Traits> using type = dingo::container<Traits>;
};

struct nested_runtime_child_shape {
  template <typename Traits, typename Parent>
  using type =
      dingo::runtime_container<Traits, typename Traits::allocator_type, Parent>;
};

struct nested_container_child_shape {
  template <typename Traits, typename Parent>
  using type =
      dingo::container<Traits, typename Traits::allocator_type, Parent>;
};

struct nested_processor {
  virtual ~nested_processor() = default;
  virtual int id() const = 0;
};

template <int Id> struct nested_processor_impl : nested_processor {
  int id() const override { return Id; }
};

struct nested_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<nested_processor>>;
};

struct nested_owned_dependency {
  explicit nested_owned_dependency(int initial_value) : value(initial_value) {}

  int value;
};

struct nested_owned_service {
  explicit nested_owned_service(
      std::shared_ptr<nested_owned_dependency> resolved_dependency)
      : dependency(std::move(resolved_dependency)) {}

  std::shared_ptr<nested_owned_dependency> dependency;
};

struct nested_parent_owned_service {
  nested_parent_owned_service() {
    ++constructions;
    ++instances;
  }
  ~nested_parent_owned_service() { --instances; }

  inline static int constructions = 0;
  inline static int instances = 0;
};

struct nested_failing_child_service {
  explicit nested_failing_child_service(
      std::shared_ptr<nested_parent_owned_service>) {
    throw std::runtime_error("child construction failed");
  }
};

struct nested_decorated_value {
  explicit nested_decorated_value(int initial_value) : value(initial_value) {}

  int value;
};

struct nested_transient {};
struct nested_external {};

struct nested_local_dependency {
  explicit nested_local_dependency(int initial_value) : value(initial_value) {}

  int value;
};

struct nested_inherited_dependency {
  explicit nested_inherited_dependency(int initial_value)
      : value(initial_value) {}

  int value;
};

struct nested_pack {
  std::shared_ptr<nested_local_dependency> local;
  std::shared_ptr<nested_inherited_dependency> inherited;
};

struct nested_packed_service {
  explicit nested_packed_service(nested_pack dependency_pack)
      : dependencies(std::move(dependency_pack)) {}

  nested_pack dependencies;
};

struct nested_transaction_parent {
  nested_transaction_parent() { ++constructions; }

  inline static int constructions = 0;
};

struct nested_transaction_child {
  nested_transaction_child() { ++constructions; }
  ~nested_transaction_child() { ++destructions; }

  inline static int constructions = 0;
  inline static int destructions = 0;
};

struct nested_transaction_failure {};
struct nested_transaction_trigger {};
struct nested_child_registration {};
struct nested_parent_registration {};

struct nested_throwing_dependency {};

struct nested_failing_pack {
  std::shared_ptr<nested_transaction_parent> parent;
  std::shared_ptr<nested_throwing_dependency> failure;
};

struct nested_failed_service {};

} // namespace dingo::matrix
