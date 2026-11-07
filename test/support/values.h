//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <string>
#include <utility>

namespace dingo::scenarios {

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

struct interface_type {
  virtual ~interface_type() = default;
  virtual int marker() const = 0;
  virtual bool valid() const = 0;
};

struct element_interface {
  virtual ~element_interface() = default;
  virtual int id() const = 0;
};

template <int Id> struct element_type : element_interface {
  int id() const override { return Id; }
};

inline bool is_constructed_value(const interface_type &value) {
  return value.marker() == 3 && value.valid();
}

} // namespace dingo::scenarios
