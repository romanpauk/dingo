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
#include <dingo/type/type_name.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "support/custom_wrappers.h"

namespace dingo::matrix {

class value_type {
  public:
    value_type() { ++constructor_count_; }
    value_type(const value_type& other)
        : marker_(other.marker_), name_(other.name_) {
        ++copy_constructor_count_;
    }
    value_type(value_type&& other) noexcept
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

inline bool is_constructed_value(const value_type& value) {
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
    explicit copyable_interface_type(const value_type& value)
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

struct implementation_type : interface_type, copyable_interface_type,
                             second_interface_type {
    explicit implementation_type(value_type& dependency)
        : copyable_interface_type(dependency), dependency_(dependency) {}

    int marker() const override { return dependency_.marker(); }
    bool valid() const override { return dependency_.valid(); }
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

inline bool is_constructed_value(const interface_type& value) {
    return value.marker() == 3 && value.valid();
}

inline bool is_constructed_value(const copyable_interface_type& value) {
    return value.valid();
}

inline bool is_constructed_value(const unique_interface_type& value) {
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
    explicit dependent_type(value_type& dependency)
        : value(dependency.marker()) {}

    int value;
};

struct keyed_value_dependency_type {
    explicit keyed_value_dependency_type(
        dingo::keyed<value_type&, key_a> dependency)
        : value(static_cast<value_type&>(dependency).marker()) {}

    int value;
};

struct keyed_collection_dependency_type {
    explicit keyed_collection_dependency_type(
        dingo::keyed<std::vector<std::shared_ptr<element_interface>>, key_a>
            elements)
        : count(0), sum(0) {
        auto values =
            static_cast<std::vector<std::shared_ptr<element_interface>>>(
                elements);
        count = values.size();
        for (const auto& element : values) {
            sum += element->id();
        }
    }

    std::size_t count;
    int sum;
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

struct local_override_value_type {
    explicit local_override_value_type(local_dependency_type& dependency)
        : value(dependency.value) {}

    int value;
};

struct local_collection_value_type {
    explicit local_collection_value_type(
        std::vector<std::shared_ptr<element_interface>> elements)
        : count(elements.size()), sum(0) {
        for (const auto& element : elements) {
            sum += element->id();
        }
    }

    std::size_t count;
    int sum;
};

struct nested_value_type {
    explicit nested_value_type(int dependency) : value(dependency) {}

    int value;
};

struct factory_function_value_type {
  private:
    explicit factory_function_value_type(int init) : value(init) {}

  public:
    static factory_function_value_type create() {
        return factory_function_value_type(9);
    }

    int value;
};

struct factory_constructor_value_type {
    explicit factory_constructor_value_type(value_type& dependency)
        : value(dependency.marker() + 6) {}

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

struct nested_variant_a {
    int value = 3;
};

struct nested_variant_b {
    int value = 7;
};

using nested_variant_type = std::variant<nested_variant_a, nested_variant_b>;
using shared_unique_value_type = std::shared_ptr<std::unique_ptr<value_type>>;
using shared_unique_array_value_type =
    std::shared_ptr<std::unique_ptr<value_type[]>>;
using unique_shared_value_type = std::unique_ptr<std::shared_ptr<value_type>>;
using variant_unique_value_type =
    std::variant<std::unique_ptr<value_type>, nested_variant_b>;
using variant_shared_value_type =
    std::variant<std::shared_ptr<value_type>, nested_variant_b>;
using variant_shared_unique_value_type =
    std::variant<std::shared_ptr<std::unique_ptr<value_type>>, nested_variant_b>;
using unique_variant_value_type = std::unique_ptr<nested_variant_type>;
using shared_variant_value_type = std::shared_ptr<nested_variant_type>;
using array_variant_value_type =
    std::variant<std::array<value_type, 2>, nested_variant_b>;

inline shared_unique_value_type make_shared_unique_value() {
    return std::make_shared<std::unique_ptr<value_type>>(
        std::make_unique<value_type>());
}

inline shared_unique_array_value_type make_shared_unique_array_value() {
    return std::make_shared<std::unique_ptr<value_type[]>>(
        std::make_unique<value_type[]>(2));
}

inline unique_shared_value_type make_unique_shared_value() {
    return std::make_unique<std::shared_ptr<value_type>>(
        std::make_shared<value_type>());
}

inline variant_unique_value_type make_variant_unique_value() {
    return variant_unique_value_type(std::in_place_type<std::unique_ptr<value_type>>,
                                     std::make_unique<value_type>());
}

inline variant_shared_value_type make_variant_shared_value() {
    return variant_shared_value_type(std::in_place_type<std::shared_ptr<value_type>>,
                                     std::make_shared<value_type>());
}

inline variant_shared_unique_value_type make_variant_shared_unique_value() {
    return variant_shared_unique_value_type(
        std::in_place_type<std::shared_ptr<std::unique_ptr<value_type>>>,
        std::make_shared<std::unique_ptr<value_type>>(
            std::make_unique<value_type>()));
}

inline unique_variant_value_type make_unique_variant_value() {
    return std::make_unique<nested_variant_type>(
        std::in_place_type<nested_variant_a>);
}

inline shared_variant_value_type make_shared_variant_value() {
    return std::make_shared<nested_variant_type>(
        std::in_place_type<nested_variant_a>);
}

inline array_variant_value_type make_array_variant_value() {
    return array_variant_value_type(std::in_place_type<std::array<value_type, 2>>);
}

struct key_a : std::integral_constant<int, 0> {};
struct key_b : std::integral_constant<int, 1> {};
struct tag_a {};
struct tag_b {};

struct indexed_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<std::size_t, dingo::index_type::map>>;
};

struct indexed_int_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<int, dingo::index_type::map>>;
};

struct indexed_string_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<std::string, dingo::index_type::map>>;
};

struct indexed_unordered_container_traits : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<std::size_t, dingo::index_type::unordered_map>>;
};

struct indexed_int_unordered_container_traits
    : dingo::dynamic_container_traits {
    using index_definition_type =
        std::tuple<std::tuple<int, dingo::index_type::unordered_map>>;
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

struct scenario_type {};

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

template <typename Container> struct indexed_key {
    using type = std::tuple_element_t<
        0, std::tuple_element_t<0, typename std::remove_reference_t<
                                       Container>::index_definition_type>>;
};

template <typename Container>
using indexed_key_t = typename indexed_key<Container>::type;

template <typename T> T indexed_value(int value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return std::to_string(value);
    } else {
        return static_cast<T>(value);
    }
}

template <typename Container>
void exercise_indexed_registration(Container& container) {
    using key_type = indexed_key_t<Container>;

    container.template register_indexed_type<
        dingo::scope<dingo::unique>,
        dingo::storage<std::unique_ptr<element_type<0>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(0));
    container.template register_indexed_type<
        dingo::scope<dingo::unique>,
        dingo::storage<std::unique_ptr<element_type<1>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(1));
    ASSERT_THROW(
        (container.template register_indexed_type<
            dingo::scope<dingo::unique>,
            dingo::storage<std::unique_ptr<element_type<1>>>,
            dingo::interfaces<element_interface>>(indexed_value<key_type>(1))),
        type_already_registered_exception);

    ASSERT_EQ(container
                  .template resolve<std::shared_ptr<element_interface>>(
                      indexed_value<key_type>(0))
                  ->id(),
              0);
    ASSERT_EQ(container
                  .template resolve<std::shared_ptr<element_interface>>(
                      indexed_value<key_type>(1))
                  ->id(),
              1);
    ASSERT_THROW(
        (container.template resolve<std::shared_ptr<element_interface>>(
            indexed_value<key_type>(-1))),
        type_not_found_exception);

    container.template register_indexed_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<element_type<2>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(2));
    container.template register_indexed_type<
        dingo::scope<dingo::shared>,
        dingo::storage<std::shared_ptr<element_type<3>>>,
        dingo::interfaces<element_interface>>(indexed_value<key_type>(3));
    ASSERT_THROW(
        (container.template register_indexed_type<
            dingo::scope<dingo::shared>,
            dingo::storage<std::shared_ptr<element_type<3>>>,
            dingo::interfaces<element_interface>>(indexed_value<key_type>(3))),
        type_already_registered_exception);

    ASSERT_EQ(
        container
            .template resolve<element_interface&>(indexed_value<key_type>(2))
            .id(),
        2);
    ASSERT_EQ(
        container
            .template resolve<element_interface&>(indexed_value<key_type>(3))
            .id(),
        3);
    ASSERT_THROW((container.template resolve<element_interface&>(
                     indexed_value<key_type>(-2))),
                 type_not_found_exception);
}

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
