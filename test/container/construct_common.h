//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/runtime/injector.h>
#include <dingo/runtime/registry.h>
#include <dingo/runtime_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <tuple>
#include <variant>
#include <vector>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {

template <typename T> struct construct_test : public test<T> {};
template <typename T> struct construct_local_test : public test<T> {};
template <typename T> struct construct_dependency_test : public test<T> {};

} // namespace dingo
