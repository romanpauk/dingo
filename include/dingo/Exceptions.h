#pragma once

#include <exception>

namespace dingo
{
    struct TypeNotFoundException : std::exception {};
    struct TypeNotConvertibleException : std::exception {};
    struct TypeAmbiguousException : std::exception {};
    struct TypeRecursionException : std::exception {};

#ifdef _DEBUG
    struct TypeAllocateFailedException : std::exception {};
#endif
}