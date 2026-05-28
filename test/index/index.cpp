//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/index/map.h>
#include <dingo/storage/shared.h>
#include <dingo/type/type_name.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

namespace dingo {

template <typename IndexKey, typename IndexType>
struct dynamic_container_with_index : dynamic_container_traits {
    using index_definition_type = std::tuple<std::tuple<IndexKey, IndexType>>;
};

TEST(index_test, index_tag) {
    using indexes =
        std::tuple<std::tuple<short, float>, std::tuple<int, double>,
                   std::tuple<std::size_t, char>>;
    static_assert(
        std::is_same_v<typename index_tag<int, indexes>::type, double>);
    static_assert(
        std::is_same_v<typename index_tag<short, indexes>::type, float>);
    static_assert(
        std::is_same_v<typename index_tag<std::size_t, indexes>::type, char>);
}

TEST(index_test, exception_message_index_out_of_range) {
    struct indexed_interface {
        virtual ~indexed_interface() = default;
    };
    struct indexed_class : indexed_interface {};

    using container_type = container<
        dynamic_container_with_index<std::size_t, index_type::array<2>>>;

    container_type container;

    try {
        container.template register_indexed_type<
            scope<shared>, storage<std::shared_ptr<indexed_class>>,
            interfaces<indexed_interface>>(std::size_t(3));
        FAIL() << "expected type_index_out_of_range_exception";
    } catch (const type_index_out_of_range_exception& e) {
        std::string expected = "type index out of range: key type ";
        expected += type_name<std::size_t>();
        ASSERT_STREQ(e.what(), expected.c_str());
    }
}

TEST(index_test, exception_message_index_out_of_range_negative) {
    auto e = detail::make_type_index_out_of_range_exception(-1, std::size_t(2));

    std::string expected = "type index out of range: key type ";
    expected += type_name<int>();
    ASSERT_STREQ(e.what(), expected.c_str());
}

} // namespace dingo
