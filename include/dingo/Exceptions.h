#pragma once

#include <exception>

namespace dingo
{
	struct TypeNotFoundException : std::exception {};
	struct TypeNotConvertibleException : std::exception {};
	struct TypeAmbiguousException : std::exception {};

}