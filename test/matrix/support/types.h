//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/index/map.h>
#include <dingo/index/unordered_map.h>
#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace dingo::matrix {

class value_type {
  public:
    int marker() const { return marker_; }

  private:
    int marker_ = 3;
};

struct interface_type {
    virtual ~interface_type() = default;
    virtual int marker() const = 0;
};

struct implementation_type : interface_type {
    explicit implementation_type(value_type& dependency)
        : dependency_(dependency) {}

    int marker() const override { return dependency_.marker(); }

  private:
    value_type& dependency_;
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

struct unique_implementation_type : unique_interface_type {
    int value() const override { return 7; }
};

struct dependent_type {
    explicit dependent_type(value_type& dependency)
        : value(dependency.marker()) {}

    int value;
};

struct key_a : std::integral_constant<int, 0> {};
struct key_b : std::integral_constant<int, 1> {};

struct indexed_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<int, dingo::index_type::map>>;
};

struct indexed_unordered_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<int, dingo::index_type::unordered_map>>;
};

} // namespace dingo::matrix
