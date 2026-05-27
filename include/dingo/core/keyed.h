//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <type_traits>
#include <utility>

namespace dingo {
template <typename T> struct key;

namespace detail {
template <typename T, typename Key, bool IsConstructible = std::is_constructible_v<T>>
struct keyed_base;

template <typename T, typename Key>
struct keyed_base<T, Key, true> {
    keyed_base(const keyed_base&) = default;
    keyed_base(keyed_base&&) = default;
    keyed_base& operator=(const keyed_base&) = default;
    keyed_base& operator=(keyed_base&&) = default;

    explicit keyed_base(T&& value)
        : value_(std::move(value)) {}

    operator T() { return std::move(value_); }

  private:
    T value_;
};

template <typename T, typename Key>
struct keyed_base<T&, Key, false> {
    keyed_base(const keyed_base&) = default;
    keyed_base(keyed_base&&) = default;
    keyed_base& operator=(const keyed_base&) = default;
    keyed_base& operator=(keyed_base&&) = default;

    explicit keyed_base(T& value)
        : value_(value) {}

    operator T&() { return value_; }

  private:
    T& value_;
};

template <typename T, typename Key>
struct keyed_base<T, Key, false> : keyed_base<T&, Key, false> {
    explicit keyed_base(T& value)
        : keyed_base<T&, Key, false>(value) {}
};
} // namespace detail

template <typename T, typename Key>
struct keyed : detail::keyed_base<T, Key> {
    using detail::keyed_base<T, Key>::keyed_base;
};

template <typename T, typename Key>
struct keyed<T&, Key> : detail::keyed_base<T&, Key> {
    using detail::keyed_base<T&, Key>::keyed_base;
};

template <typename T>
struct keyed_traits {
    using type = T;
    using key_type = void;
};

template <typename T, typename Key>
struct keyed_traits<keyed<T, Key>> {
    using type = T;
    using key_type = Key;
};

template <typename T, typename Key>
struct keyed_traits<keyed<T, Key>&> {
    using type = T&;
    using key_type = Key;
};

template <typename T, typename Key>
struct keyed_traits<keyed<T, Key>&&> {
    using type = T&&;
    using key_type = Key;
};

template <typename T, typename Key>
struct keyed_traits<keyed<T, Key>*> {
    using type = T*;
    using key_type = Key;
};

template <typename T>
using keyed_type_t = typename keyed_traits<T>::type;

template <typename T>
using keyed_key_t = typename keyed_traits<T>::key_type;

template <typename T>
struct is_keyed : std::bool_constant<!std::is_void_v<keyed_key_t<T>>> {};

template <typename T>
inline constexpr bool is_keyed_v = is_keyed<T>::value;

} // namespace dingo
