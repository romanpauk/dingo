//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/index/map.h>
#include <dingo/index/unordered_map.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_map.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
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

struct second_interface_type {
    virtual ~second_interface_type() = default;
    virtual int second_marker() const = 0;
};

struct implementation_type : interface_type, second_interface_type {
    explicit implementation_type(value_type& dependency)
        : dependency_(dependency) {}

    int marker() const override { return dependency_.marker(); }
    int second_marker() const override { return dependency_.marker() + 1; }

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

struct cycle_b_type;

struct cycle_a_type {
    explicit cycle_a_type(cycle_b_type& init_dependency)
        : dependency(&init_dependency) {}

    cycle_b_type* dependency;
};

struct cycle_b_type {
    explicit cycle_b_type(cycle_a_type& init_dependency)
        : dependency(&init_dependency) {}

    cycle_a_type* dependency;
};

struct local_dependency_type {
    local_dependency_type() : value(4) {}

    int value;
};

struct local_value_type {
    local_value_type(local_dependency_type& local, value_type& host)
        : value(local.value + host.marker() + 5) {}

    int value;
};

struct nested_value_type {
    explicit nested_value_type(int dependency) : value(dependency) {}

    int value;
};

struct factory_value_type {
  private:
    explicit factory_value_type(int init) : value(init) {}

  public:
    static factory_value_type create() { return factory_value_type(9); }

    int value;
};

struct variant_a {
    explicit variant_a(value_type& dependency) : value(dependency.marker()) {}

    int value;
};

struct variant_b {
    explicit variant_b(int init) : value(init) {}

    int value;
};

struct key_a : std::integral_constant<int, 0> {};
struct key_b : std::integral_constant<int, 1> {};
struct tag_a {};
struct tag_b {};

struct indexed_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<std::size_t, dingo::index_type::map>>;
};

struct indexed_unordered_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<std::size_t, dingo::index_type::unordered_map>>;
};

struct indexed_array_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<std::size_t, dingo::index_type::array<8>>>;
};

template <typename T> class test_allocator_stats {
  protected:
    static std::size_t allocated_;

  public:
    static std::size_t allocated() { return allocated_; }
};

template <typename T> std::size_t test_allocator_stats<T>::allocated_ = 0;

template <typename T> class test_allocator : public test_allocator_stats<void> {
  public:
    using value_type = T;

    test_allocator() noexcept = default;
    template <typename U> test_allocator(const test_allocator<U>&) noexcept {}

    value_type* allocate(std::size_t n) {
        this->allocated_ += n * sizeof(value_type);
        return static_cast<value_type*>(::operator new(n * sizeof(value_type)));
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        this->allocated_ -= n * sizeof(value_type);
        ::operator delete(p);
    }
};

template <typename T, typename U>
bool operator==(const test_allocator<T>&, const test_allocator<U>&) noexcept {
    return true;
}

template <typename T, typename U>
bool operator!=(const test_allocator<T>& lhs,
                const test_allocator<U>& rhs) noexcept {
    return !(lhs == rhs);
}

struct custom_rtti_container_traits {
    template <typename>
    using rebind_t = custom_rtti_container_traits;

    using tag_type = void;
    using rtti_type = dingo::rtti<dingo::static_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = dingo::dynamic_type_map<Value, rtti_type, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type =
        dingo::dynamic_type_cache<Value, rtti_type, Allocator>;
    using allocator_type = std::allocator<char>;
    using index_definition_type = std::tuple<>;
    static constexpr bool cache_enabled = true;
};

} // namespace dingo::matrix
