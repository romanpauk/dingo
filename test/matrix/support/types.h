//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/index/map.h>
#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <tuple>
#include <vector>

namespace dingo::matrix {

class config {
  public:
    int retries() const { return retries_; }

  private:
    int retries_ = 3;
};

struct service_interface {
    virtual ~service_interface() = default;
    virtual int retries() const = 0;
};

struct service : service_interface {
    explicit service(config& cfg) : cfg_(cfg) {}

    int retries() const override { return cfg_.retries(); }

  private:
    config& cfg_;
};

struct processor_interface {
    virtual ~processor_interface() = default;
    virtual int id() const = 0;
};

template <int Id> struct processor : processor_interface {
    int id() const override { return Id; }
};

struct unique_interface {
    virtual ~unique_interface() = default;
    virtual int value() const = 0;
};

struct unique_service : unique_interface {
    int value() const override { return 7; }
};

struct consumer {
    explicit consumer(config& cfg) : value(cfg.retries()) {}

    int value;
};

struct first_key : std::integral_constant<int, 0> {};
struct second_key : std::integral_constant<int, 1> {};

struct indexed_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<int, dingo::index_type::map>>;
};

} // namespace dingo::matrix
