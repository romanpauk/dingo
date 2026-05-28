//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/values.h"

#include <memory>
#include <vector>

namespace dingo::matrix {

struct local_dependency_type {
    local_dependency_type() : value(4) {}

    int value;
};

struct local_value_type {
    local_value_type(local_dependency_type& local, value_type& host)
        : value(local.value + host.marker() + 5) {}

    int value;
};

struct local_override_value_type {
    explicit local_override_value_type(local_dependency_type& dependency)
        : value(dependency.value) {}

    int value;
};

struct local_collection_value_type {
    explicit local_collection_value_type(
        std::vector<std::shared_ptr<element_interface>> elements)
        : count(elements.size()), sum(0) {
        for (const auto& element : elements) {
            sum += element->id();
        }
    }

    std::size_t count;
    int sum;
};

} // namespace dingo::matrix
