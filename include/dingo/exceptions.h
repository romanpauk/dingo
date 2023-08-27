#pragma once

#include <exception>

namespace dingo {
struct exception : std::exception {};

struct type_not_found_exception : exception {};
struct type_not_convertible_exception : exception {};
struct type_ambiguous_exception : exception {};
struct type_recursion_exception : exception {};
struct type_not_constructed_exception : exception {};
struct type_already_registered_exception : exception {};
struct type_overflow_exception : exception {};

struct virtual_pointer_exception : exception {};

#ifdef _DEBUG
struct arena_allocation_exception : exception {};
#endif
} // namespace dingo