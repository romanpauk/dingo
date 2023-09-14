#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct multibindings_test : public testing::Test {};
TYPED_TEST_SUITE(multibindings_test, container_types);

TYPED_TEST(multibindings_test, multiple_interfaces_shared_value) {
    using container_type = TypeParam;

    struct multiple_interfaces_shared_value {};
    typedef Class<multiple_interfaces_shared_value, __COUNTER__> C;

    container_type container;
    container.template register_binding<storage<shared, C>, IClass, IClass1, IClass2>();

    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass&>()));
    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<IClass2*>()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_shared_cyclical_value) {
    using container_type = TypeParam;

    struct multiple_interfaces_shared_cyclical_value {};
    typedef Class<multiple_interfaces_shared_cyclical_value, __COUNTER__> C;

    container_type container;
    container.template register_binding<storage<shared_cyclical, C, class_factory<C>, container_type>,
                                        /*IClass, */ IClass1, IClass2>();

    // TODO: virtual base issues
    // BOOST_TEST(dynamic_cast<C*>(&container.resolve< IClass& >()));
    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<IClass2*>()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_shared_shared_ptr) {
    using container_type = TypeParam;

    struct multiple_interfaces_shared_shared_ptr {};
    typedef Class<multiple_interfaces_shared_shared_ptr, __COUNTER__> C;

    container_type container;
    container.template register_binding<storage<shared, std::shared_ptr<C>>, IClass, IClass1, IClass2>();

    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass&>()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass>&>().get()));
    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass1>&>().get()));
    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass2&>()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass2>*>()->get()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_shared_cyclical_shared_ptr) {
    using container_type = TypeParam;

    struct multiple_interfaces_shared_cyclical_shared_ptr {};
    typedef Class<multiple_interfaces_shared_cyclical_shared_ptr, __COUNTER__> C;

    container_type container;
    container.template register_binding<storage<shared_cyclical, std::shared_ptr<C>, class_factory<C>, container_type>,
                                        IClass1, IClass2>();

    // TODO: virtual base issues with cyclical_shared
    // BOOST_TEST(dynamic_cast<C*>(&container.resolve< IClass& >()));
    // BOOST_TEST(dynamic_cast<C*>(container.resolve< std::shared_ptr< IClass >&
    // >().get()));
    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass1&>()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass1>&>().get()));
    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<IClass2&>()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass2>*>()->get()));
}

/*
// TODO: does not compile
TYPED_TEST(multibindings_test, multiple_interfaces_shared_unique_ptr) {
    using container_type = TypeParam;

    struct multiple_interfaces_shared_unique_ptr {};
    typedef Class< multiple_interfaces_shared_unique_ptr, __COUNTER__ > C;

    container_type container;
    container.template register_binding< storage< container_type, shared,
std::unique_ptr< C > >, IClass, IClass1, IClass2 >();
}
*/

TYPED_TEST(multibindings_test, multiple_interfaces_unique_shared_ptr) {
    using container_type = TypeParam;

    struct multiple_interfaces_unique_shared_ptr {};
    typedef Class<multiple_interfaces_unique_shared_ptr, __COUNTER__> C;

    container_type container;
    container.template register_binding<storage<unique, std::shared_ptr<C>>, IClass, IClass1, IClass2>();

    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass>>().get()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass1>>().get()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::shared_ptr<IClass2>>().get()));
}

TYPED_TEST(multibindings_test, multiple_interfaces_unique_unique_ptr) {
    using container_type = TypeParam;

    struct multiple_interfaces_unique_unique_ptr {};
    typedef Class<multiple_interfaces_unique_unique_ptr, __COUNTER__> C;

    container_type container;
    container.template register_binding<storage<unique, std::unique_ptr<C>>, IClass, IClass1, IClass2>();

    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::unique_ptr<IClass>>().get()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::unique_ptr<IClass1>>().get()));
    ASSERT_TRUE(dynamic_cast<C*>(container.template resolve<std::unique_ptr<IClass2>>().get()));
}

// TODO: this is not correctly implemented, it leaks memory, also it does not
// compile now
#if 0
    TYPED_TEST(multibindings_test, resolve_multiple)
    {
        using container_type = TypeParam;

        struct resolve_multiple {};
        struct I {};
        struct A : I {};
        struct B : I {};
        struct C
        {
            C(std::vector< I* > v, std::list< I* > l, std::set< I* > s) : v_(v), l_(l), s_(s) {}

            std::vector< I* > v_;
            std::list< I* > l_;
            std::set< I* > s_;
        };

        container_type container;
        container.template register_binding< storage< dingo::container<>, shared, A >, A, I >();
        container.template register_binding< storage< dingo::container<>, shared, B >, B, I >();
        container.template register_binding< storage< dingo::container<>, shared, C > >();

        {
            auto vector = container.template resolve< std::vector< I* > >();
            ASSERT_EQ(vector.size(), 2);

            auto list = container.template resolve< std::list< I* > >();
            ASSERT_EQ(list.size(), 2);

            auto set = container.template resolve< std::set< I* > >();
            ASSERT_EQ(set.size(), 2);
        }

        {
            ASSERT_THROW(container.template resolve< std::vector< std::shared_ptr< I > > >(), type_not_convertible_exception);
        }

        {
            auto& c = container.template resolve< C& >();
            ASSERT_EQ(c.v_.size(), 2);
            ASSERT_EQ(c.l_.size(), 2);
            ASSERT_EQ(c.s_.size(), 2);
        }
    }
#endif
} // namespace dingo
