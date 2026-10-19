//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>

namespace dingo {
template <typename... Registrations> struct static_bindings;
template <typename T> struct storage {
  using type = T;
  template <typename U> using rebind_t = storage<U>;
};

template <typename T> struct scope {
  using type = T;
  template <typename U> using rebind_t = scope<U>;
};

template <typename... Args> struct interfaces {
  using type = type_list<Args...>;

  template <typename U> using rebind_t = interfaces<U>;
};

template <typename... Args> struct interfaces<type_list<Args...>> {
  using type = type_list<Args...>;

  template <typename U> using rebind_t = interfaces<U>;
};

template <typename T> struct factory {
  using type = T;
  template <typename U> using rebind_t = factory<U>;
};

template <typename T> struct conversions {
  using type = T;
  template <typename U> using rebind_t = conversions<U>;
};

template <typename... Args> struct dependencies {
  using type = type_list<Args...>;
  template <typename U> using rebind_t = dependencies<U>;
};

template <typename... Args> struct bindings {
  using type = static_bindings<Args...>;
  template <typename U> using rebind_t = bindings<U>;
};

template <> struct bindings<void> {
  using type = void;
  template <typename U> using rebind_t = bindings<U>;
};

template <typename... Args> struct bindings<static_bindings<Args...>> {
  using type = static_bindings<Args...>;
  template <typename U> using rebind_t = bindings<U>;
};

template <typename T> struct static_bindings_source;

template <typename... Args> struct static_bindings_source<bindings<Args...>> {
  using type = typename bindings<Args...>::type;
};

template <typename... Args>
struct static_bindings_source<static_bindings<Args...>> {
  using type = static_bindings<Args...>;
};

template <typename T>
using static_bindings_source_t = typename static_bindings_source<T>::type;

template <> struct dependencies<void> {
  using type = void;
  template <typename U> using rebind_t = dependencies<U>;
};

template <typename... Args> struct dependencies<type_list<Args...>> {
  using type = type_list<Args...>;
  template <typename U> using rebind_t = dependencies<U>;
};

} // namespace dingo
