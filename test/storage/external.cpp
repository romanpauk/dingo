//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <variant>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {
namespace {
template <typename Target, typename Source>
void expect_type_not_convertible_with_plan(
    const type_not_convertible_exception& e,
    std::initializer_list<std::string_view> expected_plan_parts) {
    std::string message = e.what();
    std::string expected = "type is not convertible to ";
    expected += type_name<Target>();
    expected += " from ";
    expected += type_name<Source>();

    EXPECT_NE(message.find(expected), std::string::npos);
    EXPECT_NE(message.find("resolution plan:"), std::string::npos);

    for (auto part : expected_plan_parts) {
        EXPECT_NE(message.find(part), std::string::npos);
    }
}

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
TYPED_TEST_SUITE(external_test, container_types, );

TYPED_TEST(external_test, value) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class>,
                                     interfaces<Class, IClass>>(std::move(c));

    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
    ASSERT_THROW(container.template resolve<std::optional<Class>>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, ref) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class&>,
                                     interfaces<Class, IClass>>(c);

    ASSERT_EQ(container.template resolve<Class*>(), &c);
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(external_test, ptr) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class*>,
                                     interfaces<Class, IClass>>(&c);

    ASSERT_EQ(container.template resolve<Class*>(), &c);
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<Class>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<Class>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<IClass>>(),
                 type_not_convertible_exception);
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(external_test, shared_ptr_interface) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::shared_ptr<Class>>, interfaces<IClass>>(c);
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
}

TYPED_TEST(external_test, exception_message_type_not_convertible_shared_ptr_interface) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::shared_ptr<Class>>, interfaces<IClass>>(c);

    try {
        (void)container.template resolve<std::unique_ptr<IClass>>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        expect_type_not_convertible_with_plan<std::unique_ptr<IClass>,
                                              std::shared_ptr<Class>>(
            e, {"request value", "storage external", "shape wrapper",
                "family handle"});
    }
}

TYPED_TEST(external_test, shared_ptr) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>>,
                                     interfaces<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>>());
    AssertClass(**container.template resolve<std::shared_ptr<Class>*>());
    AssertClass(*container.template resolve<std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>>());
    AssertClass(**container.template resolve<const std::shared_ptr<Class>*>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(*container.template resolve<std::shared_ptr<const Class>&>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>&>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<const std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<std::shared_ptr<const IClass>>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const IClass>>());
}

TYPED_TEST(external_test, shared_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>&>,
                                     interfaces<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(container.template resolve<std::shared_ptr<Class>>());
    AssertClass(container.template resolve<std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>&>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<const std::shared_ptr<IClass>>());
    AssertClass(container.template resolve<std::shared_ptr<const IClass>>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(external_test,
           exception_message_type_not_convertible_shared_ptr_interface_ref) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>&>,
                                     interfaces<IClass>>(c);

    try {
        (void)container.template resolve<std::unique_ptr<IClass>>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        expect_type_not_convertible_with_plan<std::unique_ptr<IClass>,
                                              std::shared_ptr<Class>&>(
            e, {"request value", "storage external", "shape plain"});
    }
}

TYPED_TEST(external_test, unique_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>&>,
                                     interfaces<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::unique_ptr<Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test,
           exception_message_type_not_convertible_unique_ptr_interface_ref) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>&>,
                                     interfaces<Class, IClass>>(c);

    try {
        (void)container.template resolve<std::unique_ptr<IClass>&>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        expect_type_not_convertible_with_plan<std::unique_ptr<IClass>&,
                                              std::unique_ptr<Class>&>(
            e, {"request lvalue_reference", "storage external",
                "shape plain"});
    }
}

TYPED_TEST(external_test, unique_ptr_move) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>>,
                                     interfaces<Class, IClass>>(std::move(c));
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::unique_ptr<Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(external_test, unique_ptr_interface_move) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::unique_ptr<Class>>, interfaces<IClass>>(
        std::move(c));
    AssertClass(*container.template resolve<std::unique_ptr<IClass>&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_test, exception_message_type_not_convertible_unique_ptr_value) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>>,
                                     interfaces<Class, IClass>>(std::move(c));

    try {
        (void)container.template resolve<Class>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        expect_type_not_convertible_with_plan<Class, std::unique_ptr<Class>>(
            e, {"request value", "storage external", "shape wrapper",
                "family borrow"});
    }
}

TYPED_TEST(external_test, optional_ref) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>&>,
                                interfaces<Class, IClass>>(c);
    ASSERT_EQ(&container.template resolve<std::optional<Class>&>(), &c);
    ASSERT_EQ(container.template resolve<Class*>(), std::addressof(c.value()));
    AssertClass(*container.template resolve<std::optional<Class>&>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_test, optional_move) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>>,
                                interfaces<Class, IClass>>(std::move(c));
    AssertClass(*container.template resolve<std::optional<Class>&>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_test,
           exception_message_type_not_convertible_optional_interface_ref) {
    using container_type = TypeParam;
    struct Base {};
    struct Derived : Base {};

    auto c = std::make_optional<Derived>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Derived>&>,
                                interfaces<Base>>(c);

    try {
        (void)container.template resolve<std::optional<Base>&>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        expect_type_not_convertible_with_plan<std::optional<Base>&,
                                              std::optional<Derived>&>(
            e, {"request lvalue_reference", "storage external",
                "shape plain"});
    }
}

TYPED_TEST(external_test, exception_message_type_not_convertible_optional_value) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>>,
                                interfaces<Class, IClass>>(std::move(c));

    try {
        (void)container.template resolve<Class>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        expect_type_not_convertible_with_plan<Class, std::optional<Class>>(
            e, {"request value", "storage external", "shape wrapper",
                "family borrow"});
    }
}

TYPED_TEST(external_test, variant_ref) {
    using container_type = TypeParam;
    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    std::variant<A, B> c(std::in_place_type<A>, 7);

    container_type container;
    container
        .template register_type<scope<external>, storage<std::variant<A, B>&>>(
            c);

    auto whole = container.template resolve<std::variant<A, B>>();
    ASSERT_TRUE(std::holds_alternative<A>(whole));
    EXPECT_EQ(std::get<A>(whole).value, 7);

    auto& value = container.template resolve<std::variant<A, B>&>();

    ASSERT_EQ(&value, &c);
    ASSERT_TRUE(std::holds_alternative<A>(value));
    EXPECT_EQ(std::get<A>(value).value, 7);

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 7);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 7);
    EXPECT_EQ(&selected, &std::get<A>(c));

    auto& selected_const = container.template resolve<const A&>();
    EXPECT_EQ(selected_const.value, 7);
    EXPECT_EQ(&selected_const, &std::get<A>(c));

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<A>(c));

    auto* selected_const_ptr = container.template resolve<const A*>();
    ASSERT_NE(selected_const_ptr, nullptr);
    EXPECT_EQ(selected_const_ptr, &std::get<A>(c));

    ASSERT_THROW(container.template resolve<B>(), type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, variant_explicit_interfaces_override_defaults) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    std::variant<A, B> c(std::in_place_type<A>, 11);

    container_type container;
    container
        .template register_type<scope<external>, storage<std::variant<A, B>&>,
                                interfaces<A>>(c);

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 11);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 11);
    EXPECT_EQ(&selected, &std::get<A>(c));

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<A>(c));

    ASSERT_THROW((container.template resolve<std::variant<A, B>>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>&>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>*>()),
                 type_not_found_exception);
    ASSERT_THROW(container.template resolve<B>(), type_not_found_exception);
}

TYPED_TEST(external_test, shared_ptr_variant_ref) {
    using container_type = TypeParam;

    auto handle = std::make_shared<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, 13);

    container_type container;
    container
        .template register_type<scope<external>,
                                storage<std::shared_ptr<shared_ptr_variant>>>(handle);

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 13);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 13);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 13);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 13);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(*handle));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(*handle));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, unique_ptr_variant_ref) {
    using container_type = TypeParam;

    auto handle = std::make_unique<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, 13);

    container_type container;
    container
        .template register_type<scope<external>,
                                storage<std::unique_ptr<shared_ptr_variant>&>>(
            handle);

    ASSERT_THROW(container.template resolve<shared_ptr_variant>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_a>(),
                 type_not_convertible_exception);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 13);

    auto* value_ptr = container.template resolve<shared_ptr_variant*>();
    ASSERT_NE(value_ptr, nullptr);
    EXPECT_EQ(value_ptr, &*handle);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 13);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(*handle));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(*handle));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, unique_ptr_variant_move) {
    using container_type = TypeParam;

    auto handle = std::make_unique<shared_ptr_variant>(
        std::in_place_type<shared_ptr_variant_a>, 17);

    container_type container;
    container
        .template register_type<scope<external>,
                                storage<std::unique_ptr<shared_ptr_variant>>>(
            std::move(handle));

    ASSERT_THROW(container.template resolve<shared_ptr_variant>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_a>(),
                 type_not_convertible_exception);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 17);

    auto* value_ptr = container.template resolve<shared_ptr_variant*>();
    ASSERT_NE(value_ptr, nullptr);
    EXPECT_EQ(value_ptr, &value);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 17);
    EXPECT_EQ(&selected, &std::get<shared_ptr_variant_a>(value));

    auto* selected_ptr = container.template resolve<shared_ptr_variant_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<shared_ptr_variant_a>(value));

    ASSERT_THROW(container.template resolve<shared_ptr_variant_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<shared_ptr_variant_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, shared_multiple) {
    using container_type = TypeParam;
    struct C {
        C(std::shared_ptr<IClass1>& c1, std::shared_ptr<IClass1>& c11,
          std::shared_ptr<IClass2>& c2, std::shared_ptr<IClass2>& c22) {
            AssertClass(*c1);
            AssertClass(*c11);
            assert(&c1 == &c11);
            AssertClass(*c2);
            AssertClass(*c22);
            assert(&c2 == &c22);
        }
    };

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>>,
                                     interfaces<IClass1, IClass2>>(
        std::make_shared<Class>());
    container.template construct<C>();

    {
        auto& c1 = container.template resolve<std::shared_ptr<IClass1>&>();
        AssertClass(*c1);
        auto& c2 = container.template resolve<std::shared_ptr<IClass2>&>();
        AssertClass(*c2);
        auto& c11 = container.template resolve<std::shared_ptr<IClass1>&>();
        AssertClass(*c11);
        auto& c22 = container.template resolve<std::shared_ptr<IClass2>&>();
        AssertClass(*c22);
        ASSERT_EQ(&c1, &c11);
        ASSERT_EQ(&c2, &c22);
    }
}

TYPED_TEST(external_test, constructor_ambiguous) {
    using container_type = TypeParam;
    container_type container;

    struct A {
        A(int) {}
        A(double) {}
        static A& instance() {
            static A a(1);
            return a;
        }
    };

    container.template register_type<scope<external>, storage<A>>(
        A::instance());
}

TYPED_TEST(external_test, constructor_private) {
    using container_type = TypeParam;
    container_type container;

    struct A {
        static A& instance() {
            static A a;
            return a;
        }

      private:
        A() {}
    };

    container.template register_type<scope<external>, storage<A>>(
        A::instance());
}


TYPED_TEST(external_test, vector)
{
    using container_type = TypeParam;

    struct vector_class_ctor {
        std::vector<int> data;
        vector_class_ctor(std::vector<int> v): data(v) {}
    };

    struct vector_class_aggregate {
        std::vector<int> data;
    };

    std::vector<int> vec = {1,2,3};

    container_type container;
    container.template register_type<dingo::scope<dingo::external>, dingo::storage<std::vector<int>>>(vec);
    container.template register_type<dingo::scope<dingo::shared>, dingo::storage<vector_class_ctor>>();
    container.template register_type<dingo::scope<dingo::shared>, dingo::storage<vector_class_aggregate>>();

    {
        auto& cls = container.template resolve<vector_class_ctor&>();
        ASSERT_EQ(cls.data, vec);
    }
    {
        auto& cls = container.template resolve<vector_class_aggregate&>();
        ASSERT_EQ(cls.data, vec);
    }
}

} // namespace dingo
