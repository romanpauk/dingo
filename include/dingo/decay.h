//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>
#include <dingo/type_traits.h>

#include <type_traits>

namespace dingo {
// TODO: this has misleading name
template <class T, class = void> struct decay : std::decay<T> {};

template <class T> struct decay<const T> : decay<T> {};
template <class T> struct decay<T*> : decay<T> {};
template <class T> struct decay<const T*> : decay<T> {};
template <class T> struct decay<T&> : decay<T> {};
template <class T> struct decay<const T&> : decay<T> {};
template <class T> struct decay<T&&> : decay<T> {};

template <class T>
struct decay<T, std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>>
    : decay<typename type_traits<T>::value_type> {};

template <class T, class Tag>
struct decay<annotated<T, Tag>, void>
    : std::decay<annotated<typename decay<T>::type, Tag>> {};

template <class T> using decay_t = typename decay<T>::type;
} // namespace dingo
