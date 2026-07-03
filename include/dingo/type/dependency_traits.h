//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/registration/annotated.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>

namespace dingo {
template <typename Request> struct dependency_interface {
  using type = typename annotated_traits<Request>::type;
};

template <typename T, typename Selector>
struct dependency_interface<detail::selected<T, Selector>> {
  using type = T;
};

template <typename T, typename Selector>
struct dependency_interface<detail::selected<T, Selector> &> {
  using type = T &;
};

template <typename T, typename Selector>
struct dependency_interface<detail::selected<T, Selector> &&> {
  using type = T &&;
};

template <typename T, typename Selector>
struct dependency_interface<detail::selected<T, Selector> *> {
  using type = T *;
};

template <typename Request>
using dependency_interface_t = typename dependency_interface<Request>::type;

template <typename Request>
using dependency_value_t = normalized_type_t<Request>;

template <typename Request>
using dependency_result_t = dependency_interface_t<
    std::conditional_t<std::is_rvalue_reference_v<Request>,
                       std::remove_reference_t<Request>, Request>>;

template <typename Request, bool RemoveRvalueReferences>
using resolve_dependency_t = std::conditional_t<
    RemoveRvalueReferences,
    std::conditional_t<std::is_rvalue_reference_v<Request>,
                       std::remove_reference_t<Request>, Request>,
    Request>;

template <typename Request, bool RemoveRvalueReferences>
using resolve_dependency_result_t = dependency_interface_t<
    resolve_dependency_t<Request, RemoveRvalueReferences>>;

} // namespace dingo
