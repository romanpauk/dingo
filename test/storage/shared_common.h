//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once
#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_conversion_traits.h>

#include <gtest/gtest.h>

#include <variant>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {
namespace {
struct pointer_conversion_view {
    virtual ~pointer_conversion_view() = default;
};

struct pointer_conversion_owner : pointer_conversion_view {
    static inline int destructors = 0;

    pointer_conversion_owner() : view_(new pointer_conversion_view) {}

    ~pointer_conversion_owner() {
        ++destructors;
        delete view_;
    }

    pointer_conversion_view* view_;
};

struct shared_ptr_variant_a {
    explicit shared_ptr_variant_a(int init) : value(init) {}
    int value;
};

struct shared_ptr_variant_b {
    explicit shared_ptr_variant_b(float init) : value(init) {}
    float value;
};

struct custom_sum_a {
    explicit custom_sum_a(int init) : value(init) {}
    int value;
};

struct custom_sum_b {
    explicit custom_sum_b(float init) : value(init) {}
    float value;
};

using shared_ptr_variant =
    std::variant<shared_ptr_variant_a, shared_ptr_variant_b>;

struct custom_sum {
    std::variant<custom_sum_a, custom_sum_b> value;
};

inline std::shared_ptr<shared_ptr_variant> make_shared_ptr_variant(int value) {
    return std::make_shared<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, value);
}

inline std::unique_ptr<shared_ptr_variant> make_unique_ptr_variant(int value) {
    return std::make_unique<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, value);
}

inline std::shared_ptr<custom_sum> make_shared_custom_sum(int value) {
    return std::make_shared<custom_sum>(
        custom_sum{std::variant<custom_sum_a, custom_sum_b>(
            std::in_place_type<custom_sum_a>, value)});
}
} // namespace

template <> struct alternative_type_traits<custom_sum> {
    static constexpr bool enabled = true;

    using alternatives = type_list<custom_sum_a, custom_sum_b>;

    template <typename Selected, typename Value>
    static custom_sum wrap(Value&& value) {
        return custom_sum{std::variant<custom_sum_a, custom_sum_b>(
            std::in_place_type<Selected>, std::forward<Value>(value))};
    }

    template <typename Selected> static Selected* get(custom_sum& value) {
        return std::get_if<Selected>(&value.value);
    }

    template <typename Selected>
    static const Selected* get(const custom_sum& value) {
        return std::get_if<Selected>(&value.value);
    }
};

template <>
struct type_conversion_traits<pointer_conversion_view*,
                              pointer_conversion_owner*> {
    static pointer_conversion_view* convert(pointer_conversion_owner* source) {
        return source->view_;
    }
};

template <typename T> struct shared_test : public test<T> {};
template <typename T> struct shared_variant_test : public test<T> {};
template <typename T> struct shared_hierarchy_test : public test<T> {};

} // namespace dingo
