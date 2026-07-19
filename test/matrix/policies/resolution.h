//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/fixtures/dependency_shapes.h"
#include "matrix/policies/policy_core.h"

#include <dingo/core/dependency.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace dingo::matrix::resolution {

namespace requests {

template <typename Type> struct direct {
  template <typename Container>
  static decltype(auto) resolve(Container &container) {
    return container.template resolve<Type>();
  }
};

} // namespace requests

template <typename Interface>
using interface_ref = requests::direct<Interface &>;

template <typename Interface>
using interface_ptr = requests::direct<Interface *>;

template <typename Interface>
using interface_shared_ptr = requests::direct<std::shared_ptr<Interface>>;

template <typename Request> struct constructed {
  template <typename Container> static void check(Container &container) {
    decltype(auto) instance = Request::resolve(container);
    ASSERT_TRUE(is_constructed_value(instance));
  }
};

template <typename Request> struct pointee_constructed {
  template <typename Container> static void check(Container &container) {
    decltype(auto) instance = Request::resolve(container);
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

template <typename Type> struct value {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<Type>();
    ASSERT_TRUE(is_constructed_value(instance));
  }
};

template <typename Type> struct ref_ptr {
  template <typename Container> static void check(Container &container) {
    Type &instance = container.template resolve<Type &>();
    Type *pointer = container.template resolve<Type *>();
    ASSERT_TRUE(is_constructed_value(instance));
    ASSERT_TRUE(is_constructed_value(*pointer));
    ASSERT_EQ(&instance, pointer);
  }
};

template <typename Type> struct const_ref {
  template <typename Container> static void check(Container &container) {
    const Type &instance = container.template resolve<const Type &>();
    ASSERT_TRUE(is_constructed_value(instance));
  }
};

template <typename Type> struct const_ptr {
  template <typename Container> static void check(Container &container) {
    const Type *instance = container.template resolve<const Type *>();
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

template <typename Interface> struct interface_ref_ptr_shared_ptr {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Interface &>();
    auto *pointer = container.template resolve<Interface *>();
    auto &first_handle =
        container.template resolve<std::shared_ptr<Interface> &>();
    auto &second_handle =
        container.template resolve<std::shared_ptr<Interface> &>();

    ASSERT_TRUE(is_constructed_value(instance));
    ASSERT_EQ(std::addressof(instance), pointer);
    ASSERT_EQ(pointer, first_handle.get());
    ASSERT_EQ(std::addressof(first_handle), std::addressof(second_handle));
    ASSERT_EQ(first_handle.get(), second_handle.get());
  }
};

template <typename Implementation, typename FirstInterface,
          typename SecondInterface, int Marker>
struct multiple_interface_identity {
  template <typename Container> static void check(Container &container) {
    auto &first = container.template resolve<FirstInterface &>();
    auto &second = container.template resolve<SecondInterface &>();
    auto *first_implementation = dynamic_cast<Implementation *>(&first);
    auto *second_implementation = dynamic_cast<Implementation *>(&second);

    ASSERT_NE(first_implementation, nullptr);
    ASSERT_EQ(first_implementation, second_implementation);
    ASSERT_EQ(second.second_marker(), Marker);
    ASSERT_TRUE(second.valid());
  }
};

template <typename Implementation, typename FirstInterface,
          typename SecondInterface, int Marker>
struct multiple_interface_shared_identity {
  template <typename Container> static void check(Container &container) {
    multiple_interface_identity<Implementation, FirstInterface, SecondInterface,
                                Marker>::check(container);
    auto &first_handle =
        container.template resolve<std::shared_ptr<FirstInterface> &>();
    auto &second_handle =
        container.template resolve<std::shared_ptr<SecondInterface> &>();
    auto *first_implementation =
        dynamic_cast<Implementation *>(first_handle.get());
    auto *second_implementation =
        dynamic_cast<Implementation *>(second_handle.get());

    ASSERT_NE(first_implementation, nullptr);
    ASSERT_EQ(first_implementation, second_implementation);
    ASSERT_FALSE(first_handle.owner_before(second_handle));
    ASSERT_FALSE(second_handle.owner_before(first_handle));
  }
};

template <typename Type> struct unique_ptr {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<std::unique_ptr<Type> &&>();
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

template <typename Type> struct shared_ptr {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<std::shared_ptr<Type>>();
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

template <typename Type> struct shared_ptr_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<std::shared_ptr<Type> &>();
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

template <typename Type> struct optional {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<std::optional<Type>>();
    ASSERT_TRUE(instance.has_value());
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

template <typename Type> struct optional_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<std::optional<Type> &>();
    ASSERT_TRUE(instance.has_value());
    ASSERT_TRUE(is_constructed_value(*instance));
  }
};

template <typename Cycle> struct cycle_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Cycle &>();
    ASSERT_NE(instance.dependency, nullptr);
    ASSERT_EQ(instance.dependency->dependency, &instance);
  }
};

template <typename Type, int Expected> struct construct_member_value {
  template <typename Container> static void check(Container &container) {
    auto constructed = container.template construct<Type>();
    ASSERT_EQ(constructed.value, Expected);
  }
};

template <typename Type> struct construct_count_sum {
  template <typename Container> static void check(Container &container) {
    auto constructed = container.template construct<Type>();
    ASSERT_EQ(constructed.count, 2u);
    ASSERT_EQ(constructed.sum, 1);
  }
};

template <typename Constructed, typename Dependency, int Expected>
struct construct_invoke {
  template <typename Container> static void check(Container &container) {
    auto constructed = container.template construct<Constructed>();
    ASSERT_EQ(constructed.value, Expected);
    auto invoked = container.invoke(
        [](Dependency &dependency) { return dependency.marker(); });
    ASSERT_EQ(invoked, Expected);
  }
};

template <typename Type> struct dependency_marker_value {
  static int get(const Type &dependency) { return dependency.marker(); }
};

template <typename Type> struct dependency_marker_value<Type *> {
  static int get(Type *dependency) { return dependency->marker(); }
};

template <typename Type> struct dependency_marker_value<std::shared_ptr<Type>> {
  static int get(const std::shared_ptr<Type> &dependency) {
    return dependency->marker();
  }
};

template <typename Type> struct dependency_marker_value<std::unique_ptr<Type>> {
  static int get(const std::unique_ptr<Type> &dependency) {
    return dependency->marker();
  }
};

template <typename Type> struct dependency_marker_value<std::optional<Type>> {
  static int get(const std::optional<Type> &dependency) {
    return dependency->marker();
  }
};

template <typename Type, std::size_t Size>
struct dependency_marker_value<Type[Size]> {
  static_assert(Size > 0);
  static int get(const Type (&dependency)[Size]) {
    return dependency[0].marker();
  }
};

template <> struct dependency_marker_value<variant_a> {
  static int get(const variant_a &dependency) { return dependency.value; }
};

template <> struct dependency_marker_value<variant_b> {
  static int get(const variant_b &dependency) { return dependency.value; }
};

template <typename... Types>
struct dependency_marker_value<std::variant<Types...>> {
  static int get(const std::variant<Types...> &dependency) {
    return std::visit(
        [](const auto &alternative) {
          using alternative_type = std::decay_t<decltype(alternative)>;
          return dependency_marker_value<alternative_type>::get(alternative);
        },
        dependency);
  }
};

template <typename Type> int dependency_marker(Type &&dependency) {
  using dependency_type = std::remove_cv_t<std::remove_reference_t<Type>>;
  return dependency_marker_value<dependency_type>::get(dependency);
}

template <typename Request, typename Value = Request> struct dependency_invoke {
  template <typename Container> static void check(Container &container) {
    auto invoked = container.invoke([](Request dependency) {
      return dependency_marker(
          static_cast<Value>(std::forward<Request>(dependency)));
    });
    ASSERT_EQ(invoked, 3);
  }
};

template <typename Request, typename Value = Request>
struct dependency_invoke_explicit {
  template <typename Container> static void check(Container &container) {
    auto invoked =
        container.template invoke<int(Request)>([](Request dependency) {
          return dependency_marker(
              static_cast<Value>(std::forward<Request>(dependency)));
        });
    ASSERT_EQ(invoked, 3);
  }
};

template <typename Type, typename Key, int Expected> struct keyed_invoke {
  template <typename Container> static void check(Container &container) {
    auto invoked =
        container.invoke([](dingo::dependency<Type &, Key> dependency) {
          return static_cast<Type &>(dependency).marker();
        });
    ASSERT_EQ(invoked, Expected);
  }
};

template <typename Collection, typename Key, std::size_t Expected>
struct keyed_collection_invoke {
  template <typename Container> static void check(Container &container) {
    auto invoked =
        container.invoke([](dingo::dependency<Collection, Key> elements) {
          auto values = static_cast<Collection>(elements);
          return values.size();
        });
    ASSERT_EQ(invoked, Expected);
  }
};

template <typename Type, template <typename> typename Wrapper>
struct custom_shared {
  template <typename Container> static void check(Container &container) {
    auto &wrapper = container.template resolve<Wrapper<Type> &>();
    ASSERT_NE(wrapper.get(), nullptr);
    Type *pointer = container.template resolve<Type *>();
    ASSERT_EQ(pointer, wrapper.get());
    ASSERT_TRUE(is_constructed_value(*pointer));
    auto copy = container.template resolve<Wrapper<Type>>();
    ASSERT_EQ(copy.get(), wrapper.get());
  }
};

template <typename Type, template <typename> typename Unique,
          template <typename> typename Shared>
struct custom_unique {
  template <typename Container> static void check(Container &container) {
    auto unique = container.template resolve<Unique<Type>>();
    ASSERT_NE(unique.get(), nullptr);
    ASSERT_TRUE(is_constructed_value(*unique.get()));
    auto shared = container.template resolve<Shared<Type>>();
    ASSERT_NE(shared.get(), nullptr);
    ASSERT_TRUE(is_constructed_value(*shared.get()));
  }
};

template <typename Type, template <typename> typename Wrapper>
struct custom_optional_ref {
  template <typename Container> static void check(Container &container) {
    auto &wrapper = container.template resolve<Wrapper<Type> &>();
    ASSERT_NE(wrapper.get(), nullptr);
    ASSERT_TRUE(is_constructed_value(*wrapper.get()));
    Type &value = container.template resolve<Type &>();
    ASSERT_EQ(std::addressof(value), wrapper.get());
  }
};

template <typename Type, template <typename> typename Wrapper>
struct custom_optional {
  template <typename Container> static void check(Container &container) {
    auto wrapper = container.template resolve<Wrapper<Type>>();
    ASSERT_NE(wrapper.get(), nullptr);
    ASSERT_TRUE(is_constructed_value(*wrapper.get()));
  }
};

} // namespace dingo::matrix::resolution
