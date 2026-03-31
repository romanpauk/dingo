//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/index/map.h>
#include <dingo/index/unordered_map.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <string>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {
template <typename IndexKey, typename IndexType>
struct dynamic_container_with_index {
    template <typename>
    using rebind_t = dynamic_container_with_index;

    using tag_type = void;
    using rtti_type = dingo::rtti<dingo::static_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = dingo::dynamic_type_map<Value, rtti_type, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type =
        dingo::dynamic_type_cache<Value, rtti_type, Allocator>;
    using allocator_type = std::allocator<char>;
    using index_definition_type = std::tuple<std::tuple<IndexKey, IndexType>>;
    static constexpr bool cache_enabled = true;
};

using container_types = ::testing::Types<
    dingo::container<dingo::dynamic_container_with_index<int, index_type::map>>>;

template <typename T> struct index_test : public test<T> {};
TYPED_TEST_SUITE(index_test, container_types, );

// TODO: could this be useful in the container<>?
template <typename Container> struct get_index_type {
    using type = std::tuple_element_t<
        0, std::tuple_element_t<0, typename Container::index_definition_type>>;
};

template <typename Container>
using get_index_type_t = typename get_index_type<Container>::type;

template <typename T> static T value(int);
template <> int value<int>(int i) { return i; }

TYPED_TEST(index_test, index_tag) {
    using indexes =
        std::tuple<std::tuple<short, float>, std::tuple<int, double>,
                   std::tuple<size_t, char>>;
    static_assert(
        std::is_same_v<typename index_tag<int, indexes>::type, double>);
    static_assert(
        std::is_same_v<typename index_tag<short, indexes>::type, float>);
    static_assert(
        std::is_same_v<typename index_tag<size_t, indexes>::type, char>);
}

TYPED_TEST(index_test, register_indexed_type_unique) {
    using container_type = TypeParam;
    container_type container;
    using index_type = get_index_type_t<container_type>;

    container.template register_indexed_type<
        scope<unique>, storage<std::unique_ptr<ClassTag<0>>>,
        interfaces<IClass>>(value<index_type>(0));
    container.template register_indexed_type<
        scope<unique>, storage<std::unique_ptr<ClassTag<1>>>,
        interfaces<IClass>>(value<index_type>(1));
    ASSERT_THROW((container.template register_indexed_type<
                     scope<unique>, storage<std::unique_ptr<ClassTag<1>>>,
                     interfaces<IClass>>(value<index_type>(1))),
                 type_already_registered_exception);

    ASSERT_EQ(
        container
            .template resolve<std::shared_ptr<IClass>>(value<index_type>(0))
            ->GetTag(),
        0);
    ASSERT_EQ(
        container
            .template resolve<std::shared_ptr<IClass>>(value<index_type>(1))
            ->GetTag(),
        1);
    ASSERT_THROW(container.template resolve<std::shared_ptr<IClass>>(
                     value<index_type>(-1)),
                 type_not_found_exception);
}

TYPED_TEST(index_test, register_indexed_type_shared) {
    using container_type = TypeParam;
    container_type container;
    using index_type = get_index_type_t<container_type>;

    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<ClassTag<0>>>, interfaces<IClass>>(
        value<index_type>(0));
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<ClassTag<1>>>, interfaces<IClass>>(
        value<index_type>(1));
    ASSERT_THROW((container.template register_indexed_type<
                     scope<shared>, storage<std::shared_ptr<ClassTag<1>>>,
                     interfaces<IClass>>(value<index_type>(1))),
                 type_already_registered_exception);

    ASSERT_EQ(
        container.template resolve<IClass&>(value<index_type>(0)).GetTag(), 0);
    ASSERT_EQ(
        container.template resolve<IClass&>(value<index_type>(1)).GetTag(), 1);
    ASSERT_THROW(container.template resolve<IClass&>(value<index_type>(-1)),
                 type_not_found_exception);
}

TYPED_TEST(index_test, resolve_indexed_type_with_lvalue_key) {
    using container_type = TypeParam;
    container_type container;
    using index_type = get_index_type_t<container_type>;

    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<ClassTag<0>>>, interfaces<IClass>>(
        value<index_type>(0));

    index_type key = value<index_type>(0);
    ASSERT_EQ(container.template resolve<IClass&>(key).GetTag(), 0);
}

TYPED_TEST(index_test, exception_message_indexed_type_not_found) {
    using container_type = TypeParam;
    using index_type = get_index_type_t<container_type>;

    container_type container;
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<ClassTag<0>>>, interfaces<IClass>>(
        value<index_type>(0));

    try {
        (void)container.template resolve<IClass&>(value<index_type>(1));
        FAIL() << "expected type_not_found_exception";
    } catch (const type_not_found_exception& e) {
        std::string message = e.what();
        std::string expected = "type not found: ";
        expected += type_name<IClass&>();
        expected += " (index type: ";
        expected += type_name<index_type>();
        expected += ", index value 1)";

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("lookup plan: "), std::string::npos);
        EXPECT_NE(message.find("lookup indexed"), std::string::npos);
        EXPECT_NE(message.find("candidates:"), std::string::npos);
        EXPECT_NE(message.find("registered plans:"), std::string::npos);
        EXPECT_NE(message.find(std::string("index key ") + type_name<index_type>()),
                  std::string::npos);
        EXPECT_NE(message.find("payload default_constructed"),
                  std::string::npos);
        EXPECT_NE(message.find("storage shared"), std::string::npos);
        EXPECT_NE(message.find(type_name<ClassTag<0>>()), std::string::npos);
    }
}

TYPED_TEST(index_test, exception_message_indexed_type_already_registered) {
    using container_type = TypeParam;
    using index_type = get_index_type_t<container_type>;

    container_type container;
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<ClassTag<0>>>, interfaces<IClass>>(
        value<index_type>(0));

    try {
        container.template register_indexed_type<
            scope<shared>, storage<std::shared_ptr<ClassTag<1>>>,
            interfaces<IClass>>(value<index_type>(0));
        FAIL() << "expected type_index_already_registered_exception";
    } catch (const type_index_already_registered_exception& e) {
        std::string message = e.what();
        std::string expected = "type index already registered: interface ";
        expected += type_name<IClass>();
        expected += ", storage ";
        expected += type_name<std::shared_ptr<ClassTag<1>>>();
        expected += ", index type ";
        expected += type_name<index_type>();

        EXPECT_NE(message.find(expected), std::string::npos);
        EXPECT_NE(message.find("registration plan: "), std::string::npos);
        EXPECT_NE(message.find(std::string("interfaces [") + type_name<IClass>() +
                                   "]"),
                  std::string::npos);
        EXPECT_NE(message.find(std::string("index key ") + type_name<index_type>()),
                  std::string::npos);
        EXPECT_NE(
            message.find(std::string("registered storage ") +
                         type_name<std::shared_ptr<ClassTag<1>>>()),
            std::string::npos);
        EXPECT_NE(message.find("payload default_constructed"),
                  std::string::npos);
        EXPECT_NE(message.find("materialization single_interface"),
                  std::string::npos);
    }
}

TEST(index_test, exception_message_index_out_of_range) {
    using container_type =
        container<dynamic_container_with_index<size_t, index_type::array<2>>>;

    container_type container;

    try {
        container.template register_indexed_type<
            scope<shared>, storage<std::shared_ptr<Class>>, interfaces<IClass>>(
            size_t(3));
        FAIL() << "expected type_index_out_of_range_exception";
    } catch (const type_index_out_of_range_exception& e) {
        std::string expected = "type index out of range: key type ";
        expected += type_name<size_t>();
        expected += ", value 3, bound 2";
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TEST(index_test, exception_message_index_out_of_range_negative) {
    auto e = detail::make_type_index_out_of_range_exception(-1, size_t(2));

    std::string expected = "type index out of range: key type ";
    expected += type_name<int>();
    expected += ", value -1, bound 2";
    ASSERT_STREQ(e.what(), expected.c_str());
}

} // namespace dingo
