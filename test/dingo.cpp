#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type_list.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct dingo_test : public testing::Test {};
TYPED_TEST_SUITE(dingo_test, container_types);

TYPED_TEST(dingo_test, unique_hierarchy) {
    using container_type = TypeParam;

    struct S {};
    struct U {};
    struct B {
        B(std::shared_ptr<S>&&) {}
    };

    container_type container;
    container.template register_binding<storage<unique, std::shared_ptr<S>>>();
    container.template register_binding<storage<unique, std::unique_ptr<U>>>();
    container.template register_binding<storage<shared, B>>();

    container.template resolve<B&>();
}

TYPED_TEST(dingo_test, resolve_rollback) {
    using container_type = TypeParam;

    struct resolve_rollback {};
    typedef Class<resolve_rollback, __COUNTER__> A;
    typedef Class<resolve_rollback, __COUNTER__> B;
    struct Ex {};
    struct C {
        C(A&, B&) { throw Ex(); }
    };

    container_type container;
    container.template register_binding<storage<shared, A>>();
    container.template register_binding<storage<shared, B>>();
    container.template register_binding<storage<shared, C>>();

    ASSERT_THROW(container.template resolve<C&>(), Ex);
    ASSERT_EQ(A::Constructor, 1);
    ASSERT_EQ(A::Destructor, 1);
    ASSERT_EQ(B::Constructor, 1);
    ASSERT_EQ(B::Destructor, 1);

    container.template resolve<A&>();
    ASSERT_EQ(A::Constructor, 2);
    ASSERT_EQ(A::Destructor, 1);
    ASSERT_THROW(container.template resolve<C>(), Ex);
    ASSERT_EQ(A::Constructor, 2);
    ASSERT_EQ(A::Destructor, 1);
    ASSERT_EQ(B::Constructor, 2);
    ASSERT_EQ(B::Destructor, 2);
}

TYPED_TEST(dingo_test, type_already_registered) {
    using container_type = TypeParam;

    struct type_already_registered {};
    typedef Class<type_already_registered, __COUNTER__> A;

    container_type container;
    {
        container.template register_binding<storage<shared, A>>();
        auto reg = [&] { container.template register_binding<storage<shared, A>>(); };
        ASSERT_THROW(reg(), dingo::type_already_registered_exception);
    }
    {
        container.template register_binding<storage<shared, A>, IClass>();
        auto reg = [&] { container.template register_binding<storage<shared, A>, IClass>(); };
        ASSERT_THROW(reg(), dingo::type_already_registered_exception);
    }
}
} // namespace dingo