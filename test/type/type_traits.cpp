//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/registration/collection_traits.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/type_traits.h>

#include <gtest/gtest.h>

#include <map>
#include <vector>

#include "support/class.h"
#include "support/custom_wrappers.h"

namespace dingo {
static_assert(
    !std::is_copy_constructible_v<detail::recursion_guard<int, false>>);
static_assert(
    !std::is_move_constructible_v<detail::recursion_guard<int, false>>);

static_assert(is_interface_storage_rebindable_v<test_shared<Class>, IClass>);
static_assert(is_interface_storage_rebindable_v<test_unique<Class>, IClass>);
static_assert(is_copy_constructible_v<test_shared<Class>>);
static_assert(!is_copy_constructible_v<test_unique<Class>>);
static_assert(is_copy_constructible_v<test_optional<Class>>);
static_assert(is_copy_constructible_v<std::vector<test_shared<Class>>>);
static_assert(!is_copy_constructible_v<std::vector<test_unique<Class>>>);
static_assert(!is_copy_constructible_v<std::map<int, std::unique_ptr<Class>>>);
} // namespace dingo
