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
#include <dingo/type_list.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

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

    struct UniqueBase {
        UniqueBase() {}
        int value = 1;
    };

    struct Unique {
        Unique(UniqueBase& base): base_(base) {}
        UniqueBase& base_;
    };

    struct Shared {
        Shared(Unique& a): a_(a) {}
        Unique& a_;
    };

    container_type container;
    container.template register_type<scope<shared>, storage<Shared>>();
    container.template register_type<scope<unique>, storage<Unique>>();
    container.template register_type<scope<unique>, storage<UniqueBase>>();

    // TODO: We need to detect that we are returning dangling reference to unique instance
    // For top-level container.resolve(), the reference is removed and copy is returned.
    // For recursively instantiated types, the context type needs to be checked. If it is
    // shared scope, all is fine.
    auto& c = container.template resolve<Shared&>();
    ASSERT_EQ(c.a_.base_.value, 1);
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

#if 1
// TODO: consider allowing taking T& for unique_ptr<T> as we can take T& to unique instance anyways.
// But there is more to this test, get_address() that creates an unique instance in context
// is called after type_conversion, which needs an address to dereference.
TYPED_TEST(dingo_test, shared_unique_interface) {
    using container_type = TypeParam;

    struct UniqueBase {
        UniqueBase() {}
        int value = 1;
    };

    struct Unique: UniqueBase {
        Unique() {}
    };

    struct Shared {
        Shared(UniqueBase* a): a_(*a) {}
        UniqueBase& a_;
    };

    container_type container;
    container.template register_type<scope<shared>, storage<Shared>>();
    container.template register_type<scope<unique>, interfaces<UniqueBase>, storage<std::unique_ptr<Unique>>>();
    
    auto& c = container.template resolve<Shared&>();
    ASSERT_EQ(c.a_.value, 1);
}
#endif

} // namespace dingo
