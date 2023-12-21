//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <gtest/gtest.h>

#include "class.h"

namespace dingo {

template <typename T> struct test : public testing::Test {
    virtual void SetUp() {
        ClassTag<0>::ClearStats();
        ClassTag<1>::ClearStats();
        ClassTag<2>::ClearStats();
    }

    virtual void TearDown() {}
};
} // namespace dingo
