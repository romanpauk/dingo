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

#include <memory>
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

} // namespace dingo::matrix
