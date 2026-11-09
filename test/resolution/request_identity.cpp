//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What one container answers across different requests of one binding, and
// the requests the model probe in spec/ does not make: cv-qualified ones, a
// second interface at an offset, a concrete interface by value, typed keys,
// multi-dimensional arrays and a variant made by a constructor of one of its
// alternatives.

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

using namespace dingo;

namespace {
// A value whose copy is checked: a moved-from one loses its name.
struct value {
  value() : name("value") {}
  bool intact() const { return name == "value"; }
  int marker() const { return 3; }

  std::string name;
};

struct runtime {
  template <typename... Args, typename Container>
  static void add(Container &container, type_registration<Args...> *) {
    container.template register_type<Args...>();
  }

  template <typename... Binds, typename F> static void with(F &&function) {
    runtime_container<> container;
    (add(container, static_cast<Binds *>(nullptr)), ...);
    function(container);
  }
};

struct fixed {
  template <typename... Binds, typename F> static void with(F &&function) {
    static_container<bindings<Binds...>> container;
    function(container);
  }
};

struct padding {
  virtual ~padding() = default;
  long bytes[4] = {};
};

struct first_interface {
  virtual ~first_interface() = default;
  virtual int first_marker() const = 0;
};

struct second_interface {
  virtual ~second_interface() = default;
  virtual int second_marker() const = 0;
};

// The second interface is at an offset from the implementation: a pointer to
// it is adjusted.
struct implementation : padding, first_interface, second_interface {
  int first_marker() const override { return 1; }
  int second_marker() const override { return 2; }
};

// An interface that is a concrete type, requested by value as a copy of the
// part of the implementation it is.
struct concrete_interface {
  int marker = 5;
};

struct concrete_implementation : concrete_interface {
  concrete_implementation() { marker = 6; }
};

struct variant_a {
  explicit variant_a(value dependency) : marker(dependency.marker()) {}
  int marker;
};

struct variant_b {
  explicit variant_b(int init) : marker(init) {}
  int marker;
};

struct key_a {};

struct keyed_consumer {
  explicit keyed_consumer(dependency<value &, key_type<key_a>> dependency)
      : marker(static_cast<value &>(dependency).marker()) {}
  int marker;
};
} // namespace

template <typename T> struct request_identity_test : testing::Test {};

using container_kinds = testing::Types<runtime, fixed>;
TYPED_TEST_SUITE(request_identity_test, container_kinds, );

// Every reference and pointer request of a kept instance, whatever its cv
// qualification, reaches the one instance, and a copy of it is a copy.
TYPED_TEST(request_identity_test, references_of_every_qualification_agree) {
  TypeParam::template with<dingo::bind<scope<shared>, storage<value>>>(
      [](auto &container) {
        value &instance = container.template resolve<value &>();
        value *pointer = container.template resolve<value *>();
        EXPECT_EQ(&instance, pointer);
        EXPECT_EQ(&container.template resolve<const value &>(), pointer);
        EXPECT_EQ(&container.template resolve<volatile value &>(), pointer);
        EXPECT_EQ(&container.template resolve<const volatile value &>(),
                  pointer);
        EXPECT_EQ(container.template resolve<const value *>(), pointer);
        EXPECT_EQ(container.template resolve<volatile value *>(), pointer);
        EXPECT_EQ(container.template resolve<const volatile value *>(),
                  pointer);
        EXPECT_EQ(container.template resolve<value *const>(), pointer);

        value copy = container.template resolve<value>();
        const value const_copy = container.template resolve<const value>();
        EXPECT_TRUE(copy.intact());
        EXPECT_TRUE(const_copy.intact());
        EXPECT_TRUE(instance.intact());
      });
}

// The handle a kept shared_ptr is resolved as is one object, and refers to the
// instance its interface is resolved as.
TYPED_TEST(request_identity_test, kept_handle_is_one_object) {
  TypeParam::template with<
      dingo::bind<scope<shared>, storage<std::shared_ptr<implementation>>,
                  interfaces<first_interface>>>([](auto &container) {
    auto &first =
        container.template resolve<std::shared_ptr<first_interface> &>();
    auto &second =
        container.template resolve<std::shared_ptr<first_interface> &>();
    EXPECT_EQ(&first, &second);
    EXPECT_EQ(container.template resolve<first_interface *>(), first.get());
    EXPECT_EQ(&container.template resolve<first_interface &>(), first.get());
  });
}

// A second interface at an offset reaches the same implementation as the first,
// and its handle shares the ownership of the first's.
TYPED_TEST(request_identity_test, second_interface_is_adjusted) {
  TypeParam::template with<
      dingo::bind<scope<shared>, storage<std::shared_ptr<implementation>>,
                  interfaces<first_interface, second_interface>>>(
      [](auto &container) {
        auto &first = container.template resolve<first_interface &>();
        auto &second = container.template resolve<second_interface &>();
        ASSERT_NE(dynamic_cast<implementation *>(&first), nullptr);
        EXPECT_EQ(dynamic_cast<implementation *>(&first),
                  dynamic_cast<implementation *>(&second));
        EXPECT_EQ(first.first_marker(), 1);
        EXPECT_EQ(second.second_marker(), 2);

        auto &first_handle =
            container.template resolve<std::shared_ptr<first_interface> &>();
        auto &second_handle =
            container.template resolve<std::shared_ptr<second_interface> &>();
        EXPECT_EQ(second_handle.get(), &second);
        EXPECT_FALSE(first_handle.owner_before(second_handle) ||
                     second_handle.owner_before(first_handle));
      });
}

// A concrete interface is given by reference, not as a copy of the base of
// the implementation, which would slice it. The static container rejects the
// copy as it compiles.
TEST(request_identity_runtime_test, concrete_interface_is_not_sliced) {
  runtime_container<> container;
  container.register_type<scope<shared>, storage<concrete_implementation>,
                          interfaces<concrete_interface>>();
  EXPECT_EQ(container.resolve<concrete_interface &>().marker, 6);
  EXPECT_THROW(container.resolve<concrete_interface>(),
               type_not_convertible_exception);
}

// Every element of a multi-dimensional array is constructed, and its
// references and pointers of every qualification agree.
TYPED_TEST(request_identity_test, multidimensional_array_is_one_object) {
  TypeParam::template with<dingo::bind<scope<shared>, storage<value[2][3]>>>(
      [](auto &container) {
        auto &values = container.template resolve<value(&)[2][3]>();
        for (auto &row : values) {
          for (auto &element : row) {
            EXPECT_TRUE(element.intact());
          }
        }
        EXPECT_EQ(&container.template resolve<const value(&)[2][3]>(), &values);
        EXPECT_EQ(&container.template resolve<volatile value(&)[2][3]>(),
                  &values);
        EXPECT_EQ(&container.template resolve<const volatile value(&)[2][3]>(),
                  &values);
      });
}

TYPED_TEST(request_identity_test, pointer_to_array_of_every_qualification) {
  TypeParam::template with<dingo::bind<scope<shared>, storage<value[2]>>>(
      [](auto &container) {
        auto values = container.template resolve<value(*)[2]>();
        EXPECT_TRUE((*values)[0].intact());
        EXPECT_TRUE((*values)[1].intact());
        EXPECT_EQ(container.template resolve<const value(*)[2]>(), values);
        EXPECT_EQ(container.template resolve<volatile value(*)[2]>(), values);
        EXPECT_EQ(container.template resolve<const volatile value(*)[2]>(),
                  values);
      });
}

// A factory may construct one alternative of a stored variant, of a
// dependency taken by value.
TYPED_TEST(request_identity_test,
           variant_is_made_by_constructor_of_alternative) {
  TypeParam::template with<
      dingo::bind<scope<shared>, storage<value>>,
      dingo::bind<scope<shared>, storage<std::variant<variant_a, variant_b>>,
                  factory<constructor<variant_a(value)>>>>([](auto &container) {
    auto instance =
        container.template resolve<std::variant<variant_a, variant_b>>();
    ASSERT_TRUE(std::holds_alternative<variant_a>(instance));
    EXPECT_EQ(std::get<variant_a>(instance).marker, 3);
  });
}

// A dependency names a binding by a typed key, in a constructor and in a
// callable.
TYPED_TEST(request_identity_test, dependency_selects_typed_key) {
  TypeParam::template with<
      dingo::bind<scope<shared>, storage<value>, key_type<key_a>>>(
      [](auto &container) {
        EXPECT_EQ(container.template construct<keyed_consumer>().marker, 3);
        EXPECT_EQ(container.invoke([](dependency<value &, key_type<key_a>> d) {
          return static_cast<value &>(d).marker();
        }),
                  3);
      });
}

namespace {
struct element_interface {
  virtual ~element_interface() = default;
  virtual int id() const = 0;
};

template <int Id> struct element : element_interface {
  int id() const override { return Id; }
};

using elements = std::vector<std::shared_ptr<element_interface>>;

struct keyed_collection_consumer {
  explicit keyed_collection_consumer(dependency<elements, key_type<key_a>> all)
      : count(static_cast<elements>(all).size()) {}
  std::size_t count;
};

template <int Id>
using keyed_element =
    dingo::bind<scope<shared>, storage<std::shared_ptr<element<Id>>>,
                interfaces<element_interface>, key_type<key_a>>;
} // namespace

// A dependency takes every binding of a typed key as a collection.
TEST(request_identity_test, dependency_takes_collection_of_typed_key) {
  static_container<bindings<keyed_element<0>, keyed_element<1>>> container;
  EXPECT_EQ(container.construct<keyed_collection_consumer>().count, 2u);
  EXPECT_EQ(container.invoke([](dependency<elements, key_type<key_a>> all) {
    return static_cast<elements>(all).size();
  }),
            2u);
}

namespace {
struct runtime_key_traits : runtime_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, element_interface>>;
};
} // namespace

// A lookup declared for one interface leaves the unkeyed resolution of others
// as it is.
TEST(request_identity_test, unrelated_lookup_leaves_unkeyed_resolution) {
  runtime_container<runtime_key_traits> container;
  container.register_type<scope<shared>, storage<value>>();
  EXPECT_EQ(&container.resolve<value &>(), container.resolve<value *>());
  EXPECT_TRUE(container.resolve<value>().intact());
}
