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
        auto processors = container.template resolve<
            std::vector<std::shared_ptr<processor_interface>>>();
        std::vector<int> ids;
        ids.reserve(processors.size());
        for (auto& processor : processors) {
            ids.push_back(processor->id());
        }
        std::sort(ids.begin(), ids.end());

        ASSERT_EQ(processors.size(), 2u);
        ASSERT_EQ(ids[0], 0);
        ASSERT_EQ(ids[1], 1);
    });
}

} // namespace dingo::matrix
