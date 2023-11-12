#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/index/map.h>
#include <dingo/index/unordered_map.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename IndexKey, typename IndexType>
struct dynamic_container_with_index {
    template <typename>
    using rebind_t = dynamic_container_with_static_rtti_traits;

    using tag_type = void;
    using rtti_type = dingo::static_rtti;
    template <typename Value, typename Allocator>
    using type_factory_map_type =
        dingo::dynamic_type_map<rtti_type, Value, Allocator>;
    using allocator_type = std::allocator<char>;
    using index_definition_type = std::tuple<std::tuple<IndexKey, IndexType>>;
};

using container_types = ::testing::Types<
    dingo::container<dingo::dynamic_container_with_index<int, index_type::map>>,
    dingo::container<
        dingo::dynamic_container_with_index<size_t, index_type::map>>,
    dingo::container<
        dingo::dynamic_container_with_index<std::string, index_type::map>>,
    dingo::container<
        dingo::dynamic_container_with_index<int, index_type::unordered_map>>,
    dingo::container<
        dingo::dynamic_container_with_index<size_t, index_type::map>>,
    dingo::container<
        dingo::dynamic_container_with_index<size_t, index_type::array<32>>>>;

template <typename T> struct index_test : public testing::Test {};
TYPED_TEST_SUITE(index_test, container_types);

// TODO: could this be useful in the container<>?
template <typename Container> struct get_index_type {
    using type = std::tuple_element_t<
        0, std::tuple_element_t<0, typename Container::index_definition_type>>;
};

template <typename Container>
using get_index_type_t = typename get_index_type<Container>::type;

template <typename T> static T value(size_t);
template <> int value<int>(size_t i) { return (int)i; }
template <> size_t value<size_t>(size_t i) { return i; }
template <> std::string value<std::string>(size_t i) {
    return std::to_string(i);
}

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

// TODO: does not work with unique_ptr<>
TYPED_TEST(index_test, register_indexed_type_unique) {
    using container_type = TypeParam;
    container_type container;
    using index_type = get_index_type_t<container_type>;

    struct tag {};
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Class<tag, 0>>>,
        interface<IClass>>(value<index_type>(0));
    container.template register_indexed_type<
        scope<unique>, storage<std::shared_ptr<Class<tag, 1>>>,
        interface<IClass>>(value<index_type>(1));

    ASSERT_EQ(
        container
            .template resolve<std::shared_ptr<IClass>>(value<index_type>(0))
            ->tag(),
        0);
    ASSERT_EQ(
        container
            .template resolve<std::shared_ptr<IClass>>(value<index_type>(1))
            ->tag(),
        1);
}

TYPED_TEST(index_test, register_indexed_type_shared) {
    using container_type = TypeParam;
    container_type container;
    using index_type = get_index_type_t<container_type>;

    struct tag {};
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Class<tag, 0>>>,
        interface<IClass>>(value<index_type>(0));
    container.template register_indexed_type<
        scope<shared>, storage<std::shared_ptr<Class<tag, 1>>>,
        interface<IClass>>(value<index_type>(1));

    ASSERT_EQ(container.template resolve<IClass&>(value<index_type>(0)).tag(),
              0);
    ASSERT_EQ(container.template resolve<IClass&>(value<index_type>(1)).tag(),
              1);
}

} // namespace dingo
