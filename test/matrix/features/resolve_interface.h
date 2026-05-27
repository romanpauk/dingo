//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/support/types.h"

#include <gtest/gtest.h>

#include <memory>

namespace dingo::matrix {

template <typename Case> void run_resolve_interface() {
    Case::with_container([](auto& container) {
        interface_type& instance =
            container.template resolve<interface_type&>();
        ASSERT_EQ(instance.marker(), 3);

        interface_type* pointer = container.template resolve<interface_type*>();
        ASSERT_EQ(pointer->marker(), 3);

        auto& handle =
            container.template resolve<std::shared_ptr<interface_type>&>();
        ASSERT_EQ(handle->marker(), 3);
    });
}

} // namespace dingo::matrix
