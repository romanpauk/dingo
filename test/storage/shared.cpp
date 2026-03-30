//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

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
        return custom_sum{
            std::variant<custom_sum_a, custom_sum_b>(
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
TYPED_TEST_SUITE(shared_test, container_types, );

TYPED_TEST(shared_test, value) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());

        AssertTypeNotConvertible<
            Class, type_list<std::shared_ptr<Class>, std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, value_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>, storage<Class>,
                                     interfaces<IClass>>();

    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(shared_test, ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class*>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());

        AssertTypeNotConvertible<
            Class, type_list<std::shared_ptr<Class>, std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>, storage<Class*>,
                                     interfaces<IClass>>();

    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(shared_test, ptr_interface_deletes_owner_not_converted_pointer) {
    using container_type = TypeParam;

    pointer_conversion_owner::destructors = 0;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<pointer_conversion_owner*>,
                                         interfaces<pointer_conversion_view>>();

        auto* view = container.template resolve<pointer_conversion_view*>();
        ASSERT_NE(view, nullptr);
        EXPECT_EQ(pointer_conversion_owner::destructors, 0);
    }

    EXPECT_EQ(pointer_conversion_owner::destructors, 1);
}

TYPED_TEST(shared_test, shared_ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<std::shared_ptr<Class>>,
                                         interfaces<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>>());
        AssertClass(
            *container.template resolve<std::shared_ptr<const Class>>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::shared_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::shared_ptr<const Class>&>());
        AssertClass(**container.template resolve<std::shared_ptr<Class>*>());
        AssertClass(
            **container.template resolve<const std::shared_ptr<Class>*>());
        AssertClass(
            **container
                  .template resolve<const std::shared_ptr<const Class>*>());

        AssertTypeNotConvertible<Class, type_list<std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, shared_ptr_multiple_interface) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<Class>>,
                                interfaces<Class, IClass>>();

    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>&>());
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(shared_test, shared_ptr_single_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<Class>>, interfaces<IClass>>();

    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>&>());
}

TYPED_TEST(shared_test, shared_ptr_nested_unique_ptr) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<std::unique_ptr<Class>>>,
        interfaces<Class>>();

    auto& outer =
        container.template resolve<std::shared_ptr<std::unique_ptr<Class>>&>();
    auto& inner = container.template resolve<std::unique_ptr<Class>&>();
    auto* leaf = container.template resolve<Class*>();

    AssertClass(*inner);
    AssertClass(*leaf);
    ASSERT_EQ(outer.get(), std::addressof(inner));
    ASSERT_EQ(inner.get(), leaf);
    ASSERT_THROW(container.template resolve<std::unique_ptr<Class>>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_test, shared_ptr_nested_unique_ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<std::unique_ptr<Class>>>,
        interfaces<IClass>>();

    auto& leaf = container.template resolve<IClass&>();
    auto* ptr = container.template resolve<IClass*>();

    AssertClass(leaf);
    AssertClass(*ptr);
    ASSERT_EQ(ptr, std::addressof(leaf));
    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<IClass>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(
        container.template resolve<std::shared_ptr<std::unique_ptr<IClass>>&>(),
        type_not_convertible_exception);
}

TYPED_TEST(shared_test, unique_ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<shared>, storage<std::unique_ptr<Class>>, interfaces<Class>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<const Class*>());
        AssertClass(container.template resolve<const Class&>());
        AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::unique_ptr<Class>&>());
        AssertClass(
            *container.template resolve<const std::unique_ptr<const Class>&>());
        AssertClass(**container.template resolve<std::unique_ptr<Class>*>());
        AssertClass(
            **container.template resolve<const std::unique_ptr<Class>*>());
        AssertClass(
            **container
                  .template resolve<const std::unique_ptr<const Class>*>());

        AssertTypeNotConvertible<Class, type_list<std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, unique_ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::unique_ptr<Class>>, interfaces<IClass>>();
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(*container.template resolve<std::unique_ptr<IClass>&>());
}

TYPED_TEST(shared_test, optional) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<std::optional<Class>>,
                                         interfaces<Class, IClass>>();

        AssertClass(container.template resolve<Class*>());
        AssertClass(container.template resolve<const Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<const Class&>());
        AssertClass(*container.template resolve<std::optional<Class>&>());
        AssertClass(*container.template resolve<const std::optional<Class>&>());
        AssertClass(**container.template resolve<std::optional<Class>*>());
        AssertClass(
            **container.template resolve<const std::optional<Class>*>());

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::MoveConstructor, 1);
        ASSERT_EQ(Class::Destructor, 1);
        ASSERT_EQ(Class::CopyConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, optional_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::optional<Class>>, interfaces<IClass>>();
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(shared_test, variant_factory_selects_alternative) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::variant<A, B>>,
        factory<constructor_detection<A>>>();

    auto whole = container.template resolve<std::variant<A, B>>();
    ASSERT_TRUE(std::holds_alternative<A>(whole));
    EXPECT_EQ(std::get<A>(whole).value, 0);

    auto& value = container.template resolve<std::variant<A, B>&>();
    ASSERT_TRUE(std::holds_alternative<A>(value));
    EXPECT_EQ(std::get<A>(value).value, 0);

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<A>(value));

    auto& selected_const = container.template resolve<const A&>();
    EXPECT_EQ(selected_const.value, 0);
    EXPECT_EQ(&selected_const, &std::get<A>(value));

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<A>(value));

    auto* selected_const_ptr = container.template resolve<const A*>();
    ASSERT_NE(selected_const_ptr, nullptr);
    EXPECT_EQ(selected_const_ptr, &std::get<A>(value));

    ASSERT_THROW(container.template resolve<B>(), type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<B*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_test, variant_explicit_interfaces_override_defaults) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::variant<A, B>>,
        factory<constructor_detection<A>>, interfaces<A>>();

    auto copy = container.template resolve<A>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<A&>();
    EXPECT_EQ(selected.value, 0);

    auto* selected_ptr = container.template resolve<A*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr->value, 0);

    ASSERT_THROW((container.template resolve<std::variant<A, B>>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>&>()),
                 type_not_found_exception);
    ASSERT_THROW((container.template resolve<std::variant<A, B>*>()),
                 type_not_found_exception);
    ASSERT_THROW(container.template resolve<B>(), type_not_found_exception);
}

TYPED_TEST(shared_test, custom_alternative_type_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<shared>, storage<custom_sum>,
                                     factory<constructor_detection<custom_sum_a>>>();

    auto whole = container.template resolve<custom_sum>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(whole.value));
    EXPECT_EQ(std::get<custom_sum_a>(whole.value).value, 0);

    auto& value = container.template resolve<custom_sum&>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(value.value));
    EXPECT_EQ(std::get<custom_sum_a>(value.value).value, 0);

    auto copy = container.template resolve<custom_sum_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<custom_sum_a&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<custom_sum_a>(value.value));

    auto* selected_ptr = container.template resolve<custom_sum_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<custom_sum_a>(value.value));

    ASSERT_THROW(container.template resolve<custom_sum_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_test, shared_ptr_variant_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<shared_ptr_variant>>,
        factory<function<&make_shared_ptr_variant>>>();

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 0);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 0);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 0);
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

TYPED_TEST(shared_test, shared_ptr_custom_sum_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<shared>, storage<std::shared_ptr<custom_sum>>,
                                     factory<function<&make_shared_custom_sum>>>();

    auto whole = container.template resolve<custom_sum>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(whole.value));
    EXPECT_EQ(std::get<custom_sum_a>(whole.value).value, 0);

    auto& value = container.template resolve<custom_sum&>();
    ASSERT_TRUE(std::holds_alternative<custom_sum_a>(value.value));
    EXPECT_EQ(std::get<custom_sum_a>(value.value).value, 0);

    auto copy = container.template resolve<custom_sum_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<custom_sum_a&>();
    EXPECT_EQ(selected.value, 0);
    EXPECT_EQ(&selected, &std::get<custom_sum_a>(value.value));

    auto* selected_ptr = container.template resolve<custom_sum_a*>();
    ASSERT_NE(selected_ptr, nullptr);
    EXPECT_EQ(selected_ptr, &std::get<custom_sum_a>(value.value));

    ASSERT_THROW(container.template resolve<custom_sum_b>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b&>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<custom_sum_b*>(),
                 type_not_convertible_exception);
}

TYPED_TEST(shared_test, unique_ptr_variant_factory_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::unique_ptr<shared_ptr_variant>>,
        factory<function<&make_unique_ptr_variant>>>();

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 0);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 0);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 0);
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

TYPED_TEST(shared_test, unique_ptr_variant_direct_construction_selects_alternative) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<
        scope<shared>, storage<std::unique_ptr<shared_ptr_variant>>,
        factory<constructor_detection<shared_ptr_variant_a>>>();

    auto whole = container.template resolve<shared_ptr_variant>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(whole));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(whole).value, 0);

    auto& value = container.template resolve<shared_ptr_variant&>();
    ASSERT_TRUE(std::holds_alternative<shared_ptr_variant_a>(value));
    EXPECT_EQ(std::get<shared_ptr_variant_a>(value).value, 0);

    auto copy = container.template resolve<shared_ptr_variant_a>();
    EXPECT_EQ(copy.value, 0);

    auto& selected = container.template resolve<shared_ptr_variant_a&>();
    EXPECT_EQ(selected.value, 0);
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

TYPED_TEST(shared_test, shared_multiple) {
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
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<Class>>,
                                interfaces<IClass1, IClass2>>();
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

TYPED_TEST(shared_test, hierarchy) {
    using container_type = TypeParam;

    struct S : Class {};
    struct U : Class {
        U(S& s1) { AssertClass(s1); }
    };

    struct B : Class {
        B(S s1, S& s2, S* s3, std::shared_ptr<S>* s4, std::shared_ptr<S>& s5,
          std::shared_ptr<S> s6,
          U u1, U& u2, U* u3, std::unique_ptr<U>* u4,
          std::unique_ptr<U>& u5) {
            AssertClass(s1);
            AssertClass(s2);
            AssertClass(*s3);
            AssertClass(**s4);
            AssertClass(*s5);
            AssertClass(*s6);
            AssertClass(u1);
            AssertClass(u2);
            AssertClass(*u3);
            AssertClass(**u4);
            AssertClass(*u5);
        }
    };

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<S>>>();
    container
        .template register_type<scope<shared>, storage<std::unique_ptr<U>>>();
    container.template register_type<scope<shared>, storage<B>>();

    container.template resolve<B&>();
}

TYPED_TEST(shared_test, const_hierarchy) {
    using container_type = TypeParam;

    struct S : Class {};
    struct U : Class {
        U(S& s1) { AssertClass(s1); }
    };

    struct B : Class {
        B(const S s1, const S& s2, const S* s3, const std::shared_ptr<S>* s4,
          const std::shared_ptr<S>& s5, const std::shared_ptr<S> s6,
          const U u1, const U& u2, const U* u3, const std::unique_ptr<U>* u4,
          std::unique_ptr<U>& u5) {
            AssertClass(s1);
            AssertClass(s2);
            AssertClass(*s3);
            AssertClass(**s4);
            AssertClass(*s5);
            AssertClass(*s6);
            AssertClass(u1);
            AssertClass(u2);
            AssertClass(*u3);
            AssertClass(**u4);
            AssertClass(*u5);
        }
    };

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<S>>>();
    container
        .template register_type<scope<shared>, storage<std::unique_ptr<U>>>();
    container.template register_type<scope<shared>, storage<B>>();

    container.template resolve<B&>();
}

TYPED_TEST(shared_test, ambiguous_resolve) {
    struct A {
        A() : index(0) {}
        A(int) : index(1) {}
        A(float) : index(2) {}
        A(int, float) : index(3) {}
        A(float, int) : index(4) {}

        int index;
    };

    using container_type = TypeParam;

    using container_type = TypeParam;
    container_type container;

    container.template register_type<scope<shared>, storage<A>,
                                     factory<constructor<A, float, int>>>();
    container.template register_type<scope<shared>, storage<float>>();
    container.template register_type<scope<shared>, storage<int>>();

    ASSERT_EQ((container.template construct<A, constructor<A>>().index), 0);
    ASSERT_EQ((container.template construct<A, constructor<A()>>().index), 0);
    ASSERT_EQ((container.template construct<A, constructor<A(int)>>().index),
              1);
    ASSERT_EQ((container.template construct<A, constructor<A(float)>>().index),
              2);
    ASSERT_EQ(
        (container.template construct<A, constructor<A, int, float>>().index),
        3);
    ASSERT_EQ(
        (container.template construct<A, constructor<A, float, int>>().index),
        4);
}

} // namespace dingo
