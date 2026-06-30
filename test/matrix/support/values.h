//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/keyed.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::matrix {

class value_type {
public:
  value_type() { ++constructor_count_; }
  value_type(const value_type &other)
      : marker_(other.marker_), name_(other.name_) {
    ++copy_constructor_count_;
  }
  value_type(value_type &&other) noexcept
      : marker_(other.marker_), name_(std::move(other.name_)) {
    ++move_constructor_count_;
  }
  ~value_type() { ++destructor_count_; }

  int marker() const { return marker_; }
  bool valid() const { return marker_ == 3 && name_ == "value_type"; }

  static std::size_t constructor_count() { return constructor_count_; }
  static std::size_t copy_constructor_count() {
    return copy_constructor_count_;
  }
  static std::size_t move_constructor_count() {
    return move_constructor_count_;
  }
  static std::size_t destructor_count() { return destructor_count_; }
  static std::size_t total_instances() {
    return constructor_count_ + copy_constructor_count_ +
           move_constructor_count_;
  }
  static void clear_stats() {
    constructor_count_ = copy_constructor_count_ = move_constructor_count_ =
        destructor_count_ = 0;
  }

private:
  int marker_ = 3;
  std::string name_ = "value_type";

  inline static std::size_t constructor_count_ = 0;
  inline static std::size_t copy_constructor_count_ = 0;
  inline static std::size_t move_constructor_count_ = 0;
  inline static std::size_t destructor_count_ = 0;
};

inline bool is_constructed_value(const value_type &value) {
  return value.valid();
}

inline std::unique_ptr<value_type[]> make_unique_value_array() {
  return std::make_unique<value_type[]>(2);
}

inline std::shared_ptr<value_type[]> make_shared_value_array() {
  return std::shared_ptr<value_type[]>(new value_type[2]);
}

struct interface_type {
  virtual ~interface_type() = default;
  virtual int marker() const = 0;
  virtual bool valid() const = 0;
};

class copyable_interface_type {
public:
  copyable_interface_type() = default;
  explicit copyable_interface_type(const value_type &value)
      : marker_(value.marker()), valid_(value.valid()) {}

  int marker() const { return marker_; }
  bool valid() const { return marker_ == 3 && valid_; }

private:
  int marker_ = 0;
  bool valid_ = false;
};

struct second_interface_type {
  virtual ~second_interface_type() = default;
  virtual int second_marker() const = 0;
  virtual bool valid() const = 0;
};

struct implementation_type : interface_type,
                             copyable_interface_type,
                             second_interface_type {
  explicit implementation_type(value_type &dependency)
      : copyable_interface_type(dependency), dependency_(dependency) {}

  int marker() const override { return dependency_.marker(); }
  bool valid() const override { return dependency_.valid(); }
  int second_marker() const override { return dependency_.marker() + 1; }

private:
  value_type &dependency_;
};

struct element_interface {
  virtual ~element_interface() = default;
  virtual int id() const = 0;
};

template <int Id> struct element_type : element_interface {
  int id() const override { return Id; }
};

struct unique_interface_type {
  virtual ~unique_interface_type() = default;
  virtual int value() const = 0;
};

inline bool is_constructed_value(const interface_type &value) {
  return value.marker() == 3 && value.valid();
}

inline bool is_constructed_value(const copyable_interface_type &value) {
  return value.valid();
}

inline bool is_constructed_value(const unique_interface_type &value) {
  return value.value() == 7;
}

struct unique_implementation_type : unique_interface_type {
  int value() const override { return 7; }
};

struct custom_interface_implementation_type : interface_type {
  int marker() const override { return 3; }
  bool valid() const override { return true; }
};

struct custom_unique_implementation_type : unique_interface_type {
  int value() const override { return 7; }
};

struct key_a;
struct key_b;

struct dependent_type {
  explicit dependent_type(value_type &dependency)
      : value(dependency.marker()) {}

  int value;
};

struct overloaded_invoker {
  int operator()(value_type &dependency) const {
    return dependency.marker() + 42;
  }

  int operator()(int value) const { return value + 100; }
};

inline int free_function_invoke(value_type &dependency) {
  return dependency.marker() + 54;
}

inline int noexcept_function_invoke(value_type &dependency) noexcept {
  return dependency.marker() + 60;
}

struct member_function_invoker {
  static int invoke(value_type &dependency) { return dependency.marker() + 66; }
};

struct lvalue_ref_qualified_invoker {
  int operator()(value_type &dependency) & { return dependency.marker() + 72; }
};

struct rvalue_ref_qualified_invoker {
  int operator()(value_type &dependency) && { return dependency.marker() + 78; }
};

struct noexcept_invoker {
  int operator()(value_type &dependency) const noexcept {
    return dependency.marker() + 84;
  }
};

struct move_only_invoker {
  move_only_invoker() : offset(std::make_unique<int>(90)) {}
  move_only_invoker(const move_only_invoker &) = delete;
  move_only_invoker &operator=(const move_only_invoker &) = delete;
  move_only_invoker(move_only_invoker &&) noexcept = default;
  move_only_invoker &operator=(move_only_invoker &&) noexcept = default;

  int operator()(value_type &dependency) const {
    return dependency.marker() + *offset;
  }

  std::unique_ptr<int> offset;
};

struct keyed_value_dependency_type {
  explicit keyed_value_dependency_type(
      dingo::request<value_type &, dingo::key<key_a>> dependency)
      : value(static_cast<value_type &>(dependency).marker()) {}

  int value;
};

struct keyed_collection_dependency_type {
  explicit keyed_collection_dependency_type(
      dingo::request<std::vector<std::shared_ptr<element_interface>>,
                     dingo::key<key_a>>
          elements)
      : count(0), sum(0) {
    auto values =
        static_cast<std::vector<std::shared_ptr<element_interface>>>(elements);
    count = values.size();
    for (const auto &element : values) {
      sum += element->id();
    }
  }

  std::size_t count;
  int sum;
};

struct cycle_b_type;

struct cycle_a_type {
  explicit cycle_a_type(cycle_b_type &init_dependency)
      : dependency(&init_dependency) {}

  cycle_b_type *dependency;
};

struct cycle_b_type {
  explicit cycle_b_type(cycle_a_type &init_dependency)
      : dependency(&init_dependency) {}

  cycle_a_type *dependency;
};

struct key_a : std::integral_constant<int, 0> {};
struct key_b : std::integral_constant<int, 1> {};
struct tag_a {};
struct tag_b {};

} // namespace dingo::matrix
