//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/type/type_name.h>

#include <gtest/gtest.h>

#include <string>

namespace dingo {

TEST(dingo_exception_test, make_indexed_type_not_found_exception_context) {
    struct Missing {};
    struct Consumer {};
    struct context {
        bool has_type_path() const { return true; }
        const type_descriptor* active_type() const { return &type; }
        void append_type_path(std::string& message) const {
            message += type_name<Consumer>();
        }
        type_descriptor type = describe_type<Consumer>();
    };

    auto e = detail::make_type_not_found_exception<Missing, int>(context{});
    std::string expected = "type not found: ";
    expected += type_name<Missing>();
    expected += " (index type: ";
    expected += type_name<int>();
    expected += ")";
    expected += " (required by ";
    expected += type_name<Consumer>();
    expected += ")";

    ASSERT_STREQ(e.what(), expected.c_str());
}

} // namespace dingo
