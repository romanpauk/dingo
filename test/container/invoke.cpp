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

#include <gtest/gtest.h>

#include "support/containers.h"
#include "support/test.h"

namespace dingo {

template <typename T> struct invoke_test : public test<T> {};
TYPED_TEST_SUITE(invoke_test, container_types, );

template< typename T = void > struct invoke_test_functions {
    static void call(int arg) { result = arg * 2; }
    static int result;

    static int call_return(int arg) { return arg * 2; }
};

template< typename T > int invoke_test_functions<T>::result;

TYPED_TEST(invoke_test, invoke) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<external>, storage<int>>(2);

    {
        std::function<int(int)> fn = [](int arg) { return arg * 2; };
        ASSERT_EQ(container.invoke(fn), 4);
    }

    {
        int result = 0;
        std::function<void(int)> fn = [&](int arg) { result = arg * 2; };
        container.invoke(fn);
        ASSERT_EQ(result, 4);
    }

    {
        ASSERT_EQ(container.invoke([](int arg) { return arg * 2; }), 4);
    }

    {
        int result = 0;
        container.invoke([&](int arg) { result = arg * 2; });
        ASSERT_EQ(result, 4);
    }

    {
        container.invoke(&invoke_test_functions<>::call);
        ASSERT_EQ(invoke_test_functions<>::result, 4);
    }

    {
        ASSERT_EQ(container.invoke(invoke_test_functions<>::call_return), 4);
        ASSERT_EQ(container.invoke(&invoke_test_functions<>::call_return), 4);
    }
}

TYPED_TEST(invoke_test, invoke_exception_required_by_type) {
    using container_type = TypeParam;

    struct Missing {
        explicit Missing(int) {}
    };

    container_type container;
    auto callable = [](Missing&) {};

    try {
        container.invoke(callable);
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string expected = "type not found: ";
        expected += type_name<Missing&>();
        expected += " (required by ";
        expected += type_name<decltype(callable)>();
        expected += ")";

        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TYPED_TEST(invoke_test, invoke_static_function_exception_required_by_type) {
    using container_type = TypeParam;

    struct Missing {
        explicit Missing(int) {}
    };

    struct functions {
        static void call(Missing&) {}
    };

    container_type container;

    try {
        container.invoke(&functions::call);
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string expected = "type not found: ";
        expected += type_name<Missing&>();
        expected += " (required by ";
        expected += type_name<decltype(&functions::call)>();
        expected += ")";

        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TYPED_TEST(invoke_test, invoke_functor_exception_required_by_type) {
    using container_type = TypeParam;

    struct Missing {
        explicit Missing(int) {}
    };

    struct callable {
        void operator()(Missing&) const {}
    };

    container_type container;

    try {
        container.invoke(callable{});
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string expected = "type not found: ";
        expected += type_name<Missing&>();
        expected += " (required by ";
        expected += type_name<callable>();
        expected += ")";

        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

}
