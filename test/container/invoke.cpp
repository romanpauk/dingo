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

#include <functional>

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

TYPED_TEST(invoke_test, invoke_value_wrappers) {
    using container_type = TypeParam;

    struct SharedValue {
        explicit SharedValue(int init) : value(init) {}
        int value;
    };

    struct UniqueValue {
        explicit UniqueValue(int init) : value(init) {}
        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(7);
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<SharedValue>>>();
    container.template register_type<scope<unique>,
                                     storage<std::unique_ptr<UniqueValue>>>();

    ASSERT_EQ(container.invoke([](std::shared_ptr<SharedValue> value) {
        return value->value;
    }), 7);

    ASSERT_EQ(container.invoke([](std::unique_ptr<UniqueValue> value) {
        return value->value;
    }), 7);
}

TYPED_TEST(invoke_test, invoke_mutable_lambda) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<external>, storage<int>>(3);

    auto fn = [factor = 2](int arg) mutable {
        factor += arg;
        return factor;
    };

    ASSERT_EQ(container.invoke(fn), 5);
    ASSERT_EQ(container.invoke(fn), 8);
}

#if defined(__cpp_lib_move_only_function)
TYPED_TEST(invoke_test, invoke_move_only_function) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);

    std::move_only_function<int(int)> fn = [](int arg) { return arg * 3; };
    ASSERT_EQ(container.invoke(std::move(fn)), 12);
}
#endif

#if defined(__cpp_lib_copyable_function)
TYPED_TEST(invoke_test, invoke_copyable_function) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<external>, storage<int>>(5);

    std::copyable_function<int(int)> fn = [](int arg) { return arg * 4; };
    ASSERT_EQ(container.invoke(fn), 20);
}
#endif

#if DINGO_CXX_STANDARD >= 20
TYPED_TEST(invoke_test, invoke_explicit_signature_generic_lambda) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<external>, storage<int>>(6);

    auto fn = []<typename T>(T arg) { return arg * 2; };

    ASSERT_EQ((container.template invoke<int(int)>(fn)), 12);
}

#endif

TYPED_TEST(invoke_test, invoke_explicit_signature_overloaded_functor) {
    using container_type = TypeParam;

    struct overloaded {
        int operator()(int arg) const { return arg + 10; }
        float operator()(float arg) const { return arg + 100.0f; }
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(7);
    container.template register_type<scope<external>, storage<float>>(1.5f);

    ASSERT_EQ((container.template invoke<int(int)>(overloaded{})), 17);
    ASSERT_FLOAT_EQ((container.template invoke<float(float)>(overloaded{})),
                    101.5f);
}

TYPED_TEST(invoke_test, invoke_explicit_signature_missing_dependency_reports_signature) {
    using container_type = TypeParam;

    struct Missing {
        explicit Missing(int) {}
    };

    struct overloaded_missing {
        void operator()(Missing&) const {}
        void operator()(int) const {}
    };

    container_type container;

    try {
        container.template invoke<void(Missing&)>(overloaded_missing{});
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string expected = "type not found: ";
        expected += type_name<Missing&>();
        expected += " (required by ";
        expected += type_name<overloaded_missing>();
        expected += ")";

        ASSERT_STREQ(e.what(), expected.c_str());
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

    struct functor {
        void operator()(Missing&) const {}
    };

    container_type container;

    try {
        container.invoke(functor{});
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string expected = "type not found: ";
        expected += type_name<Missing&>();
        expected += " (required by ";
        expected += type_name<functor>();
        expected += ")";

        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

}
