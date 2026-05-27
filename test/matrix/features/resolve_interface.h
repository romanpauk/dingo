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
        service_interface& instance =
            container.template resolve<service_interface&>();
        ASSERT_EQ(instance.retries(), 3);

        service_interface* pointer =
            container.template resolve<service_interface*>();
        ASSERT_EQ(pointer->retries(), 3);

        auto& handle =
            container.template resolve<std::shared_ptr<service_interface>&>();
        ASSERT_EQ(handle->retries(), 3);
    });
}

} // namespace dingo::matrix
