//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_list.h>

#include <gtest/gtest.h>

#include <string>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {
template <typename T> struct dingo_test : public test<T> {};
TYPED_TEST_SUITE(dingo_test, container_types, );

TYPED_TEST(dingo_test, unique_hierarchy) {
    using container_type = TypeParam;

    struct S {};
    struct U {};
    struct B {
        B(std::shared_ptr<S>&&) {}
    };

    container_type container;
    container
        .template register_type<scope<unique>, storage<std::shared_ptr<S>>>();
    container
        .template register_type<scope<unique>, storage<std::unique_ptr<U>>>();
    container.template register_type<scope<unique>, storage<B>>();

    container.template resolve<B>();
}

TYPED_TEST(dingo_test, nested_wrapper_injection) {
    using container_type = TypeParam;

    struct Consumer {
        Consumer(IClass& value, IClass* ptr) : value_(value), ptr_(ptr) {}

        IClass& value_;
        IClass* ptr_;
    };

    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<std::unique_ptr<Class>>>,
        interfaces<IClass>>();
    container.template register_type<scope<unique>, storage<Consumer>>();

    auto consumer = container.template resolve<Consumer>();
    AssertClass(consumer.value_);
    AssertClass(*consumer.ptr_);
    ASSERT_EQ(consumer.ptr_, std::addressof(consumer.value_));
}

TYPED_TEST(dingo_test, resolve_rollback) {
    using container_type = TypeParam;

    struct A : ClassTag<0> {};
    struct B : ClassTag<1> {};
    struct Ex {};
    struct C {
        C(A&, B&) { throw Ex(); }
    };

    container_type container;
    container.template register_type<scope<shared>, storage<A>>();
    container.template register_type<scope<shared>, storage<B>>();
    container.template register_type<scope<shared>, storage<C>>();

    ASSERT_THROW(container.template resolve<C&>(), Ex);
    ASSERT_EQ(A::Constructor, 1);
    ASSERT_EQ(A::Destructor, 0);
    ASSERT_EQ(B::Constructor, 1);
    ASSERT_EQ(B::Destructor, 0);

    container.template resolve<A&>();
    ASSERT_EQ(A::Constructor, 1);
    ASSERT_EQ(A::Destructor, 0);
    ASSERT_THROW(container.template resolve<C>(), Ex);
    ASSERT_EQ(A::Constructor, 1);
    ASSERT_EQ(A::Destructor, 0);
    ASSERT_EQ(B::Constructor, 1);
    ASSERT_EQ(B::Destructor, 0);
}

TYPED_TEST(dingo_test, type_already_registered) {
    using container_type = TypeParam;

    container_type container;
    {
        container.template register_type<scope<shared>, storage<Class>>();
        auto reg = [&] {
            container.template register_type<scope<shared>, storage<Class>>();
        };
        ASSERT_THROW(reg(), dingo::type_already_registered_exception);
    }
    {
        container.template register_type<scope<shared>, storage<Class>,
                                         interfaces<IClass>>();
        auto reg = [&] {
            container.template register_type<scope<shared>, storage<Class>,
                                             interfaces<IClass>>();
            ;
        };
        ASSERT_THROW(reg(), dingo::type_already_registered_exception);
    }
}

TYPED_TEST(dingo_test, shared_unique_reference) {
    using container_type = TypeParam;

    struct Unique {
        Unique() = default;
        std::shared_ptr<int> token = std::make_shared<int>(1);
    };

    struct Shared {
        Shared(Unique& unique) : token(unique.token) {}
        std::weak_ptr<int> token;
    };

    container_type container;
    container.template register_type<scope<shared>, storage<Shared>>();
    container.template register_type<scope<unique>, storage<Unique>,
                                     factory<constructor<Unique()>>>();

    // TODO: We need to detect that we are returning dangling reference to unique instance
    // For top-level container.resolve(), the reference is removed and copy is returned.
    // For recursively instantiated types, the context type needs to be checked. If it is
    // shared scope, all is fine.
    auto& c = container.template resolve<Shared&>();
    ASSERT_FALSE(c.token.expired());
    ASSERT_FALSE(container.template resolve<Shared&>().token.expired());
}

TYPED_TEST(dingo_test, shared_unique_reference_exception) {
    struct Unique {
        Unique(Unique&& other): dtor_(other.dtor_) {}
        Unique(const Unique& other): dtor_(other.dtor_) {}
        Unique(int& dtor): dtor_(dtor) {}
        ~Unique() { ++dtor_; }
        int& dtor_;
    };

    struct Shared {
        Shared(Unique&) { throw std::runtime_error("Hello!"); }
    };

    using container_type = TypeParam;

    using container_type = TypeParam;
    container_type container;

    int unique_dtor = 0;
    container.template register_type<scope<external>, storage<int&>>(unique_dtor);
    container.template register_type<scope<unique>, storage<Unique>>();
    container.template register_type<scope<shared>, storage<Shared>>();

    ASSERT_THROW(container.template resolve<Shared>(), std::runtime_error);
    // Two Uniques should be destroyed as there is one move constructor
    // TODO: pass counters
    ASSERT_EQ(unique_dtor, 2);
}

TYPED_TEST(dingo_test, exception_message_type_not_found) {
    using container_type = TypeParam;

    struct Dependency {
        explicit Dependency(int) {}
    };
    struct Missing {
        Dependency& dependency;
    };

    container_type container;

    try {
        (void)container.template resolve<Missing>();
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string message = e.what();
        std::string expected = "type not found: ";
        expected += type_name<Dependency&>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("lookup plan: "), std::string::npos);
        EXPECT_NE(message.find("lookup single"), std::string::npos);
        EXPECT_NE(message.find("constructor path: "), std::string::npos);
        EXPECT_NE(message.find(type_name<Missing>()), std::string::npos);
        EXPECT_NE(message.find("automatic slot 0"), std::string::npos);
    }
}

TYPED_TEST(dingo_test, exception_message_explicit_constructor_type_not_found) {
    using container_type = TypeParam;

    struct Dependency {
        explicit Dependency(int) {}
    };
    struct Missing {
        explicit Missing(Dependency&) {}
    };

    container_type container;

    try {
        (void)container.template construct<Missing, constructor<Missing(Dependency&)>>();
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string message = e.what();
        std::string expected = "type not found: ";
        expected += type_name<Dependency&>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("constructor path: "), std::string::npos);
        EXPECT_NE(message.find(type_name<Missing>()), std::string::npos);
        EXPECT_NE(message.find(type_name<Dependency&>()), std::string::npos);
    }
}

TYPED_TEST(dingo_test, exception_message_type_ambiguous) {
    using container_type = TypeParam;

    struct IValue {
        virtual ~IValue() = default;
    };
    struct ValueA : IValue {};
    struct ValueB : IValue {};

    container_type container;
    container.template register_type<scope<shared>, storage<ValueA>,
                                     interfaces<IValue>>();
    container.template register_type<scope<shared>, storage<ValueB>,
                                     interfaces<IValue>>();

    try {
        (void)container.template resolve<IValue&>();
        FAIL() << "expected type_ambiguous_exception";
    } catch (const type_ambiguous_exception& e) {
        std::string message = e.what();
        std::string expected = "type resolution is ambiguous: ";
        expected += type_name<IValue&>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("lookup plan: "), std::string::npos);
        EXPECT_NE(message.find("lookup single"), std::string::npos);
        EXPECT_NE(message.find("candidates:"), std::string::npos);
        EXPECT_NE(message.find("registered plans:"), std::string::npos);
        EXPECT_NE(message.find("payload default_constructed"),
                  std::string::npos);
        EXPECT_NE(message.find("materialization single_interface"),
                  std::string::npos);
        EXPECT_NE(message.find("storage shared"), std::string::npos);
        EXPECT_NE(message.find(type_name<ValueA>()), std::string::npos);
        EXPECT_NE(message.find(type_name<ValueB>()), std::string::npos);
    }
}

TYPED_TEST(dingo_test, exception_message_type_already_registered) {
    using container_type = TypeParam;

    struct IService {
        virtual ~IService() = default;
    };
    struct Service : IService {};

    container_type container;
    container.template register_type<scope<shared>, storage<Service>,
                                     interfaces<IService>>();

    try {
        container.template register_type<scope<shared>, storage<Service>,
                                         interfaces<IService>>();
        FAIL() << "expected type_already_registered_exception";
    } catch (const type_already_registered_exception& e) {
        std::string message = e.what();
        std::string expected = "type already registered: interface ";
        expected += type_name<IService>();
        expected += ", storage ";
        expected += type_name<Service>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("registration plan: "), std::string::npos);
        EXPECT_NE(message.find(std::string("interfaces [") +
                                   type_name<IService>() + "]"),
                  std::string::npos);
        EXPECT_NE(message.find(std::string("scope ") + type_name<shared>()),
                  std::string::npos);
        EXPECT_NE(message.find(std::string("registered storage ") +
                                   type_name<Service>()),
                  std::string::npos);
        EXPECT_NE(message.find("payload default_constructed"),
                  std::string::npos);
        EXPECT_NE(message.find("materialization single_interface"),
                  std::string::npos);
    }
}

TYPED_TEST(dingo_test, multi_interface_registration_is_atomic_on_duplicate) {
    using container_type = TypeParam;

    struct interface_a {
        virtual ~interface_a() = default;
    };
    struct interface_b {
        virtual ~interface_b() = default;
    };
    struct service : interface_a, interface_b {};

    container_type container;
    container.template register_type<scope<shared>, storage<service>,
                                     interfaces<interface_b>>();

    ASSERT_THROW((container.template register_type<
                      scope<shared>, storage<service>,
                      interfaces<interface_a, interface_b>>()),
                 type_already_registered_exception);
    ASSERT_THROW((container.template resolve<interface_a&>()),
                 type_not_found_exception);
    ASSERT_NO_THROW((container.template resolve<interface_b&>()));
}

TYPED_TEST(dingo_test, multi_interface_registration_rejects_internal_duplicate) {
    using container_type = TypeParam;

    struct interface_a {
        virtual ~interface_a() = default;
    };
    struct service : interface_a {};

    container_type container;

    ASSERT_THROW((container.template register_type<
                      scope<shared>, storage<service>,
                      interfaces<interface_a, interface_a>>()),
                 type_already_registered_exception);
    ASSERT_THROW((container.template resolve<interface_a&>()),
                 type_not_found_exception);
}

TYPED_TEST(dingo_test, exception_message_type_not_convertible) {
    using container_type = TypeParam;

    struct IService {
        virtual ~IService() = default;
    };
    struct Service : IService {};

    Service service;
    container_type container;
    container.template register_type<scope<external>, storage<Service&>,
                                     interfaces<IService>>(service);

    try {
        (void)container.template resolve<std::shared_ptr<IService>>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string message = e.what();
        std::string expected = "type is not convertible to ";
        expected += type_name<std::shared_ptr<IService>>();
        expected += " from ";
        expected += type_name<Service&>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("resolution plan: request value"),
                  std::string::npos);
        EXPECT_NE(message.find("storage external"), std::string::npos);
        EXPECT_NE(message.find("shape plain"), std::string::npos);
    }
}

TYPED_TEST(dingo_test, exception_message_auto_constructor_type_not_convertible) {
    using container_type = TypeParam;

    struct IService {
        virtual ~IService() = default;
    };
    struct Service : IService {};
    struct Missing {
        explicit Missing(std::shared_ptr<IService>) {}
    };

    Service service;
    container_type container;
    container.template register_type<scope<external>, storage<Service&>,
                                     interfaces<IService>>(service);

    try {
        (void)container.template construct<Missing>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string message = e.what();
        std::string expected = "type is not convertible to ";
        expected += type_name<std::shared_ptr<IService>>();
        expected += " from ";
        expected += type_name<Service&>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("constructor path: "), std::string::npos);
        EXPECT_NE(message.find(type_name<Missing>()), std::string::npos);
        EXPECT_NE(message.find("automatic slot 0"), std::string::npos);
    }
}

TYPED_TEST(dingo_test, exception_message_explicit_constructor_type_not_convertible) {
    using container_type = TypeParam;

    struct IService {
        virtual ~IService() = default;
    };
    struct Service : IService {};
    struct Missing {
        explicit Missing(std::shared_ptr<IService>) {}
    };

    Service service;
    container_type container;
    container.template register_type<scope<external>, storage<Service&>,
                                     interfaces<IService>>(service);

    try {
        (void)container.template construct<Missing,
                                           constructor<Missing(std::shared_ptr<IService>)>>();
        FAIL() << "expected type_not_convertible_exception";
    } catch (const type_not_convertible_exception& e) {
        std::string message = e.what();
        std::string expected = "type is not convertible to ";
        expected += type_name<std::shared_ptr<IService>>();
        expected += " from ";
        expected += type_name<Service&>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("constructor path: "), std::string::npos);
        EXPECT_NE(message.find(type_name<Missing>()), std::string::npos);
        EXPECT_NE(message.find(type_name<std::shared_ptr<IService>>()),
                  std::string::npos);
    }
}

} // namespace dingo
