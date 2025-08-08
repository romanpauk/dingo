//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <utility>

namespace dingo {
namespace detail {
template <typename T, typename Tag, bool IsConstructible = std::is_constructible_v<T> > struct annotated_base;

template <typename T, typename Tag> struct annotated_base<T, Tag, true> {
    annotated_base(T&& value): value_(std::move(value)) {}
    operator T&&() { return std::move(value_); }
private:
    T value_;
};

template <typename T, typename Tag> struct annotated_base<T&, Tag, false> {
    annotated_base(T& value): value_(value) {}
    operator T&() { return value_; }
private:
    T& value_;
};

template <typename T, typename Tag> struct annotated_base<T, Tag, false>: annotated_base<T&, Tag, false> {};

}

template <typename T, typename Tag> struct annotated: detail::annotated_base<T, Tag> {
    annotated(T&& value): detail::annotated_base<T, Tag>(std::move(value)) {}
};

template <typename T, typename Tag> struct annotated<T&, Tag>: detail::annotated_base<T&, Tag> {
    annotated(T& value): detail::annotated_base<T&, Tag>(value) {}
};

struct none {};

template <typename T> struct annotated_type {
    using type = annotated<T, none>;
};
template <typename T, typename Tag> struct annotated_type<annotated<T, Tag>> {
    using type = annotated<T, Tag>;
};

template <class T> using annotated_type_t = typename annotated_type<T>::type;

template <typename T> struct annotated_traits {
    using type = T;
};

template <typename T, typename Tag> struct annotated_traits<annotated<T, Tag>> {
    using type = T;
};

template <typename T, typename Tag>
struct annotated_traits<annotated<T, Tag>&> {
    using type = T&;
};

template <typename T, typename Tag>
struct annotated_traits<annotated<T, Tag>&&> {
    using type = T&&;
};

template <typename T, typename Tag>
struct annotated_traits<annotated<T, Tag>*> {
    using type = T*;
};

} // namespace dingo
