//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once
#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {
namespace {
struct shared_ptr_variant_a {
    explicit shared_ptr_variant_a(int init) : value(init) {}
    int value;
};

struct shared_ptr_variant_b {
    explicit shared_ptr_variant_b(float init) : value(init) {}
    float value;
};

using shared_ptr_variant =
    std::variant<shared_ptr_variant_a, shared_ptr_variant_b>;
} // namespace

template <typename T> struct external_test : public test<T> {};
template <typename T> struct external_unique_test : public test<T> {};
template <typename T> struct external_variant_test : public test<T> {};
template <typename T> struct external_misc_test : public test<T> {};

} // namespace dingo
