//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/factory/constructor.h of the next implementation: the constructor
// factory, which constructs the leaf by its detected constructor, or by the one
// of the declared argument types.

#include <dingo/core/projection/construct.h>
#include <dingo/registration.h>
#include <dingo/registration/constructor.h> // IWYU pragma: export

namespace dingo {

template <typename Type, typename... Args> struct constructor<Type(Args...)> {};

template <typename Type> struct constructor<Type> {};

} // namespace dingo

namespace dingo::core {

template <typename Type, typename... Args>
struct factory_of<::dingo::constructor<Type(Args...)>> {
  using type = constructor_factory<declared_leaf<Args...>, Type>;
};

template <typename Type> struct factory_of<::dingo::constructor<Type>> {
  using type = constructor_factory<detected_leaf, Type>;
};

} // namespace dingo::core
