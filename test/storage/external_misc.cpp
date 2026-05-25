//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "storage/external_common.h"

namespace dingo {

TYPED_TEST_SUITE(external_misc_test, container_types, );

TYPED_TEST(external_misc_test, shared_multiple) {
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

TYPED_TEST(external_misc_test, constructor_ambiguous) {
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

TYPED_TEST(external_misc_test, constructor_private) {
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

TYPED_TEST(external_misc_test, vector) {
    using container_type = TypeParam;

    struct vector_class_ctor {
        std::vector<int> data;
        vector_class_ctor(std::vector<int> v) : data(v) {}
    };

    struct vector_class_aggregate {
        std::vector<int> data;
    };

    std::vector<int> vec = {1, 2, 3};

    container_type container;
    container.template register_type<dingo::scope<dingo::external>,
                                     dingo::storage<std::vector<int>>>(vec);
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<vector_class_ctor>>();
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<vector_class_aggregate>>();

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
