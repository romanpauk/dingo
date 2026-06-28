//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/core/selected.h>

#include <utility>

namespace dingo {

template <typename T, typename Tag>
struct annotated : detail::selected<T, detail::type_selector<Tag>> {
    annotated(T&& value)
        : detail::selected<T, detail::type_selector<Tag>>(std::move(value)) {}
};

template <typename T, typename Tag>
struct annotated<T&, Tag> : detail::selected<T&, detail::type_selector<Tag>> {
    annotated(T& value)
        : detail::selected<T&, detail::type_selector<Tag>>(value) {}
};

template <typename T>
struct annotated_traits {
    using type = T;
};

template <typename T, typename Tag>
struct annotated_traits<annotated<T, Tag>> {
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
