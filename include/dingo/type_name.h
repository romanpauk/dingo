//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type_descriptor.h>

#include <string>

namespace dingo {

template <typename T> std::string type_name() {
    std::string name;
    append_type_name(name, describe_type<T>());
    return name;
}

} // namespace dingo
