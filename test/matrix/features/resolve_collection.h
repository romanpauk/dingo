//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/types.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace dingo::matrix {

template <typename Case> void run_resolve_collection() {
    Case::with_container([](auto& container) {
        auto elements = container.template resolve<
            std::vector<std::shared_ptr<element_interface>>>();
        std::vector<int> ids;
        ids.reserve(elements.size());
        for (auto& element : elements) {
            ids.push_back(element->id());
        }
        std::sort(ids.begin(), ids.end());

        ASSERT_EQ(elements.size(), 2u);
        ASSERT_EQ(ids[0], 0);
        ASSERT_EQ(ids[1], 1);
    });
}

} // namespace dingo::matrix
