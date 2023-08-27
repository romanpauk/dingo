#include <dingo/annotated.h>
#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/type_list.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct annotated_test : public testing::Test {};
TYPED_TEST_SUITE(annotated_test, container_types);

template <size_t N> struct tag {};

/*
TYPED_TEST(annotated_test, value) {
    using container_type = TypeParam;

    struct A {
        A(annotated< int, tag< 1 > > value1, annotated< int, tag< 2 > > value10)
{}
    };

    // TODO: support constant / lambda factory
    container_type container;
    container.template register_binding< storage< container_type, unique,
annotated< int, tag< 1 > > > >(1); container.template register_binding< storage<
container_type, unique, annotated< int, tag< 2 > > > >([]{ return 10; });
    container.template register_binding< storage< container_type, unique, A >
>();

    container.template resolve< annotated< int, tag< 1 > > >();
    container.template resolve< annotated< int, tag< 2 > > >();
    container.template resolve< A >();
}
*/

TYPED_TEST(annotated_test, interface) {
    using container_type = TypeParam;

    struct I {
        virtual ~I() {}
    };

    struct A : I {};
    struct B : I {};

    struct C : I {
        C(annotated<I&, tag<1>> ref, annotated<I*, tag<1>> ptr, annotated<std::shared_ptr<I>, tag<2>> sharedptr)
            : ref_(ref), ptr_(ptr), sharedptr_(sharedptr) {
            EXPECT_TRUE(dynamic_cast<A*>(&ref_));
            EXPECT_TRUE(dynamic_cast<A*>(ptr_));
            EXPECT_TRUE(dynamic_cast<B*>(sharedptr_.get()));
        }

        I& ref_;
        I* ptr_;
        std::shared_ptr<I> sharedptr_;
    };

    container_type container;

    container.template register_binding<storage<shared, A>, annotated<I, tag<1>>>();
    container.template register_binding<storage<shared, std::shared_ptr<B>>, annotated<I, tag<2>>>();
    container.template register_binding<storage<shared, std::shared_ptr<C>>, C, annotated<I, tag<3>>>();

    I& aref = container.template resolve<annotated<I&, tag<1>>>();
    ASSERT_TRUE(dynamic_cast<A*>(&aref));

    I* bptr = container.template resolve<annotated<I*, tag<2>>>();
    ASSERT_TRUE(dynamic_cast<B*>(bptr));

    std::shared_ptr<I> bsharedptr = container.template resolve<annotated<std::shared_ptr<I>, tag<2>>>();
    ASSERT_TRUE(dynamic_cast<B*>(bsharedptr.get()));

    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<C&>()));
    I& cref = container.template resolve<annotated<I&, tag<3>>>();
    ASSERT_TRUE(dynamic_cast<C*>(&cref));
}
} // namespace dingo