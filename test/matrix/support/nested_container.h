//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

namespace dingo::matrix {

struct nested_value_type {
    explicit nested_value_type(int dependency) : value(dependency) {}

    int value;
};

} // namespace dingo::matrix
