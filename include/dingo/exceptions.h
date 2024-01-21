//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <exception>

namespace dingo {
struct exception : std::exception {};

struct type_not_found_exception : exception {};
struct type_not_convertible_exception : exception {};
struct type_ambiguous_exception : exception {};
struct type_recursion_exception : exception {};
struct type_not_constructed_exception : exception {};
struct type_already_registered_exception : exception {};
struct type_index_already_registered_exception : exception {};
struct type_index_out_of_range_exception : exception {};
struct type_context_overflow_exception : exception {};

struct virtual_pointer_exception : exception {};

#ifdef _DEBUG
struct arena_allocation_exception : exception {};
#endif
} // namespace dingo