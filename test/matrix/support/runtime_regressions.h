//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/runtime_containers.h"
#include "matrix/support/values.h"

#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace dingo::matrix {

struct unique_hierarchy_s {};
struct unique_hierarchy_u {};
struct unique_hierarchy_b {
    explicit unique_hierarchy_b(std::shared_ptr<unique_hierarchy_s>&&) {}
};

struct nested_wrapper_value : interface_type {
    int marker() const override { return 3; }
    bool valid() const override { return true; }
};

struct nested_wrapper_consumer {
    nested_wrapper_consumer(interface_type& init_value,
                            interface_type* init_pointer)
        : value(init_value), pointer(init_pointer) {}

    interface_type& value;
    interface_type* pointer;
};

struct rollback_exception {};

struct rollback_a {
    rollback_a() { ++constructor_count; }
    ~rollback_a() { ++destructor_count; }

    inline static std::size_t constructor_count = 0;
    inline static std::size_t destructor_count = 0;
};

struct rollback_b {
    rollback_b() { ++constructor_count; }
    ~rollback_b() { ++destructor_count; }

    inline static std::size_t constructor_count = 0;
    inline static std::size_t destructor_count = 0;
};

struct rollback_c {
    rollback_c(rollback_a&, rollback_b&) { throw rollback_exception(); }
};

inline void clear_rollback_stats() {
    rollback_a::constructor_count = rollback_a::destructor_count = 0;
    rollback_b::constructor_count = rollback_b::destructor_count = 0;
}

struct unique_reference_value {
    unique_reference_value() = default;
    std::shared_ptr<int> token = std::make_shared<int>(1);
};

struct shared_from_unique_reference {
    explicit shared_from_unique_reference(unique_reference_value& unique)
        : token(unique.token) {}

    std::weak_ptr<int> token;
};

struct unique_reference_exception_value {
    explicit unique_reference_exception_value(int& init_destructor_count)
        : destructor_count(init_destructor_count) {}
    unique_reference_exception_value(
        const unique_reference_exception_value& other)
        : destructor_count(other.destructor_count) {}
    unique_reference_exception_value(unique_reference_exception_value&& other)
        : destructor_count(other.destructor_count) {}
    ~unique_reference_exception_value() { ++destructor_count; }

    int& destructor_count;
};

struct shared_unique_reference_exception_value {
    explicit shared_unique_reference_exception_value(
        unique_reference_exception_value&) {
        throw std::runtime_error("shared unique reference failure");
    }
};

namespace construct_dependency {
struct a {};
struct b {};

template <typename... Deps> class dependencies {
  public:
    dependencies() = delete;
    explicit dependencies(Deps... deps) : deps_(std::forward<Deps>(deps)...) {}

    template <typename D> auto dependency() const { return std::get<D>(deps_); }

  private:
    std::tuple<Deps...> deps_;
};

template <typename... Deps> class auto_dependencies {
  public:
    auto_dependencies() = delete;
    explicit auto_dependencies(Deps... deps)
        : deps_(std::forward<Deps>(deps)...) {}

    template <typename D> auto dependency() const { return std::get<D>(deps_); }

  private:
    std::tuple<Deps...> deps_;
};

struct foo : dependencies<std::shared_ptr<a>, std::shared_ptr<b>> {
    using dependencies_type =
        dependencies<std::shared_ptr<a>, std::shared_ptr<b>>;
    using dependencies_type::dependency;

    explicit foo(dependencies_type deps) : dependencies_type(std::move(deps)) {}
};

struct opted_in_foo
    : auto_dependencies<std::shared_ptr<a>, std::shared_ptr<b>> {
    using dependencies_type =
        auto_dependencies<std::shared_ptr<a>, std::shared_ptr<b>>;
    using dependencies_type::dependency;

    explicit opted_in_foo(dependencies_type deps)
        : dependencies_type(std::move(deps)) {}
};

struct ambiguous_auto_dependencies {
    explicit ambiguous_auto_dependencies(std::shared_ptr<a>) {}
    explicit ambiguous_auto_dependencies(std::shared_ptr<b>) {}
};

struct ambiguous_opted_in_foo {
    using dependencies_type = ambiguous_auto_dependencies;

    explicit ambiguous_opted_in_foo(dependencies_type) {}
};

template <typename T> struct holder {
    T value;
};

template <typename... Deps> struct aggregate_dependencies : holder<Deps>... {
    template <typename D> auto dependency() const {
        static_assert((std::is_same_v<D, Deps> || ...));
        return static_cast<const holder<D>&>(*this).value;
    }
};

struct aggregate_foo
    : aggregate_dependencies<std::shared_ptr<a>, std::shared_ptr<b>> {
    using dependencies_type =
        aggregate_dependencies<std::shared_ptr<a>, std::shared_ptr<b>>;
    using dependencies_type::dependency;

    explicit aggregate_foo(dependencies_type deps)
        : dependencies_type(std::move(deps)) {}
};
} // namespace construct_dependency

struct cyclical_rollback_exception {};

struct cyclical_rollback_b;

struct cyclical_rollback_a : value_type {
    explicit cyclical_rollback_a(
        std::shared_ptr<interface_type>& init_dependency)
        : dependency(init_dependency) {}

    std::shared_ptr<interface_type>& dependency;
};

struct cyclical_rollback_b : interface_type {
    explicit cyclical_rollback_b(cyclical_rollback_a&) {
        if (fail) {
            fail = false;
            throw cyclical_rollback_exception();
        }
    }

    int marker() const override { return 3; }
    bool valid() const override { return true; }

    inline static bool fail = true;
};

struct cyclical_commit_c {
    explicit cyclical_commit_c(std::shared_ptr<interface_type>&) {
        throw cyclical_rollback_exception();
    }
};

} // namespace dingo::matrix

namespace dingo {

template <typename... Deps>
struct is_auto_constructible<
    matrix::construct_dependency::auto_dependencies<Deps...>> : std::true_type {
};

template <>
struct is_auto_constructible<
    matrix::construct_dependency::ambiguous_auto_dependencies>
    : std::true_type {};

} // namespace dingo
