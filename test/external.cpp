#include <dingo/container.h>
#include <dingo/storage/external.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct external_test : public testing::Test {};
TYPED_TEST_SUITE(external_test, container_types);

TYPED_TEST(external_test, value) {
    using container_type = TypeParam;

    struct external_value {};
    typedef Class<external_value, __COUNTER__> C;
    C c;

    {
        container_type container;
        container.template register_type<scope<external>, storage<C>, interface<C, IClass>>(std::move(c));

        AssertClass(*container.template resolve<C*>());
        AssertClass(container.template resolve<C&>());
        AssertClass(container.template resolve<IClass&>());
    }
}

TYPED_TEST(external_test, ref) {
    using container_type = TypeParam;

    struct external_ref {};
    typedef Class<external_ref, __COUNTER__> C;
    C c;

    {
        container_type container;
        container.template register_type<scope<external>, storage<C&>, interface<C, IClass>>(c);

        ASSERT_EQ(container.template resolve<C*>(), &c);
        AssertClass(*container.template resolve<C*>());
        AssertClass(container.template resolve<C&>());
        AssertClass(container.template resolve<IClass&>());
    }
}

TYPED_TEST(external_test, ptr) {
    using container_type = TypeParam;

    struct external_ptr {};
    typedef Class<external_ptr, __COUNTER__> C;
    C c;

    {
        container_type container;
        container.template register_type<scope<external>, storage<C*>, interface<C, IClass>>(&c);

        ASSERT_EQ(container.template resolve<C*>(), &c);
        AssertClass(*container.template resolve<C*>());
        AssertClass(container.template resolve<C&>());
        AssertClass(container.template resolve<IClass&>());
    }
}

TYPED_TEST(external_test, shared_ptr) {
    using container_type = TypeParam;

    struct external_shared_ptr_lvalue {};
    typedef Class<external_shared_ptr_lvalue, __COUNTER__> C;
    auto c = std::make_shared<C>();

    {
        container_type container;
        container.template register_type<scope<external>, storage<std::shared_ptr<C>>, interface<C, IClass>>(c);
        AssertClass(container.template resolve<C&>());
        AssertClass(*container.template resolve<C*>());
        AssertClass(*container.template resolve<std::shared_ptr<C>>());
        AssertClass(*container.template resolve<std::shared_ptr<C>&>());
        AssertClass(container.template resolve<IClass&>());
        AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
    }
}

TYPED_TEST(external_test, shared_ptr_ref) {
    using container_type = TypeParam;

    struct external_shared_ptr_rvalue {};
    typedef Class<external_shared_ptr_rvalue, __COUNTER__> C;
    auto c = std::make_shared<C>();

    {
        container_type container;
        container.template register_type<scope<external>, storage<std::shared_ptr<C>&>, interface<C, IClass>>(c);
        container.template resolve<C&>();
        container.template resolve<C*>();
        container.template resolve<std::shared_ptr<C>>();
        container.template resolve<std::shared_ptr<C>&>();

        // container.resolve< IClass& >();
    }
}

TYPED_TEST(external_test, unique_ptr_ref) {
    using container_type = TypeParam;

    struct external_unique_ptr_lvalue {};
    typedef Class<external_unique_ptr_lvalue, __COUNTER__> C;
    auto c = std::make_unique<C>();

    {
        container_type container;
        container.template register_type<scope<external>, storage<std::unique_ptr<C>&>>(c);
        container.template resolve<C&>();
        container.template resolve<C*>();
        container.template resolve<std::unique_ptr<C>&>();
    }
}

TYPED_TEST(external_test, unique_ptr_move) {
    using container_type = TypeParam;

    struct external_unique_ptr_rvalue {};
    typedef Class<external_unique_ptr_rvalue, __COUNTER__> C;
    auto c = std::make_unique<C>();

    {
        container_type container;
        container.template register_type<scope<external>, storage<std::unique_ptr<C>>>(std::move(c));
        container.template resolve<C&>();
        container.template resolve<C*>();
        container.template resolve<std::unique_ptr<C>&>();
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

    container.template register_type<scope<external>, storage<A>>(A::instance());
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

    container.template register_type<scope<external>, storage<A>>(A::instance());
}

} // namespace dingo
