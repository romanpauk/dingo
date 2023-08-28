#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type_list.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct cyclical_test : public testing::Test {};
TYPED_TEST_SUITE(cyclical_test, container_types);

TYPED_TEST(cyclical_test, recursion_exception) {
    using container_type = TypeParam;

    struct B;
    struct A {
        A(B&) {}
    };
    struct B {
        B(std::shared_ptr<A>) {}
    };

    container_type container;
    container.template register_binding<storage<shared, std::shared_ptr<A>>>();
    container.template register_binding<storage<shared, B>>();

    ASSERT_THROW(container.template resolve<A>(), type_recursion_exception);
    ASSERT_THROW(container.template resolve<B>(), type_recursion_exception);
}

TYPED_TEST(cyclical_test, shared_cyclical_value) {
    using container_type = TypeParam;

    struct B;
    struct shared_cyclical_value {};
    struct A : Class<shared_cyclical_value, __COUNTER__> {
        A(B& b) : b_(b) {}
        B& b_;
    };

    struct B : Class<shared_cyclical_value, __COUNTER__> {
        B(A& a, IClass& ia) : a_(a), ia_(ia) {}
        A& a_;
        IClass& ia_;
    };

    container_type container;
    container.template register_binding<storage<shared_cyclical, A, container_type>, A, IClass>();
    container.template register_binding<storage<shared_cyclical, B, container_type>>();

    auto& a = container.template resolve<A&>();
    AssertClass(a);
    AssertClass(a.b_);

    auto& b = container.template resolve<B&>();
    AssertClass(b);
    AssertClass(b.a_);
    AssertClass(b.ia_);

    auto& c = container.template resolve<IClass&>();
    AssertClass(c);
}

TYPED_TEST(cyclical_test, shared_cyclical_shared_ptr) {
    using container_type = TypeParam;

    struct B;
    struct shared_cyclical_shared_ptr {};
    struct A : Class<shared_cyclical_shared_ptr, __COUNTER__> {
        A(B& b, IClass1& ib, std::shared_ptr<IClass1>& ibptr) : b_(b), ib_(ib), ibptr_(ibptr) {}
        B& b_;
        IClass1& ib_;
        std::shared_ptr<IClass1>& ibptr_;

        // Conversion to IClass& ib_; crashes during constructor as the object
        // is not healthy during construction.
        // TODO: could this be detected in runtime?
    };

    struct B : Class<shared_cyclical_shared_ptr, __COUNTER__> {
        B(A& a, IClass& ia, std::shared_ptr<IClass>& iaptr) : a_(a), ia_(ia), iaptr_(iaptr) {}
        A& a_;
        IClass& ia_;
        std::shared_ptr<IClass>& iaptr_;
    };

    container_type container;
    container.template register_binding<storage<shared_cyclical, std::shared_ptr<A>, container_type>, A, IClass>();
    container.template register_binding<storage<shared_cyclical, std::shared_ptr<B>, container_type>, B, IClass1>();

    // TODO: container.resolve< IClass& >();
    // Virtual base has some issues, the type needs to be constructed for
    // casting to work.

    auto& a = container.template resolve<A&>();
    AssertClass(a);
    AssertClass(a.b_);
    AssertClass(a.ib_);
    AssertClass(*a.ibptr_);

    auto& b = container.template resolve<B&>();
    AssertClass(b);
    AssertClass(b.a_);
    AssertClass(b.ia_);
    AssertClass(*b.iaptr_);
}
} // namespace dingo