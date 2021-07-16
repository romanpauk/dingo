#pragma once

#include <exception>

namespace dingo
{
    struct type_not_found_exception : std::exception {};
    struct type_not_convertible_exception : std::exception {};
    struct type_ambiguous_exception : std::exception {};
    struct type_recursion_exception : std::exception {};
    struct type_not_constructed_exception : std::exception {};

    struct virtual_pointer_exception : std::exception {};

#ifdef _DEBUG
    struct arena_allocation_exception : std::exception {};
#endif
}